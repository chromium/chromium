// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_WEBSOCKET_RESOURCE_H_
#define PPAPI_PROXY_WEBSOCKET_RESOURCE_H_

#include <stdint.h>

#include "base/containers/queue.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_websocket_api.h"

namespace ppapi {

class StringVar;
class Var;

namespace proxy {

// This class contains protocol checks which doesn't affect security when it
// run with untrusted code.
class PPAPI_PROXY_EXPORT WebSocketResource : public PluginResource,
                                             public thunk::PPB_WebSocket_API {
 public:
  WebSocketResource(Connection connection, PP_Instance instance);

  WebSocketResource(const WebSocketResource&) = delete;
  WebSocketResource& operator=(const WebSocketResource&) = delete;

  ~WebSocketResource() override;

  // PluginResource implementation.
  thunk::PPB_WebSocket_API* AsPPB_WebSocket_API() override;

  // PPB_WebSocket_API implementation.
  int32_t Connect(const PP_Var& url,
                  const PP_Var protocols[],
                  uint32_t protocol_count,
                  scoped_refptr<TrackedCallback> callback) override;
  int32_t Close(uint16_t code,
                const PP_Var& reason,
                scoped_refptr<TrackedCallback> callback) override;
  int32_t ReceiveMessage(
      PP_Var* message,
      scoped_refptr<TrackedCallback> callback) override;
  int32_t SendMessage(const PP_Var& message) override;
  uint64_t GetBufferedAmount() override;
  uint16_t GetCloseCode() override;
  PP_Var GetCloseReason() override;
  PP_Bool GetCloseWasClean() override;
  PP_Var GetExtensions() override;
  PP_Var GetProtocol() override;
  PP_WebSocketReadyState GetReadyState() override;
  PP_Var GetURL() override;

 private:
  // PluginResource override.
  void OnReplyReceived(const ResourceMessageReplyParams& params,
                       const IPC::Message& msg) override;

  // IPC message handlers.
  void OnPluginMsgConnectReply(const ResourceMessageReplyParams& params,
                               const std::string& url,
                               const std::string& protocol);
  void OnPluginMsgCloseReply(const ResourceMessageReplyParams& params,
                             uint64_t buffered_amount,
                             bool was_clean,
                             uint16_t code,
                             const std::string& reason);
  void OnPluginMsgReceiveTextReply(const ResourceMessageReplyParams& params,
                                   const std::string& message);
  void OnPluginMsgReceiveBinaryReply(const ResourceMessageReplyParams& params,
                                     const std::vector<uint8_t>& message);
  void OnPluginMsgErrorReply(const ResourceMessageReplyParams& params);
  void OnPluginMsgBufferedAmountReply(const ResourceMessageReplyParams& params,
                                      uint64_t buffered_amount);
  void OnPluginMsgStateReply(const ResourceMessageReplyParams& params,
                             int32_t state);
  void OnPluginMsgClosedReply(const ResourceMessageReplyParams& params,
                              uint64_t buffered_amount,
                              bool was_clean,
                              uint16_t code,
                              const std::string& reason);

  // Picks up a received message and moves it to user receiving buffer. This
  // function is used in both ReceiveMessage for fast returning path, and
  // OnPluginMsgReceiveTextReply and OnPluginMsgReceiveBinaryReply for delayed
  // callback invocations.
  int32_t DoReceive();

  // Holds user callbacks to invoke later.
  scoped_refptr<TrackedCallback> connect_callback_;
  scoped_refptr<TrackedCallback> close_callback_;
  scoped_refptr<TrackedCallback> receive_callback_;

  // Represents readyState described in the WebSocket API specification. It can
  // be read via GetReadyState().
  PP_WebSocketReadyState state_;

  // Becomes true if any error is detected. Incoming data will be disposed
  // if this variable is true, then ReceiveMessage() returns PP_ERROR_FAILED
  // after returning all received data.
  bool error_was_received_;

  // Keeps a pointer to PP_Var which is provided via ReceiveMessage().
  // Received data will be copied to this PP_Var on ready.
  PP_Var* receive_callback_var_;

  // Keeps received data until ReceiveMessage() requests.
  base::queue<scoped_refptr<Var>> received_messages_;

  // Keeps empty string for functions to return empty string.
  scoped_refptr<StringVar> empty_string_;

  // Keeps the status code field of closing handshake. It can be read via
  // GetCloseCode().
  uint16_t close_code_;

  // Keeps the reason field of closing handshake. It can be read via
  // GetCloseReason().
  scoped_refptr<StringVar> close_reason_;

  // Becomes true when closing handshake is performed successfully. It can be
  // read via GetCloseWasClean().
  PP_Bool close_was_clean_;

  // Represents extensions described in the WebSocket API specification. It can
  // be read via GetExtensions().
  scoped_refptr<StringVar> extensions_;

  // Represents protocol described in the WebSocket API specification. It can be
  // read via GetProtocol().
  scoped_refptr<StringVar> protocol_;

  // Represents url described in the WebSocket API specification. It can be
  // read via GetURL().
  scoped_refptr<StringVar> url_;

  // Keeps the number of bytes of application data that have been queued using
  // SendMessage(). WebKit side implementation calculates the actual amount.
  // This is a cached value which is notified through a WebKit callback.
  // This value is used to calculate bufferedAmount in the WebSocket API
  // specification. The calculated value can be read via GetBufferedAmount().
  uint64_t buffered_amount_;

  // Keeps the number of bytes of application data that have been ignored
  // because the connection was already closed.
  // This value is used to calculate bufferedAmount in the WebSocket API
  // specification. The calculated value can be read via GetBufferedAmount().
  uint64_t buffered_amount_after_close_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_WEBSOCKET_RESOURCE_H_
