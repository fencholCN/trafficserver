/** @file
 @section license License

 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

#include <arpa/inet.h>
#include <ts.h>
#include "ts-cpp11.h"

using namespace ats::api;

namespace {
TSHttpHookID TSHookIDFromHookType(HookType hook);

class HookContinuationData {
public:
  GlobalHookCallback callback;
  HookType hooktype;
  TSHttpHookID ts_hook_id;
};

class ats::api::Transaction {
public:
  TSHttpTxn ts_http_txn_ = NULL;
  TSCont ts_contp_ = NULL;
};
}

extern "C" void TSPluginInit(int argc, const char *argv[]) {

  TSPluginRegistrationInfo registration_info;

  const char *api_version_string = "cpp11api";

  registration_info.plugin_name = const_cast<char*>(api_version_string);
  registration_info.vendor_name = const_cast<char*>(api_version_string);
  registration_info.support_email = const_cast<char*>(api_version_string);

  if (TSPluginRegister(TS_SDK_VERSION_3_0, &registration_info) != TS_SUCCESS) {
    return;
  }

  StringVector arguments;
  for (int i = 0; i < argc; ++i) {
    arguments.push_back(std::string(argv[i]));
  }

  // Finally we will call the wrapper API registration point
  PluginRegister(arguments);
}

namespace {
TSHttpHookID TSHookIDFromHookType(HookType hook) {
  switch (hook) {
  case ats::api::HookType::HOOK_PRE_REMAP:
    return TS_HTTP_PRE_REMAP_HOOK;
    break;
  case ats::api::HookType::HOOK_POST_REMAP:
    return TS_HTTP_POST_REMAP_HOOK;
    break;
  case ats::api::HookType::HOOK_READ_REQUEST_HEADER:
    return TS_HTTP_READ_REQUEST_HDR_HOOK;
    break;
  case ats::api::HookType::HOOK_READ_RESPONSE_HEADER:
    return TS_HTTP_READ_RESPONSE_HDR_HOOK;
    break;
  case ats::api::HookType::HOOK_SEND_RESPONSE_HEADER:
    return TS_HTTP_SEND_RESPONSE_HDR_HOOK;
    break;
  }

  return TS_HTTP_READ_REQUEST_HDR_HOOK;
}

void inline ReenableBasedOnNextState(TSHttpTxn txnp, NextState ns) {
  switch (ns) {
  case NextState::HTTP_DONT_CONTINUE:
    return;
    break;
  case NextState::HTTP_ERROR:
    TSHttpTxnReenable(txnp, static_cast<TSEvent>(TS_EVENT_HTTP_ERROR));
    break;
  case NextState::HTTP_CONTINUE:
  default:
    TSHttpTxnReenable(txnp, static_cast<TSEvent>(TS_EVENT_HTTP_CONTINUE));
    break;
  }
}

std::string printable_sockaddr_ip(sockaddr const * s_sockaddr) {
  const struct sockaddr_in * s_sockaddr_in =
      reinterpret_cast<const struct sockaddr_in *>(s_sockaddr);

  char res[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &s_sockaddr_in->sin_addr, res, INET_ADDRSTRLEN);

  return std::string(res);
}
} /* end anonymous namespace */

std::string ats::api::GetPristineRequestUrl(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc url_loc;

  if (TSHttpTxnPristineUrlGet(t.ts_http_txn_, &bufp, &url_loc) != TS_SUCCESS)
    return std::string();

  int url_len;
  char *urlp = TSUrlStringGet(bufp, url_loc, &url_len);
  std::string url(urlp, url_len);

  TSfree(urlp);

  TSHandleMLocRelease(bufp, TS_NULL_MLOC, url_loc);

  return url;
}

std::string ats::api::GetRequestUrl(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  if (TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc) != TS_SUCCESS)
    return std::string();

  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);

  int url_len;
  char *urlp = TSUrlStringGet(bufp, url_loc, &url_len);
  std::string url(urlp, url_len);

  TSfree(urlp);

  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

  return url;
}

std::string ats::api::GetRequestUrlScheme(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  if (TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc) != TS_SUCCESS)
    return std::string();

  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);

  int scheme_length;
  const char *scheme = TSUrlSchemeGet(bufp, url_loc, &scheme_length);

  std::string ret(scheme, scheme_length);

  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

  return ret;
}


std::string ats::api::GetPristineRequestUrlScheme(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc url_loc;

  TSHttpTxnPristineUrlGet(t.ts_http_txn_, &bufp, &url_loc);

  int length;
  const char *scheme = TSUrlHostGet(bufp, url_loc, &length);

  std::string ret(scheme, length);

  TSHandleMLocRelease(bufp, TS_NULL_MLOC, url_loc);

  return ret;
}

