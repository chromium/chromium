// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/proxy/websocket_resource.h"

#include <stddef.h>

#include <limits>
#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace {

const uint32_t kMaxReasonSizeInBytes = 123;
const size_t kBaseFramingOverhead = 2;
const size_t kMaskingKeyLength = 4;
const size_t kMinimumPayloadSizeWithTwoByteExtendedPayloadLength = 126;
const size_t kMinimumPayloadSizeWithEightByteExtendedPayloadLength = 0x10000;

uint64_t SaturateAdd(uint64_t a, uint64_t b) {
  if (std::numeric_limits<uint64_t>::max() - a < b)
    return std::numeric_limits<uint64_t>::max();
  return a + b;
}

uint64_t GetFrameSize(uint64_t payload_size) {
  uint64_t overhead = kBaseFramingOverhead + kMaskingKeyLength;
  if (payload_size > kMinimumPayloadSizeWithEightByteExtendedPayloadLength)
    overhead += 8;
  else if (payload_size > kMinimumPayloadSizeWithTwoByteExtendedPayloadLength)
    overhead += 2;
  return SaturateAdd(payload_size, overhead);
}

bool InValidStateToReceive(PP_WebSocketReadyState state) {
  return state == PP_WEBSOCKETREADYSTATE_OPEN ||
         state == PP_WEBSOCKETREADYSTATE_CLOSING;
}

}  // namespace


namespace ppapi {
namespace proxy {

WebSocketResource::WebSocketResource(Connection connection,
                                     PP_Instance instance)
    : PluginResource(connection, instance),
      state_(PP_WEBSOCKETREADYSTATE_INVALID),
      error_was_received_(false),
      receive_callback_var_(nullptr),
      empty_string_(new StringVar(std::string())),
      close_code_(0),
      close_reason_(nullptr),
      close_was_clean_(PP_FALSE),
      extensions_(nullptr),
      protocol_(nullptr),
      url_(nullptr),
      buffered_amount_(0),
      buffered_amount_after_close_(0) {}

WebSocketResource::~WebSocketResource() {
}

thunk::PPB_WebSocket_API* WebSocketResource::AsPPB_WebSocket_API() {
  return this;
}

int32_t WebSocketResource::Connect(
    const PP_Var& url,
    const PP_Var protocols[],
    uint32_t protocol_count,
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(connect_callback_))
    return PP_ERROR_INPROGRESS;

  // Connect() can be called at most once.
  if (state_ != PP_WEBSOCKETREADYSTATE_INVALID)
    return PP_ERROR_INPROGRESS;
  state_ = PP_WEBSOCKETREADYSTATE_CLOSED;

  // Get the URL.
  url_ = StringVar::FromPPVar(url);
  if (!url_.get())
    return PP_ERROR_BADARGUMENT;

  // Get the protocols.
  std::set<std::string> protocol_set;
  std::vector<std::string> protocol_strings;
  protocol_strings.reserve(protocol_count);
  for (uint32_t i = 0; i < protocol_count; ++i) {
    scoped_refptr<StringVar> protocol(StringVar::FromPPVar(protocols[i]));

    // Check invalid and empty entries.
    if (!protocol.get() || !protocol->value().length())
      return PP_ERROR_BADARGUMENT;

    // Check duplicated protocol entries.
    if (protocol_set.find(protocol->value()) != protocol_set.end())
      return PP_ERROR_BADARGUMENT;
    protocol_set.insert(protocol->value());

    protocol_strings.push_back(protocol->value());
  }

  // Install callback.
  connect_callback_ = callback;

  // Create remote host in the renderer, then request to check the URL and
  // establish the connection.
  state_ = PP_WEBSOCKETREADYSTATE_CONNECTING;
  SendCreate(RENDERER, PpapiHostMsg_WebSocket_Create());
  PpapiHostMsg_WebSocket_Connect msg(url_->value(), protocol_strings);
  Call<PpapiPluginMsg_WebSocket_ConnectReply>(
      RENDERER, msg,
      base::BindOnce(&WebSocketResource::OnPluginMsgConnectReply, this));

  return PP_OK_COMPLETIONPENDING;
}

int32_t WebSocketResource::Close(uint16_t code,
                                 const PP_Var& reason,
                                 scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(close_callback_))
    return PP_ERROR_INPROGRESS;
  if (state_ == PP_WEBSOCKETREADYSTATE_INVALID)
    return PP_ERROR_FAILED;

