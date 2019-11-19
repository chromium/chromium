// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_websocket_host.h"

#include <string>

#include "content/public/renderer/renderer_ppapi_host.h"
#include "net/base/port_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_array_buffer.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_plugin_container.h"

using blink::WebArrayBuffer;
using blink::WebDocument;
using blink::WebString;
using blink::WebPepperSocket;
using blink::WebURL;

namespace content {

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, np_name)        \
  static_assert(static_cast<int>(WebPepperSocket::webkit_name) == \
                    static_cast<int>(np_name),                    \
                "WebSocket enums must match PPAPI's")

COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeNormalClosure,
                             PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeGoingAway,
                             PP_WEBSOCKETSTATUSCODE_GOING_AWAY);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeProtocolError,
                             PP_WEBSOCKETSTATUSCODE_PROTOCOL_ERROR);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeUnsupportedData,
                             PP_WEBSOCKETSTATUSCODE_UNSUPPORTED_DATA);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeNoStatusRcvd,
                             PP_WEBSOCKETSTATUSCODE_NO_STATUS_RECEIVED);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeAbnormalClosure,
                             PP_WEBSOCKETSTATUSCODE_ABNORMAL_CLOSURE);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeInvalidFramePayloadData,
                             PP_WEBSOCKETSTATUSCODE_INVALID_FRAME_PAYLOAD_DATA);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodePolicyViolation,
                             PP_WEBSOCKETSTATUSCODE_POLICY_VIOLATION);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeMessageTooBig,
                             PP_WEBSOCKETSTATUSCODE_MESSAGE_TOO_BIG);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeMandatoryExt,
                             PP_WEBSOCKETSTATUSCODE_MANDATORY_EXTENSION);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeInternalError,
                             PP_WEBSOCKETSTATUSCODE_INTERNAL_SERVER_ERROR);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeTLSHandshake,
                             PP_WEBSOCKETSTATUSCODE_TLS_HANDSHAKE);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeMinimumUserDefined,
                             PP_WEBSOCKETSTATUSCODE_USER_REGISTERED_MIN);
COMPILE_ASSERT_MATCHING_ENUM(kCloseEventCodeMaximumUserDefined,
                             PP_WEBSOCKETSTATUSCODE_USER_PRIVATE_MAX);

PepperWebSocketHost::PepperWebSocketHost(RendererPpapiHost* host,
                                         PP_Instance instance,
                                         PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource),
      renderer_ppapi_host_(host),
      connecting_(false),
      initiating_close_(false),
      accepting_close_(false),
      error_was_received_(false) {}

PepperWebSocketHost::~PepperWebSocketHost() {
  if (websocket_)
    websocket_->Disconnect();
}

int32_t PepperWebSocketHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  PPAPI_BEGIN_MESSAGE_MAP(PepperWebSocketHost, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_WebSocket_Connect,
                                      OnHostMsgConnect)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_WebSocket_Close,
                                      OnHostMsgClose)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_WebSocket_SendText,
                                      OnHostMsgSendText)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_WebSocket_SendBinary,
                                      OnHostMsgSendBinary)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL(PpapiHostMsg_WebSocket_Fail,
                                      OnHostMsgFail)
  PPAPI_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

void PepperWebSocketHost::DidConnect() {
  std::string protocol;
  if (websocket_)
    protocol = websocket_->Subprotocol().Utf8();
  connecting_ = false;
  connect_reply_.params.set_result(PP_OK);
  host()->SendReply(connect_reply_,
                    PpapiPluginMsg_WebSocket_ConnectReply(url_, protocol));
}

void PepperWebSocketHost::DidReceiveMessage(const blink::WebString& message) {
  // Dispose packets after receiving an error.
  if (error_was_received_)
    return;

  // Send an IPC to transport received data.
  std::string string_message = message.Utf8();
  host()->SendUnsolicitedReply(
      pp_resource(), PpapiPluginMsg_WebSocket_ReceiveTextReply(string_message));
}

void PepperWebSocketHost::DidReceiveArrayBuffer(
    const blink::WebArrayBuffer& binaryData) {
  // Dispose packets after receiving an error.
  if (error_was_received_)
    return;

  // Send an IPC to transport received data.
  uint8_t* data = static_cast<uint8_t*>(binaryData.Data());
  std::vector<uint8_t> array_message(data, data + binaryData.ByteLength());
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_WebSocket_ReceiveBinaryReply(array_message));
}

void PepperWebSocketHost::DidReceiveMessageError() {
  // Records the error, then stops receiving any frames after this error.
  // The error must be notified after all queued messages are read.
  error_was_received_ = true;

  // Send an IPC to report the error. After this IPC, ReceiveTextReply and
  // ReceiveBinaryReply IPC are not sent anymore because |error_was_received_|
  // blocks.
  host()->SendUnsolicitedReply(pp_resource(),
                               PpapiPluginMsg_WebSocket_ErrorReply());
}

void PepperWebSocketHost::DidUpdateBufferedAmount(uint64_t buffered_amount) {
  // Send an IPC to update buffered amount.
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_WebSocket_BufferedAmountReply(buffered_amount));
}

void PepperWebSocketHost::DidStartClosingHandshake() {
  accepting_close_ = true;

  // Send an IPC to notice that server starts closing handshake.
  host()->SendUnsolicitedReply(
      pp_resource(),
      PpapiPluginMsg_WebSocket_StateReply(PP_WEBSOCKETREADYSTATE_CLOSING));
}

