#include "asyncrpcoperation_common.h"

#include "core_io.h"
#include "init.h"
#include "rpc/protocol.h"

extern UniValue signrawtransaction(const UniValue& params, bool fHelp);

// TODO: instead of passing a TestMode flag, tests should override `CommitTransaction`
// on the wallet.
UniValue SendTransaction(const CTransaction& tx, const std::vector<SendManyRecipient>& recipients, std::optional<std::reference_wrapper<CReserveKey>> reservekey, bool testmode) {
    UniValue o(UniValue::VOBJ);
    // Send the transaction
    if (!testmode) {
        CWalletTx wtx(pwalletMain, tx);
        // save the mapping from (receiver, txid) to UA
        if (!pwalletMain->SaveRecipientMappings(tx.GetHash(), recipients)) {
            // More details in debug.log
            throw JSONRPCError(RPC_WALLET_ERROR, "SendTransaction: SaveRecipientMappings failed");
        }
        if (!pwalletMain->CommitTransaction(wtx, reservekey)) {
            // More details in debug.log
            throw JSONRPCError(RPC_WALLET_ERROR, "SendTransaction: CommitTransaction failed");
        }
        o.pushKV("txid", tx.GetHash().ToString());
    } else {
        // Test mode does not send the transaction to the network.
        o.pushKV("test", 1);
        o.pushKV("txid", tx.GetHash().ToString());
        o.pushKV("hex", EncodeHexTx(tx));
    }
    return o;
}

std::pair<CTransaction, UniValue> SignSendRawTransaction(UniValue obj, std::optional<std::reference_wrapper<CReserveKey>> reservekey, bool testmode) {
    // Sign the raw transaction
    UniValue rawtxnValue = find_value(obj, "rawtxn");
    if (rawtxnValue.isNull()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing hex data for raw transaction");
    }
    std::string rawtxn = rawtxnValue.get_str();

    UniValue params = UniValue(UniValue::VARR);
    params.push_back(rawtxn);
    UniValue signResultValue = signrawtransaction(params, false);
    UniValue signResultObject = signResultValue.get_obj();
    UniValue completeValue = find_value(signResultObject, "complete");
    bool complete = completeValue.get_bool();
    if (!complete) {
        // TODO: #1366 Maybe get "errors" and print array vErrors into a string
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Failed to sign transaction");
    }

    UniValue hexValue = find_value(signResultObject, "hex");
    if (hexValue.isNull()) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Missing hex data for signed transaction");
    }
    std::string signedtxn = hexValue.get_str();
    CDataStream stream(ParseHex(signedtxn), SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    stream >> tx;

    UniValue sendResult = SendTransaction(tx, {}, reservekey, testmode);

    return std::make_pair(tx, sendResult);
}