  // Validate |code| and |reason|.
  scoped_refptr<StringVar> reason_string_var;
  std::string reason_string;
  if (code != PP_WEBSOCKETSTATUSCODE_NOT_SPECIFIED) {
    if (code != PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE &&
        (code < PP_WEBSOCKETSTATUSCODE_USER_REGISTERED_MIN ||
        code > PP_WEBSOCKETSTATUSCODE_USER_PRIVATE_MAX))
      // RFC 6455 limits applications to use reserved connection close code in
      // section 7.4.2.. The WebSocket API (http://www.w3.org/TR/websockets/)
      // defines this out of range error as InvalidAccessError in JavaScript.
      return PP_ERROR_NOACCESS;

    // |reason| must be ignored if it is PP_VARTYPE_UNDEFINED or |code| is
    // PP_WEBSOCKETSTATUSCODE_NOT_SPECIFIED.
    if (reason.type != PP_VARTYPE_UNDEFINED) {
      // Validate |reason|.
      reason_string_var = StringVar::FromPPVar(reason);
      if (!reason_string_var.get() ||
          reason_string_var->value().size() > kMaxReasonSizeInBytes)
        return PP_ERROR_BADARGUMENT;
      reason_string = reason_string_var->value();
    }
  }

  // Check state.
  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSING)
    return PP_ERROR_INPROGRESS;
  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSED)
    return PP_OK;

  // Install |callback|.
  close_callback_ = callback;

  // Abort ongoing connect.
  if (TrackedCallback::IsPending(connect_callback_)) {
    state_ = PP_WEBSOCKETREADYSTATE_CLOSING;
    // Need to do a "Post" to avoid reentering the plugin.
    connect_callback_->PostAbort();
    connect_callback_ = nullptr;
    Post(RENDERER, PpapiHostMsg_WebSocket_Fail(
        "WebSocket was closed before the connection was established."));
    return PP_OK_COMPLETIONPENDING;
  }

  // Abort ongoing receive.
  if (TrackedCallback::IsPending(receive_callback_)) {
    receive_callback_var_ = nullptr;
    // Need to do a "Post" to avoid reentering the plugin.
    receive_callback_->PostAbort();
    receive_callback_ = nullptr;
  }

  // Close connection.
  state_ = PP_WEBSOCKETREADYSTATE_CLOSING;
  PpapiHostMsg_WebSocket_Close msg(static_cast<int32_t>(code),
                                   reason_string);
  Call<PpapiPluginMsg_WebSocket_CloseReply>(
      RENDERER, msg,
      base::BindOnce(&WebSocketResource::OnPluginMsgCloseReply, this));
  return PP_OK_COMPLETIONPENDING;
}

int32_t WebSocketResource::ReceiveMessage(
    PP_Var* message,
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(receive_callback_))
    return PP_ERROR_INPROGRESS;

  // Check state.
  if (state_ == PP_WEBSOCKETREADYSTATE_INVALID ||
      state_ == PP_WEBSOCKETREADYSTATE_CONNECTING)
    return PP_ERROR_BADARGUMENT;

  // Just return received message if any received message is queued.
  if (!received_messages_.empty()) {
    receive_callback_var_ = message;
    return DoReceive();
  }

  // Check state again. In CLOSED state, no more messages will be received.
  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSED)
    return PP_ERROR_BADARGUMENT;

  // Returns PP_ERROR_FAILED after an error is received and received messages
  // is exhausted.
  if (error_was_received_)
    return PP_ERROR_FAILED;

  // Or retain |message| as buffer to store and install |callback|.
  receive_callback_var_ = message;
  receive_callback_ = callback;

  return PP_OK_COMPLETIONPENDING;
}

