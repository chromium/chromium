// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_WEBSOCKET_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_WEBSOCKET_HOST_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/memory/raw_ptr.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"
#include "third_party/blink/public/web/web_pepper_socket.h"
#include "third_party/blink/public/web/web_pepper_socket_client.h"

namespace content {

class RendererPpapiHost;

class PepperWebSocketHost : public ppapi::host::ResourceHost,
                            public ::blink::WebPepperSocketClient {
 public:
  explicit PepperWebSocketHost(RendererPpapiHost* host,
                               PP_Instance instance,
                               PP_Resource resource);

  PepperWebSocketHost(const PepperWebSocketHost&) = delete;
  PepperWebSocketHost& operator=(const PepperWebSocketHost&) = delete;

  ~PepperWebSocketHost() override;

  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // WebPepperSocketClient implementation.
  void DidConnect() override;
  void DidReceiveMessage(const blink::WebString& message) override;
  void DidReceiveArrayBuffer(const blink::WebArrayBuffer& binaryData) override;
  void DidReceiveMessageError() override;
  void DidUpdateBufferedAmount(uint64_t buffered_amount) override;
  void DidStartClosingHandshake() override;
  void DidClose(uint64_t unhandled_buffered_amount,
                ClosingHandshakeCompletionStatus status,
                uint16_t code,
                const blink::WebString& reason) override;

 private:
  // IPC message handlers.
  int32_t OnHostMsgConnect(ppapi::host::HostMessageContext* context,
                           const std::string& url,
                           const std::vector<std::string>& protocols);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context,
                         int32_t code,
                         const std::string& reason);
  int32_t OnHostMsgSendText(ppapi::host::HostMessageContext* context,
                            const std::string& message);
  int32_t OnHostMsgSendBinary(ppapi::host::HostMessageContext* context,
                              const std::vector<uint8_t>& message);
  int32_t OnHostMsgFail(ppapi::host::HostMessageContext* context,
                        const std::string& message);

  // Non-owning pointer.
  raw_ptr<RendererPpapiHost> renderer_ppapi_host_;

  // IPC reply parameters.
  ppapi::host::ReplyMessageContext connect_reply_;
  ppapi::host::ReplyMessageContext close_reply_;

  // The server URL to which this instance connects.
  std::string url_;

  // A flag to indicate if opening handshake is going on.
  bool connecting_;

  // A flag to indicate if client initiated closing handshake is performed.
  bool initiating_close_;

  // A flag to indicate if server initiated closing handshake is performed.
  bool accepting_close_;

  // Becomes true if any error is detected. Incoming data will be disposed
  // if this variable is true.
  bool error_was_received_;

  // Keeps the WebKit side WebSocket object. This is used for calling WebKit
  // side functions via WebKit API.
  std::unique_ptr<blink::WebPepperSocket> websocket_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_WEBSOCKET_HOST_H_
