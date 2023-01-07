// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>

#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/proxy/locking_resource_releaser.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/websocket_resource.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_var_shared.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

typedef PluginProxyTest WebSocketResourceTest;

bool g_callback_called;
int32_t g_callback_result;
const PPB_Var* ppb_var_ = NULL;

void Callback(void* user_data, int32_t result) {
  g_callback_called = true;
  g_callback_result = result;
}

PP_CompletionCallback MakeCallback() {
  g_callback_called = false;
  g_callback_result = PP_OK;
  return PP_MakeCompletionCallback(Callback, NULL);
}

PP_Var MakeStringVar(const std::string& string) {
  if (!ppb_var_)
    ppb_var_ = ppapi::PPB_Var_Shared::GetVarInterface1_2();
  return ppb_var_->VarFromUtf8(string.c_str(),
                               static_cast<uint32_t>(string.length()));
}

}  // namespace


// Does a test of Connect().
TEST_F(WebSocketResourceTest, Connect) {
  const PPB_WebSocket_1_0* websocket_iface =
      thunk::GetPPB_WebSocket_1_0_Thunk();

  std::string url("ws://ws.google.com");
  std::string protocol0("x-foo");
  std::string protocol1("x-bar");
  PP_Var url_var = MakeStringVar(url);
  PP_Var protocols[] = { MakeStringVar(protocol0), MakeStringVar(protocol1) };

  LockingResourceReleaser res(websocket_iface->Create(pp_instance()));

  int32_t result = websocket_iface->Connect(res.get(), url_var, protocols, 2,
                                            MakeCallback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  // Should be sent a "Connect" message.
  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_WebSocket_Connect::ID, &params, &msg));
  PpapiHostMsg_WebSocket_Connect::Schema::Param p;
  PpapiHostMsg_WebSocket_Connect::Read(&msg, &p);
  EXPECT_EQ(url, std::get<0>(p));
  EXPECT_EQ(protocol0, std::get<1>(p)[0]);
  EXPECT_EQ(protocol1, std::get<1>(p)[1]);

  // Synthesize a response.
  ResourceMessageReplyParams reply_params(params.pp_resource(),
                                          params.sequence());
  reply_params.set_result(PP_OK);
  PluginMessageFilter::DispatchResourceReplyForTest(
      reply_params, PpapiPluginMsg_WebSocket_ConnectReply(url, protocol1));

  EXPECT_EQ(PP_OK, g_callback_result);
  EXPECT_EQ(true, g_callback_called);
}

// Does a test for unsolicited replies.
TEST_F(WebSocketResourceTest, UnsolicitedReplies) {
  const PPB_WebSocket_1_0* websocket_iface =
      thunk::GetPPB_WebSocket_1_0_Thunk();

  LockingResourceReleaser res(websocket_iface->Create(pp_instance()));

  // Check if BufferedAmountReply is handled.
  ResourceMessageReplyParams reply_params(res.get(), 0);
  reply_params.set_result(PP_OK);
  PluginMessageFilter::DispatchResourceReplyForTest(
      reply_params, PpapiPluginMsg_WebSocket_BufferedAmountReply(19760227u));

  uint64_t amount = websocket_iface->GetBufferedAmount(res.get());
  EXPECT_EQ(19760227u, amount);

  // Check if StateReply is handled.
  PluginMessageFilter::DispatchResourceReplyForTest(
      reply_params,
      PpapiPluginMsg_WebSocket_StateReply(
          static_cast<int32_t>(PP_WEBSOCKETREADYSTATE_CLOSING)));

  PP_WebSocketReadyState state = websocket_iface->GetReadyState(res.get());
  EXPECT_EQ(PP_WEBSOCKETREADYSTATE_CLOSING, state);
}

TEST_F(WebSocketResourceTest, MessageError) {
  const PPB_WebSocket_1_0* websocket_iface =
      thunk::GetPPB_WebSocket_1_0_Thunk();

  std::string url("ws://ws.google.com");
  PP_Var url_var = MakeStringVar(url);

  LockingResourceReleaser res(websocket_iface->Create(pp_instance()));

  // Establish the connection virtually.
  int32_t result =
      websocket_iface->Connect(res.get(), url_var, NULL, 0, MakeCallback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_WebSocket_Connect::ID, &params, &msg));

  ResourceMessageReplyParams connect_reply_params(params.pp_resource(),
                                                  params.sequence());
  connect_reply_params.set_result(PP_OK);
  PluginMessageFilter::DispatchResourceReplyForTest(
      connect_reply_params,
      PpapiPluginMsg_WebSocket_ConnectReply(url, std::string()));

  EXPECT_EQ(PP_OK, g_callback_result);
  EXPECT_TRUE(g_callback_called);

  PP_Var message;
  result = websocket_iface->ReceiveMessage(res.get(), &message, MakeCallback());
  EXPECT_FALSE(g_callback_called);

  // Synthesize a WebSocket_ErrorReply message.
  ResourceMessageReplyParams error_reply_params(res.get(), 0);
  error_reply_params.set_result(PP_OK);
  PluginMessageFilter::DispatchResourceReplyForTest(
      error_reply_params, PpapiPluginMsg_WebSocket_ErrorReply());

  EXPECT_EQ(PP_ERROR_FAILED, g_callback_result);
  EXPECT_TRUE(g_callback_called);
}

}  // namespace proxy
}  // namespace ppapi