int32_t WebSocketResource::SendMessage(const PP_Var& message) {
  // Check state.
  if (state_ == PP_WEBSOCKETREADYSTATE_INVALID ||
      state_ == PP_WEBSOCKETREADYSTATE_CONNECTING)
    return PP_ERROR_BADARGUMENT;

  if (state_ == PP_WEBSOCKETREADYSTATE_CLOSING ||
      state_ == PP_WEBSOCKETREADYSTATE_CLOSED) {
    // Handle buffered_amount_after_close_.
    uint64_t payload_size = 0;
    if (message.type == PP_VARTYPE_STRING) {
      scoped_refptr<StringVar> message_string = StringVar::FromPPVar(message);
      if (message_string.get())
        payload_size += message_string->value().length();
    } else if (message.type == PP_VARTYPE_ARRAY_BUFFER) {
      scoped_refptr<ArrayBufferVar> message_array_buffer =
          ArrayBufferVar::FromPPVar(message);
      if (message_array_buffer.get())
        payload_size += message_array_buffer->ByteLength();
    } else {
      // TODO(toyoshim): Support Blob.
      return PP_ERROR_NOTSUPPORTED;
    }

    buffered_amount_after_close_ =
        SaturateAdd(buffered_amount_after_close_, GetFrameSize(payload_size));

    return PP_ERROR_FAILED;
  }

  // Send the message.
  if (message.type == PP_VARTYPE_STRING) {
    // Convert message to std::string, then send it.
    scoped_refptr<StringVar> message_string = StringVar::FromPPVar(message);
    if (!message_string.get())
      return PP_ERROR_BADARGUMENT;
    Post(RENDERER, PpapiHostMsg_WebSocket_SendText(message_string->value()));
  } else if (message.type == PP_VARTYPE_ARRAY_BUFFER) {
    // Convert message to std::vector<uint8_t>, then send it.
    scoped_refptr<ArrayBufferVar> message_arraybuffer =
        ArrayBufferVar::FromPPVar(message);
    if (!message_arraybuffer.get())
      return PP_ERROR_BADARGUMENT;
    uint8_t* message_data = static_cast<uint8_t*>(message_arraybuffer->Map());
    uint32_t message_length = message_arraybuffer->ByteLength();
    std::vector<uint8_t> message_vector(message_data,
                                        message_data + message_length);
    Post(RENDERER, PpapiHostMsg_WebSocket_SendBinary(message_vector));
  } else {
    // TODO(toyoshim): Support Blob.
    return PP_ERROR_NOTSUPPORTED;
  }
  return PP_OK;
}

uint64_t WebSocketResource::GetBufferedAmount() {
  return SaturateAdd(buffered_amount_, buffered_amount_after_close_);
}

uint16_t WebSocketResource::GetCloseCode() {
  return close_code_;
}

PP_Var WebSocketResource::GetCloseReason() {
  if (!close_reason_.get())
    return empty_string_->GetPPVar();
  return close_reason_->GetPPVar();
}

PP_Bool WebSocketResource::GetCloseWasClean() {
  return close_was_clean_;
}

PP_Var WebSocketResource::GetExtensions() {
  return StringVar::StringToPPVar(std::string());
}

PP_Var WebSocketResource::GetProtocol() {
  if (!protocol_.get())
    return empty_string_->GetPPVar();
  return protocol_->GetPPVar();
}

PP_WebSocketReadyState WebSocketResource::GetReadyState() {
  return state_;
}

PP_Var WebSocketResource::GetURL() {
  if (!url_.get())
    return empty_string_->GetPPVar();
  return url_->GetPPVar();
}

void WebSocketResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  if (params.sequence()) {
    PluginResource::OnReplyReceived(params, msg);
    return;
  }

  PPAPI_BEGIN_MESSAGE_MAP(WebSocketResource, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_WebSocket_ReceiveTextReply,
        OnPluginMsgReceiveTextReply)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_WebSocket_ReceiveBinaryReply,
        OnPluginMsgReceiveBinaryReply)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_0(
        PpapiPluginMsg_WebSocket_ErrorReply,
        OnPluginMsgErrorReply)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_WebSocket_BufferedAmountReply,
        OnPluginMsgBufferedAmountReply)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_WebSocket_StateReply,
        OnPluginMsgStateReply)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_WebSocket_ClosedReply,
        OnPluginMsgClosedReply)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(NOTREACHED())
  PPAPI_END_MESSAGE_MAP()
}