void PepperWebSocketHost::DidClose(uint64_t unhandled_buffered_amount,
                                   ClosingHandshakeCompletionStatus status,
                                   uint16_t code,
                                   const blink::WebString& reason) {
  if (connecting_) {
    connecting_ = false;
    connect_reply_.params.set_result(PP_ERROR_FAILED);
    host()->SendReply(
        connect_reply_,
        PpapiPluginMsg_WebSocket_ConnectReply(url_, std::string()));
  }

  // Set close_was_clean_.
  bool was_clean = (initiating_close_ || accepting_close_) &&
                   !unhandled_buffered_amount &&
                   status == WebPepperSocketClient::kClosingHandshakeComplete;

  if (initiating_close_) {
    initiating_close_ = false;
    close_reply_.params.set_result(PP_OK);
    host()->SendReply(close_reply_, PpapiPluginMsg_WebSocket_CloseReply(
                                        unhandled_buffered_amount, was_clean,
                                        code, reason.Utf8()));
  } else {
    accepting_close_ = false;
    host()->SendUnsolicitedReply(
        pp_resource(),
        PpapiPluginMsg_WebSocket_ClosedReply(unhandled_buffered_amount,
                                             was_clean, code, reason.Utf8()));
  }

  // Disconnect.
  if (websocket_) {
    websocket_->Disconnect();
    websocket_.reset();
  }
}

int32_t PepperWebSocketHost::OnHostMsgConnect(
    ppapi::host::HostMessageContext* context,
    const std::string& url,
    const std::vector<std::string>& protocols) {
  // Validate url and convert it to WebURL.
  GURL gurl(url);
  url_ = gurl.spec();
  if (!gurl.is_valid())
    return PP_ERROR_BADARGUMENT;
  if (!gurl.SchemeIs("ws") && !gurl.SchemeIs("wss"))
    return PP_ERROR_BADARGUMENT;
  if (gurl.has_ref())
    return PP_ERROR_BADARGUMENT;
  if (!net::IsPortAllowedForScheme(gurl.EffectiveIntPort(),
                                   gurl.scheme_piece()))
    return PP_ERROR_BADARGUMENT;
  WebURL web_url(gurl);

  // Validate protocols.
  std::string protocol_string;
  for (auto vector_it = protocols.begin(); vector_it != protocols.end();
       ++vector_it) {
    // Check containing characters.
    for (std::string::const_iterator string_it = vector_it->begin();
         string_it != vector_it->end();
         ++string_it) {
      uint8_t character = *string_it;
      // WebSocket specification says "(Subprotocol string must consist of)
      // characters in the range U+0021 to U+007E not including separator
      // characters as defined in [RFC2616]."
      const uint8_t minimumProtocolCharacter = '!';  // U+0021.
      const uint8_t maximumProtocolCharacter = '~';  // U+007E.
      if (character < minimumProtocolCharacter ||
          character > maximumProtocolCharacter || character == '"' ||
          character == '(' || character == ')' || character == ',' ||
          character == '/' ||
          (character >= ':' && character <= '@') ||  // U+003A - U+0040
          (character >= '[' && character <= ']') ||  // U+005B - u+005D
          character == '{' ||
          character == '}')
        return PP_ERROR_BADARGUMENT;
    }
    // Join protocols with the comma separator.
    if (vector_it != protocols.begin())
      protocol_string.append(",");
    protocol_string.append(*vector_it);
  }

  // Convert protocols to WebString.
  WebString web_protocols = WebString::FromUTF8(protocol_string);

  // Create blink::WebSocket object and connect.
  blink::WebPluginContainer* container =
      renderer_ppapi_host_->GetContainerForInstance(pp_instance());
  if (!container)
    return PP_ERROR_BADARGUMENT;
  websocket_ = WebPepperSocket::Create(container->GetDocument(), this);
  DCHECK(websocket_.get());
  if (!websocket_)
    return PP_ERROR_NOTSUPPORTED;

  websocket_->Connect(web_url, web_protocols);

  connect_reply_ = context->MakeReplyMessageContext();
  connecting_ = true;
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperWebSocketHost::OnHostMsgClose(
    ppapi::host::HostMessageContext* context,
    int32_t code,
    const std::string& reason) {
  if (!websocket_)
    return PP_ERROR_FAILED;
  close_reply_ = context->MakeReplyMessageContext();
  initiating_close_ = true;

  blink::WebPepperSocket::CloseEventCode event_code =
      static_cast<blink::WebPepperSocket::CloseEventCode>(code);
  if (code == PP_WEBSOCKETSTATUSCODE_NOT_SPECIFIED) {
    // PP_WEBSOCKETSTATUSCODE_NOT_SPECIFIED and CloseEventCodeNotSpecified are
    // assigned to different values. A conversion is needed if
    // PP_WEBSOCKETSTATUSCODE_NOT_SPECIFIED is specified.
    event_code = blink::WebPepperSocket::kCloseEventCodeNotSpecified;
  }

  WebString web_reason = WebString::FromUTF8(reason);
  websocket_->Close(event_code, web_reason);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PepperWebSocketHost::OnHostMsgSendText(
    ppapi::host::HostMessageContext* context,
    const std::string& message) {
  if (websocket_) {
    WebString web_message = WebString::FromUTF8(message);
    websocket_->SendText(web_message);
  }
  return PP_OK;
}

int32_t PepperWebSocketHost::OnHostMsgSendBinary(
    ppapi::host::HostMessageContext* context,
    const std::vector<uint8_t>& message) {
  if (websocket_.get() && !message.empty()) {
    WebArrayBuffer web_message = WebArrayBuffer::Create(message.size(), 1);
    memcpy(web_message.Data(), &message.front(), message.size());
    websocket_->SendArrayBuffer(web_message);
  }
  return PP_OK;
}

int32_t PepperWebSocketHost::OnHostMsgFail(
    ppapi::host::HostMessageContext* context,
    const std::string& message) {
  if (websocket_)
    websocket_->Fail(WebString::FromUTF8(message));
  return PP_OK;
}

}  // namespace content