void ats::api::SetRequestUrlScheme(Transaction &t, const std::string &scheme) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc);
  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);

  TSUrlHostSet(bufp, url_loc, scheme.c_str(), scheme.length());

  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
}


std::string ats::api::GetRequestUrlQuery(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  if (TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc) != TS_SUCCESS)
    return std::string();

  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);

  int query_length;
  const char *query = TSUrlHttpQueryGet(bufp, url_loc, &query_length);

  std::string ret(query, query_length);

  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

  return ret;
}

std::string ats::api::GetPristineRequestUrlQuery(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc url_loc;

  TSHttpTxnPristineUrlGet(t.ts_http_txn_, &bufp, &url_loc);

  int query_length;
  const char *query = TSUrlHostGet(bufp, url_loc, &query_length);

  std::string ret(query, query_length);

  TSHandleMLocRelease(bufp, TS_NULL_MLOC, url_loc);

  return ret;
}

void ats::api::SetRequestUrlQuery(Transaction &t, const std::string &query) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc);
  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);

  TSUrlHostSet(bufp, url_loc, query.c_str(), query.length());

  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
}

std::string ats::api::GetRequestUrlHost(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  if (TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc) != TS_SUCCESS)
    return std::string();

  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);

  int host_length;
  const char *host = TSUrlHostGet(bufp, url_loc, &host_length);

  std::string ret(host, host_length);

  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

  return ret;
}

std::string ats::api::GetPristineRequestUrlHost(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc url_loc;

  TSHttpTxnPristineUrlGet(t.ts_http_txn_, &bufp, &url_loc);

  int host_length;
  const char *host = TSUrlHostGet(bufp, url_loc, &host_length);

  std::string ret(host, host_length);

  TSHandleMLocRelease(bufp, TS_NULL_MLOC, url_loc);

  return ret;
}

void ats::api::SetRequestUrlHost(Transaction &t, const std::string &host) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc);
  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);

  TSUrlHostSet(bufp, url_loc, host.c_str(), host.length());


  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
}

std::string ats::api::GetRequestUrlPath(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  if (TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc) != TS_SUCCESS)
    return std::string();

  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);

  int path_length;
  const char *path = TSUrlPathGet(bufp, url_loc, &path_length);

  std::string ret(path, path_length);

  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

  return ret;
}

unsigned int ats::api::GetRequestUrlPort(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc);
  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);
  int port = TSUrlPortGet(bufp, url_loc);

  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

  return port;
}

unsigned int ats::api::GetPristineRequestUrlPort(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc url_loc;

  TSHttpTxnPristineUrlGet(t.ts_http_txn_, &bufp, &url_loc);
  int port = TSUrlPortGet(bufp, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, url_loc); // TODO: is this wrong?

  return port;
}

void ats::api::SetRequestUrlPort(Transaction &t, unsigned int port) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc);
  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);

  TSUrlPortSet(bufp, url_loc, port);

  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
}

std::string ats::api::GetPristineRequestUrlPath(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc url_loc;

  TSHttpTxnPristineUrlGet(t.ts_http_txn_, &bufp, &url_loc);

  int path_length;
  const char *path = TSUrlPathGet(bufp, url_loc, &path_length);

  std::string ret(path, path_length);

  TSHandleMLocRelease(bufp, TS_NULL_MLOC, url_loc);

  return ret;
}

void ats::api::SetRequestUrlPath(Transaction &t, const std::string &path) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;
  TSMLoc url_loc;

  TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc);
  TSHttpHdrUrlGet(bufp, hdr_loc, &url_loc);

  TSUrlPathSet(bufp, url_loc, path.c_str(), path.length());

  TSHandleMLocRelease(bufp, hdr_loc, url_loc);
  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
}

static int GlobalContinuationHandler(TSCont contp, TSEvent event, void *edata) {
  TSHttpTxn txnp = static_cast<TSHttpTxn>(edata);

  Transaction transaction;
  transaction.ts_http_txn_ = txnp;
  transaction.ts_contp_ = contp;

  HookContinuationData *data = static_cast<HookContinuationData*>(TSContDataGet(
      contp));

  ats::api::NextState ns = data->callback(transaction);
  ReenableBasedOnNextState(txnp, ns);

  return 0;
}