void WebSocketResource::OnPluginMsgConnectReply(
    const ResourceMessageReplyParams& params,
    const std::string& url,
    const std::string& protocol) {
  if (!TrackedCallback::IsPending(connect_callback_) ||
      TrackedCallback::IsScheduledToRun(connect_callback_)) {
    return;
  }

  int32_t result = params.result();
  if (result == PP_OK) {
    state_ = PP_WEBSOCKETREADYSTATE_OPEN;
    protocol_ = new StringVar(protocol);
    url_ = new StringVar(url);
  }
  connect_callback_->Run(params.result());
}

void WebSocketResource::OnPluginMsgCloseReply(
    const ResourceMessageReplyParams& params,
    uint64_t buffered_amount,
    bool was_clean,
    uint16_t code,
    const std::string& reason) {
  // Set close related properties.
  state_ = PP_WEBSOCKETREADYSTATE_CLOSED;
  buffered_amount_ = buffered_amount;
  close_was_clean_ = PP_FromBool(was_clean);
  close_code_ = code;
  close_reason_ = new StringVar(reason);

  if (TrackedCallback::IsPending(receive_callback_)) {
    receive_callback_var_ = nullptr;
    if (!TrackedCallback::IsScheduledToRun(receive_callback_))
      receive_callback_->PostRun(PP_ERROR_FAILED);
    receive_callback_ = nullptr;
  }

  if (TrackedCallback::IsPending(close_callback_)) {
    if (!TrackedCallback::IsScheduledToRun(close_callback_))
      close_callback_->PostRun(params.result());
    close_callback_ = nullptr;
  }
}

void WebSocketResource::OnPluginMsgReceiveTextReply(
    const ResourceMessageReplyParams& params,
    const std::string& message) {
  // Dispose packets after receiving an error or in invalid state.
  if (error_was_received_ || !InValidStateToReceive(state_))
    return;

  // Append received data to queue.
  received_messages_.push(scoped_refptr<Var>(new StringVar(message)));

  if (!TrackedCallback::IsPending(receive_callback_) ||
      TrackedCallback::IsScheduledToRun(receive_callback_)) {
    return;
  }

  receive_callback_->Run(DoReceive());
}

void WebSocketResource::OnPluginMsgReceiveBinaryReply(
    const ResourceMessageReplyParams& params,
    const std::vector<uint8_t>& message) {
  // Dispose packets after receiving an error or in invalid state.
  if (error_was_received_ || !InValidStateToReceive(state_))
    return;

  // Append received data to queue.
  scoped_refptr<Var> message_var(
      PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferVar(
          base::checked_cast<uint32_t>(message.size()),
          &message.front()));
  received_messages_.push(message_var);

  if (!TrackedCallback::IsPending(receive_callback_) ||
      TrackedCallback::IsScheduledToRun(receive_callback_)) {
    return;
  }

  receive_callback_->Run(DoReceive());
}

void WebSocketResource::OnPluginMsgErrorReply(
    const ResourceMessageReplyParams& params) {
  error_was_received_ = true;

  if (!TrackedCallback::IsPending(receive_callback_) ||
      TrackedCallback::IsScheduledToRun(receive_callback_)) {
    return;
  }

  // No more text or binary messages will be received. If there is ongoing
  // ReceiveMessage(), we must invoke the callback with error code here.
  receive_callback_var_ = nullptr;
  receive_callback_->Run(PP_ERROR_FAILED);
}

void WebSocketResource::OnPluginMsgBufferedAmountReply(
    const ResourceMessageReplyParams& params,
    uint64_t buffered_amount) {
  buffered_amount_ = buffered_amount;
}

void WebSocketResource::OnPluginMsgStateReply(
    const ResourceMessageReplyParams& params,
    int32_t state) {
  state_ = static_cast<PP_WebSocketReadyState>(state);
}

void WebSocketResource::OnPluginMsgClosedReply(
    const ResourceMessageReplyParams& params,
    uint64_t buffered_amount,
    bool was_clean,
    uint16_t code,
    const std::string& reason) {
  OnPluginMsgCloseReply(params, buffered_amount, was_clean, code, reason);
}

int32_t WebSocketResource::DoReceive() {
  if (!receive_callback_var_)
    return PP_OK;

  *receive_callback_var_ = received_messages_.front()->GetPPVar();
  received_messages_.pop();
  receive_callback_var_ = nullptr;
  return PP_OK;
}

}  // namespace proxy
}  // namespace ppapi