std::string ats::api::GetClientIP(Transaction &t) {
  return printable_sockaddr_ip(TSHttpTxnClientAddrGet(t.ts_http_txn_));
}

unsigned int ats::api::GetClientPort(Transaction &t) {
  const struct sockaddr_in * client_sockaddr =
      reinterpret_cast<const struct sockaddr_in *>(TSHttpTxnClientAddrGet(
          t.ts_http_txn_));

  return static_cast<unsigned int>(ntohs(client_sockaddr->sin_port));
}

std::string ats::api::GetServerIncomingIP(Transaction &t) {
  return printable_sockaddr_ip(TSHttpTxnIncomingAddrGet(t.ts_http_txn_));
}

unsigned int ats::api::GetServerIncomingPort(Transaction &t) {
  const struct sockaddr_in * client_sockaddr =
      reinterpret_cast<const struct sockaddr_in *>(TSHttpTxnIncomingAddrGet(
          t.ts_http_txn_));

  return static_cast<unsigned int>(ntohs(client_sockaddr->sin_port));
}

bool ats::api::IsInternalRequest(Transaction &t) {
  return TSHttpIsInternalRequest(t.ts_http_txn_) == TS_SUCCESS;
}

int ats::api::GetServerResponseStatusCode(Transaction &t) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;

  TSHttpTxnServerRespGet(t.ts_http_txn_, &bufp, &hdr_loc);
  int status = TSHttpHdrStatusGet(bufp, hdr_loc);

  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

  return status;
}

std::string ats::api::GetRequestMethod(Transaction &t) {

  TSMBuffer bufp;
  TSMLoc hdr_loc;

  TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc);

  int method_len;
  const char *methodp = TSHttpHdrMethodGet(bufp, hdr_loc, &method_len);
  std::string method(methodp, method_len);

  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);

  return method;
}

void ats::api::SetRequestMethod(Transaction &t, const std::string &method) {
  TSMBuffer bufp;
  TSMLoc hdr_loc;

  TSHttpTxnClientReqGet(t.ts_http_txn_, &bufp, &hdr_loc);

  TSHttpHdrMethodSet(bufp, hdr_loc, method.c_str(), method.length());

  TSHandleMLocRelease(bufp, TS_NULL_MLOC, hdr_loc);
}

static int TransactionContinuationHandler(TSCont contp, TSEvent event,
    void *edata) {
  TSHttpTxn txnp = static_cast<TSHttpTxn>(edata);

  Transaction transaction;
  transaction.ts_http_txn_ = txnp;
  transaction.ts_contp_ = contp;

  NextState ns = NextState::HTTP_CONTINUE;
  HookContinuationData *data = static_cast<HookContinuationData*>(TSContDataGet(
      contp));
  if (event != TS_EVENT_HTTP_TXN_CLOSE
      || (event == TS_EVENT_HTTP_TXN_CLOSE
          && data->ts_hook_id == TS_HTTP_TXN_CLOSE_HOOK)) {
    ns = data->callback(transaction);
  }

  // We must free the HookContinuationData structure and continuation
  // If this transaction is complete
  if (event == TS_EVENT_HTTP_TXN_CLOSE) {
    delete data;
    TSContDestroy(contp);
  }

  ReenableBasedOnNextState(txnp, ns);
  return 0;
}

void ats::api::CreateTransactionHook(Transaction &txn, HookType hook,
    GlobalHookCallback callback) {
  TSHttpHookID ts_hook_id = TSHookIDFromHookType(hook);
  TSCont contp = TSContCreate(TransactionContinuationHandler, NULL);

  HookContinuationData *data = new HookContinuationData();
  data->callback = callback;
  data->hooktype = hook;
  data->ts_hook_id = ts_hook_id;

  TSContDataSet(contp, static_cast<void*>(data));
  TSHttpTxnHookAdd(txn.ts_http_txn_, ts_hook_id, contp);

  if (ts_hook_id != TS_HTTP_TXN_CLOSE_HOOK) {
    TSHttpTxnHookAdd(txn.ts_http_txn_, TS_HTTP_TXN_CLOSE_HOOK, contp);
  }
}

void ats::api::CreateGlobalHook(HookType hook, GlobalHookCallback callback) {

  TSHttpHookID ts_hook_id = TSHookIDFromHookType(hook);
  TSCont contp = TSContCreate(GlobalContinuationHandler, NULL);

  HookContinuationData *data = new HookContinuationData();
  data->callback = callback;
  data->hooktype = hook;

  TSContDataSet(contp, static_cast<void*>(data));
  TSHttpHookAdd(ts_hook_id, contp);
}
