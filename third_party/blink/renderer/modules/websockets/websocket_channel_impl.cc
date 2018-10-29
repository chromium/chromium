/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/websockets/websocket_channel_impl.h"

#include <memory>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/base_fetch_context.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/websockets/inspector_websocket_events.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_client.h"
#include "third_party/blink/renderer/modules/websockets/websocket_handle_impl.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/network/network_log.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace {

enum WebSocketOpCode {
  kOpCodeText = 0x1,
  kOpCodeBinary = 0x2,
};

}  // namespace

class WebSocketChannelImpl::BlobLoader final
    : public GarbageCollectedFinalized<WebSocketChannelImpl::BlobLoader>,
      public FileReaderLoaderClient {
 public:
  BlobLoader(scoped_refptr<BlobDataHandle>, WebSocketChannelImpl*);
  ~BlobLoader() override = default;

  void Cancel();

  // FileReaderLoaderClient functions.
  void DidStartLoading() override {}
  void DidReceiveData() override {}
  void DidFinishLoading() override;
  void DidFail(FileError::ErrorCode) override;

  void Trace(blink::Visitor* visitor) { visitor->Trace(channel_); }

 private:
  Member<WebSocketChannelImpl> channel_;
  std::unique_ptr<FileReaderLoader> loader_;
};

class WebSocketChannelImpl::Message
    : public GarbageCollectedFinalized<WebSocketChannelImpl::Message> {
 public:
  explicit Message(const CString&);
  explicit Message(scoped_refptr<BlobDataHandle>);
  explicit Message(DOMArrayBuffer*);
  // For WorkerWebSocketChannel
  explicit Message(std::unique_ptr<Vector<char>>, MessageType);
  // Close message
  Message(unsigned short code, const String& reason);

  void Trace(blink::Visitor* visitor) { visitor->Trace(array_buffer); }

  MessageType type;

  CString text;
  scoped_refptr<BlobDataHandle> blob_data_handle;
  Member<DOMArrayBuffer> array_buffer;
  std::unique_ptr<Vector<char>> vector_data;
  unsigned short code;
  String reason;
};

WebSocketChannelImpl::BlobLoader::BlobLoader(
    scoped_refptr<BlobDataHandle> blob_data_handle,
    WebSocketChannelImpl* channel)
    : channel_(channel),
      loader_(FileReaderLoader::Create(FileReaderLoader::kReadAsArrayBuffer,
                                       this)) {
  loader_->Start(std::move(blob_data_handle));
}

void WebSocketChannelImpl::BlobLoader::Cancel() {
  loader_->Cancel();
  loader_ = nullptr;
}

void WebSocketChannelImpl::BlobLoader::DidFinishLoading() {
  channel_->DidFinishLoadingBlob(loader_->ArrayBufferResult());
  loader_ = nullptr;
}

void WebSocketChannelImpl::BlobLoader::DidFail(
    FileError::ErrorCode error_code) {
  channel_->DidFailLoadingBlob(error_code);
  loader_ = nullptr;
}

struct WebSocketChannelImpl::ConnectInfo {
  ConnectInfo(const String& selected_protocol, const String& extensions)
      : selected_protocol(selected_protocol), extensions(extensions) {}

  const String selected_protocol;
  const String extensions;
};

// static
WebSocketChannelImpl* WebSocketChannelImpl::CreateForTesting(
    Document* document,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location,
    WebSocketHandle* handle,
    std::unique_ptr<WebSocketHandshakeThrottle> handshake_throttle) {
  auto* channel = new WebSocketChannelImpl(
      document, client, std::move(location), base::WrapUnique(handle));
  channel->handshake_throttle_ = std::move(handshake_throttle);
  return channel;
}

// static
WebSocketChannelImpl* WebSocketChannelImpl::Create(
    ExecutionContext* execution_context,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location) {
  auto* channel =
      new WebSocketChannelImpl(execution_context, client, std::move(location),
                               std::make_unique<WebSocketHandleImpl>());
  channel->handshake_throttle_ =
      channel->GetBaseFetchContext()->CreateWebSocketHandshakeThrottle();
  return channel;
}

WebSocketChannelImpl::WebSocketChannelImpl(
    ExecutionContext* execution_context,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location,
    std::unique_ptr<WebSocketHandle> handle)
    : handle_(std::move(handle)),
      client_(client),
      identifier_(CreateUniqueIdentifier()),
      execution_context_(execution_context),
      sending_quota_(0),
      received_data_size_for_flow_control_(0),
      sent_size_of_top_message_(0),
      location_at_construction_(std::move(location)),
      throttle_passed_(false) {
  if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_))
    scope->EnsureFetcher();
}

WebSocketChannelImpl::~WebSocketChannelImpl() {
  DCHECK(!blob_loader_);
}

bool WebSocketChannelImpl::Connect(
    const KURL& url,
    const String& protocol,
    network::mojom::blink::WebSocketPtr socket_ptr) {
  NETWORK_DVLOG(1) << this << " Connect()";
  if (!handle_)
    return false;

  if (GetBaseFetchContext()->ShouldBlockWebSocketByMixedContentCheck(url))
    return false;

  if (auto* scheduler = execution_context_->GetScheduler())
    connection_handle_for_scheduler_ = scheduler->OnActiveConnectionCreated();

  if (MixedContentChecker::IsMixedContent(
          execution_context_->GetSecurityOrigin(), url)) {
    String message =
        "Connecting to a non-secure WebSocket server from a secure origin is "
        "deprecated.";
    execution_context_->AddConsoleMessage(ConsoleMessage::Create(
        kJSMessageSource, kWarningMessageLevel, message));
  }

  url_ = url;
  Vector<String> protocols;
  // Avoid placing an empty token in the Vector when the protocol string is
  // empty.
  if (!protocol.IsEmpty()) {
    // Since protocol is already verified and escaped, we can simply split
    // it.
    protocol.Split(", ", true, protocols);
  }

  // If the connection needs to be filtered, asynchronously fail. Synchronous
  // failure blocks the worker thread which should be avoided. Note that
  // returning "true" just indicates that this was not a mixed content error.
  if (ShouldDisallowConnection(url)) {
    execution_context_->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(FROM_HERE,
                   WTF::Bind(&WebSocketChannelImpl::TearDownFailedConnection,
                             WrapPersistent(this)));
    return true;
  }

  handle_->Connect(
      std::move(socket_ptr), url, protocols,
      GetBaseFetchContext()->GetSiteForCookies(),
      execution_context_->UserAgent(), this,
      execution_context_->GetTaskRunner(TaskType::kNetworking).get());

  if (handshake_throttle_) {
    handshake_throttle_->ThrottleHandshake(url, this);
  } else {
    // Treat no throttle as success.
    throttle_passed_ = true;
  }

  TRACE_EVENT_INSTANT1("devtools.timeline", "WebSocketCreate",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorWebSocketCreateEvent::Data(
                           execution_context_, identifier_, url, protocol));
  probe::didCreateWebSocket(execution_context_, identifier_, url, protocol);
  return true;
}

bool WebSocketChannelImpl::Connect(const KURL& url, const String& protocol) {
  network::mojom::blink::WebSocketPtr socket_ptr;
  auto socket_request = mojo::MakeRequest(&socket_ptr);
  service_manager::InterfaceProvider* interface_provider =
      execution_context_->GetInterfaceProvider();
  if (interface_provider)
    interface_provider->GetInterface(std::move(socket_request));
  return Connect(url, protocol, std::move(socket_ptr));
}

void WebSocketChannelImpl::Send(const CString& message) {
  NETWORK_DVLOG(1) << this << " Send(" << message << ") (CString argument)";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  probe::didSendWebSocketFrame(execution_context_, identifier_,
                               WebSocketOpCode::kOpCodeText, true,
                               message.data(), message.length());
  messages_.push_back(new Message(message));
  ProcessSendQueue();
}

void WebSocketChannelImpl::Send(
    scoped_refptr<BlobDataHandle> blob_data_handle) {
  NETWORK_DVLOG(1) << this << " Send(" << blob_data_handle->Uuid() << ", "
                   << blob_data_handle->GetType() << ", "
                   << blob_data_handle->size() << ") "
                   << "(BlobDataHandle argument)";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  // FIXME: We can't access the data here.
  // Since Binary data are not displayed in Inspector, this does not
  // affect actual behavior.
  probe::didSendWebSocketFrame(execution_context_, identifier_,
                               WebSocketOpCode::kOpCodeBinary, true, "", 0);
  messages_.push_back(new Message(std::move(blob_data_handle)));
  ProcessSendQueue();
}

void WebSocketChannelImpl::Send(const DOMArrayBuffer& buffer,
                                unsigned byte_offset,
                                unsigned byte_length) {
  NETWORK_DVLOG(1) << this << " Send(" << buffer.Data() << ", " << byte_offset
                   << ", " << byte_length << ") "
                   << "(DOMArrayBuffer argument)";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  probe::didSendWebSocketFrame(
      execution_context_, identifier_, WebSocketOpCode::kOpCodeBinary, true,
      static_cast<const char*>(buffer.Data()) + byte_offset, byte_length);
  // buffer.slice copies its contents.
  // FIXME: Reduce copy by sending the data immediately when we don't need to
  // queue the data.
  messages_.push_back(
      new Message(buffer.Slice(byte_offset, byte_offset + byte_length)));
  ProcessSendQueue();
}

void WebSocketChannelImpl::SendTextAsCharVector(
    std::unique_ptr<Vector<char>> data) {
  NETWORK_DVLOG(1) << this << " SendTextAsCharVector("
                   << static_cast<void*>(data.get()) << ", " << data->size()
                   << ")";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  probe::didSendWebSocketFrame(execution_context_, identifier_,
                               WebSocketOpCode::kOpCodeText, true, data->data(),
                               data->size());
  messages_.push_back(
      new Message(std::move(data), kMessageTypeTextAsCharVector));
  ProcessSendQueue();
}

void WebSocketChannelImpl::SendBinaryAsCharVector(
    std::unique_ptr<Vector<char>> data) {
  NETWORK_DVLOG(1) << this << " SendBinaryAsCharVector("
                   << static_cast<void*>(data.get()) << ", " << data->size()
                   << ")";
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  probe::didSendWebSocketFrame(execution_context_, identifier_,
                               WebSocketOpCode::kOpCodeBinary, true,
                               data->data(), data->size());
  messages_.push_back(
      new Message(std::move(data), kMessageTypeBinaryAsCharVector));
  ProcessSendQueue();
}

void WebSocketChannelImpl::Close(int code, const String& reason) {
  NETWORK_DVLOG(1) << this << " Close(" << code << ", " << reason << ")";
  DCHECK(handle_);
  unsigned short code_to_send = static_cast<unsigned short>(
      code == kCloseEventCodeNotSpecified ? kCloseEventCodeNoStatusRcvd : code);
  messages_.push_back(new Message(code_to_send, reason));
  ProcessSendQueue();
}

void WebSocketChannelImpl::Fail(const String& reason,
                                MessageLevel level,
                                std::unique_ptr<SourceLocation> location) {
  NETWORK_DVLOG(1) << this << " Fail(" << reason << ")";
  probe::didReceiveWebSocketFrameError(execution_context_, identifier_, reason);
  const String message =
      "WebSocket connection to '" + url_.ElidedString() + "' failed: " + reason;

  std::unique_ptr<SourceLocation> captured_location = SourceLocation::Capture();
  if (!captured_location->IsUnknown()) {
    // If we are in JavaScript context, use the current location instead
    // of passed one - it's more precise.
    location = std::move(captured_location);
  } else if (location->IsUnknown()) {
    // No information is specified by the caller. Use the line number at the
    // connection.
    location = location_at_construction_->Clone();
  }

  execution_context_->AddConsoleMessage(ConsoleMessage::Create(
      kJSMessageSource, level, message, std::move(location)));
  // |reason| is only for logging and should not be provided for scripts,
  // hence close reason must be empty in tearDownFailedConnection.
  TearDownFailedConnection();
}

void WebSocketChannelImpl::Disconnect() {
  NETWORK_DVLOG(1) << this << " disconnect()";
  if (identifier_) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketDestroy", TRACE_EVENT_SCOPE_THREAD,
        "data", InspectorWebSocketEvent::Data(execution_context_, identifier_));
    probe::didCloseWebSocket(execution_context_, identifier_);
  }
  connection_handle_for_scheduler_.reset();
  AbortAsyncOperations();
  handshake_throttle_.reset();
  handle_.reset();
  client_ = nullptr;
  identifier_ = 0;
}

WebSocketChannelImpl::Message::Message(const CString& text)
    : type(kMessageTypeText), text(text) {}

WebSocketChannelImpl::Message::Message(
    scoped_refptr<BlobDataHandle> blob_data_handle)
    : type(kMessageTypeBlob), blob_data_handle(std::move(blob_data_handle)) {}

WebSocketChannelImpl::Message::Message(DOMArrayBuffer* array_buffer)
    : type(kMessageTypeArrayBuffer), array_buffer(array_buffer) {}

WebSocketChannelImpl::Message::Message(
    std::unique_ptr<Vector<char>> vector_data,
    MessageType type)
    : type(type), vector_data(std::move(vector_data)) {
  DCHECK(type == kMessageTypeTextAsCharVector ||
         type == kMessageTypeBinaryAsCharVector);
}

WebSocketChannelImpl::Message::Message(unsigned short code,
                                       const String& reason)
    : type(kMessageTypeClose), code(code), reason(reason) {}

void WebSocketChannelImpl::SendInternal(
    WebSocketHandle::MessageType message_type,
    const char* data,
    wtf_size_t total_size,
    uint64_t* consumed_buffered_amount) {
  WebSocketHandle::MessageType frame_type =
      sent_size_of_top_message_ ? WebSocketHandle::kMessageTypeContinuation
                                : message_type;
  DCHECK_GE(total_size, sent_size_of_top_message_);
  // The first cast is safe since the result of min() never exceeds
  // the range of wtf_size_t. The second cast is necessary to compile
  // min() on ILP32.
  wtf_size_t size = static_cast<wtf_size_t>(
      std::min(sending_quota_,
               static_cast<uint64_t>(total_size - sent_size_of_top_message_)));
  bool final = (sent_size_of_top_message_ + size == total_size);

  handle_->Send(final, frame_type, data + sent_size_of_top_message_, size);

  sent_size_of_top_message_ += size;
  sending_quota_ -= size;
  *consumed_buffered_amount += size;

  if (final) {
    messages_.pop_front();
    sent_size_of_top_message_ = 0;
  }
}

void WebSocketChannelImpl::ProcessSendQueue() {
  DCHECK(handle_);
  uint64_t consumed_buffered_amount = 0;
  while (!messages_.IsEmpty() && !blob_loader_) {
    Message* message = messages_.front().Get();
    CHECK(message);
    if (sending_quota_ == 0 && message->type != kMessageTypeClose)
      break;
    switch (message->type) {
      case kMessageTypeText:
        SendInternal(WebSocketHandle::kMessageTypeText, message->text.data(),
                     message->text.length(), &consumed_buffered_amount);
        break;
      case kMessageTypeBlob:
        CHECK(!blob_loader_);
        CHECK(message);
        CHECK(message->blob_data_handle);
        blob_loader_ = new BlobLoader(message->blob_data_handle, this);
        break;
      case kMessageTypeArrayBuffer:
        CHECK(message->array_buffer);
        SendInternal(WebSocketHandle::kMessageTypeBinary,
                     static_cast<const char*>(message->array_buffer->Data()),
                     message->array_buffer->ByteLength(),
                     &consumed_buffered_amount);
        break;
      case kMessageTypeTextAsCharVector:
        CHECK(message->vector_data);
        SendInternal(WebSocketHandle::kMessageTypeText,
                     message->vector_data->data(), message->vector_data->size(),
                     &consumed_buffered_amount);
        break;
      case kMessageTypeBinaryAsCharVector:
        CHECK(message->vector_data);
        SendInternal(WebSocketHandle::kMessageTypeBinary,
                     message->vector_data->data(), message->vector_data->size(),
                     &consumed_buffered_amount);
        break;
      case kMessageTypeClose: {
        // No message should be sent from now on.
        DCHECK_EQ(messages_.size(), 1u);
        DCHECK_EQ(sent_size_of_top_message_, 0u);
        handshake_throttle_.reset();
        handle_->Close(message->code, message->reason);
        messages_.pop_front();
        break;
      }
    }
  }
  if (client_ && consumed_buffered_amount > 0)
    client_->DidConsumeBufferedAmount(consumed_buffered_amount);
}

void WebSocketChannelImpl::FlowControlIfNecessary() {
  if (!handle_ || received_data_size_for_flow_control_ <
                      kReceivedDataSizeForFlowControlHighWaterMark) {
    return;
  }
  handle_->FlowControl(received_data_size_for_flow_control_);
  received_data_size_for_flow_control_ = 0;
}

void WebSocketChannelImpl::InitialFlowControl() {
  DCHECK_EQ(received_data_size_for_flow_control_, 0u);
  DCHECK(handle_);
  handle_->FlowControl(kReceivedDataSizeForFlowControlHighWaterMark * 2);
}

void WebSocketChannelImpl::AbortAsyncOperations() {
  if (blob_loader_) {
    blob_loader_->Cancel();
    blob_loader_.Clear();
  }
}

void WebSocketChannelImpl::HandleDidClose(bool was_clean,
                                          unsigned short code,
                                          const String& reason) {
  handshake_throttle_.reset();
  handle_.reset();
  AbortAsyncOperations();
  if (!client_) {
    return;
  }
  WebSocketChannelClient* client = client_;
  client_ = nullptr;
  WebSocketChannelClient::ClosingHandshakeCompletionStatus status =
      was_clean ? WebSocketChannelClient::kClosingHandshakeComplete
                : WebSocketChannelClient::kClosingHandshakeIncomplete;
  client->DidClose(status, code, reason);
}

void WebSocketChannelImpl::DidConnect(WebSocketHandle* handle,
                                      const String& selected_protocol,
                                      const String& extensions) {
  NETWORK_DVLOG(1) << this << " DidConnect(" << handle << ", "
                   << String(selected_protocol) << ", " << String(extensions)
                   << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());
  DCHECK(client_);

  if (!throttle_passed_) {
    connect_info_ =
        std::make_unique<ConnectInfo>(selected_protocol, extensions);
    return;
  }

  InitialFlowControl();

  handshake_throttle_.reset();

  client_->DidConnect(selected_protocol, extensions);
}

void WebSocketChannelImpl::DidStartOpeningHandshake(
    WebSocketHandle* handle,
    network::mojom::blink::WebSocketHandshakeRequestPtr request) {
  NETWORK_DVLOG(1) << this << " DidStartOpeningHandshake(" << handle << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "WebSocketSendHandshakeRequest",
      TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorWebSocketEvent::Data(execution_context_, identifier_));
  probe::willSendWebSocketHandshakeRequest(execution_context_, identifier_,
                                           request.get());
  handshake_request_ = std::move(request);
}

void WebSocketChannelImpl::DidFinishOpeningHandshake(
    WebSocketHandle* handle,
    network::mojom::blink::WebSocketHandshakeResponsePtr response) {
  NETWORK_DVLOG(1) << this << " DidFinishOpeningHandshake(" << handle << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "WebSocketReceiveHandshakeResponse",
      TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorWebSocketEvent::Data(execution_context_, identifier_));
  probe::didReceiveWebSocketHandshakeResponse(execution_context_, identifier_,
                                              handshake_request_.get(),
                                              response.get());
  handshake_request_ = nullptr;
}

void WebSocketChannelImpl::DidFail(WebSocketHandle* handle,
                                   const String& message) {
  NETWORK_DVLOG(1) << this << " DidFail(" << handle << ", " << String(message)
                   << ")";

  connection_handle_for_scheduler_.reset();

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  // This function is called when the browser is required to fail the
  // WebSocketConnection. Hence we fail this channel by calling
  // |this->failAsError| function.
  FailAsError(message);
}

void WebSocketChannelImpl::DidReceiveData(WebSocketHandle* handle,
                                          bool fin,
                                          WebSocketHandle::MessageType type,
                                          const char* data,
                                          size_t size) {
  NETWORK_DVLOG(1) << this << " DidReceiveData(" << handle << ", " << fin
                   << ", " << type << ", (" << static_cast<const void*>(data)
                   << ", " << size << "))";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());
  DCHECK(client_);
  // Non-final frames cannot be empty.
  DCHECK(fin || size);

  switch (type) {
    case WebSocketHandle::kMessageTypeText:
      DCHECK(receiving_message_data_.IsEmpty());
      receiving_message_type_is_text_ = true;
      break;
    case WebSocketHandle::kMessageTypeBinary:
      DCHECK(receiving_message_data_.IsEmpty());
      receiving_message_type_is_text_ = false;
      break;
    case WebSocketHandle::kMessageTypeContinuation:
      DCHECK(!receiving_message_data_.IsEmpty());
      break;
  }

  receiving_message_data_.Append(data, SafeCast<uint32_t>(size));
  received_data_size_for_flow_control_ += size;
  FlowControlIfNecessary();
  if (!fin) {
    return;
  }
  // FIXME: Change the inspector API to show the entire message instead
  // of individual frames.
  auto opcode = receiving_message_type_is_text_
                    ? WebSocketOpCode::kOpCodeText
                    : WebSocketOpCode::kOpCodeBinary;
  probe::didReceiveWebSocketFrame(execution_context_, identifier_, opcode,
                                  false, receiving_message_data_.data(),
                                  receiving_message_data_.size());
  if (receiving_message_type_is_text_) {
    String message = receiving_message_data_.IsEmpty()
                         ? g_empty_string
                         : String::FromUTF8(receiving_message_data_.data(),
                                            receiving_message_data_.size());
    receiving_message_data_.clear();
    if (message.IsNull()) {
      FailAsError("Could not decode a text frame as UTF-8.");
    } else {
      client_->DidReceiveTextMessage(message);
    }
  } else {
    std::unique_ptr<Vector<char>> binary_data =
        std::make_unique<Vector<char>>();
    binary_data->swap(receiving_message_data_);
    client_->DidReceiveBinaryMessage(std::move(binary_data));
  }
}

void WebSocketChannelImpl::DidClose(WebSocketHandle* handle,
                                    bool was_clean,
                                    unsigned short code,
                                    const String& reason) {
  NETWORK_DVLOG(1) << this << " DidClose(" << handle << ", " << was_clean
                   << ", " << code << ", " << String(reason) << ")";

  connection_handle_for_scheduler_.reset();

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  handle_.reset();

  if (identifier_) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketDestroy", TRACE_EVENT_SCOPE_THREAD,
        "data", InspectorWebSocketEvent::Data(execution_context_, identifier_));
    probe::didCloseWebSocket(execution_context_, identifier_);
    identifier_ = 0;
  }

  HandleDidClose(was_clean, code, reason);
}

void WebSocketChannelImpl::DidReceiveFlowControl(WebSocketHandle* handle,
                                                 int64_t quota) {
  NETWORK_DVLOG(1) << this << " DidReceiveFlowControl(" << handle << ", "
                   << quota << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());
  DCHECK_GE(quota, 0);

  sending_quota_ += quota;
  ProcessSendQueue();
}

void WebSocketChannelImpl::DidStartClosingHandshake(WebSocketHandle* handle) {
  NETWORK_DVLOG(1) << this << " DidStartClosingHandshake(" << handle << ")";

  DCHECK(handle_);
  DCHECK_EQ(handle, handle_.get());

  if (client_)
    client_->DidStartClosingHandshake();
}

void WebSocketChannelImpl::OnSuccess() {
  DCHECK(!throttle_passed_);
  DCHECK(handshake_throttle_);
  throttle_passed_ = true;
  handshake_throttle_ = nullptr;
  if (connect_info_) {
    // No flow control quota is supplied to the browser until we are ready to
    // receive messages. This fixes crbug.com/786776.
    InitialFlowControl();

    client_->DidConnect(std::move(connect_info_->selected_protocol),
                        std::move(connect_info_->extensions));
    connect_info_.reset();
  }
}

void WebSocketChannelImpl::OnError(const WebString& console_message) {
  DCHECK(!throttle_passed_);
  DCHECK(handshake_throttle_);
  handshake_throttle_ = nullptr;
  FailAsError(console_message);
}

void WebSocketChannelImpl::DidFinishLoadingBlob(DOMArrayBuffer* buffer) {
  blob_loader_.Clear();
  DCHECK(handle_);
  // The loaded blob is always placed on |messages_[0]|.
  DCHECK_GT(messages_.size(), 0u);
  DCHECK_EQ(messages_.front()->type, kMessageTypeBlob);
  // We replace it with the loaded blob.
  messages_.front() = new Message(buffer);
  ProcessSendQueue();
}

void WebSocketChannelImpl::DidFailLoadingBlob(FileError::ErrorCode error_code) {
  blob_loader_.Clear();
  if (error_code == FileError::kAbortErr) {
    // The error is caused by cancel().
    return;
  }
  // FIXME: Generate human-friendly reason message.
  FailAsError("Failed to load Blob: error code = " +
              String::Number(error_code));
}

void WebSocketChannelImpl::TearDownFailedConnection() {
  // |handle_| and |client_| can be null here.
  connection_handle_for_scheduler_.reset();
  handshake_throttle_.reset();

  if (client_)
    client_->DidError();

  HandleDidClose(false, kCloseEventCodeAbnormalClosure, String());
}

bool WebSocketChannelImpl::ShouldDisallowConnection(const KURL& url) {
  DCHECK(handle_);
  SubresourceFilter* subresource_filter =
      GetBaseFetchContext()->GetSubresourceFilter();
  if (!subresource_filter)
    return false;
  return !subresource_filter->AllowWebSocketConnection(url);
}

BaseFetchContext* WebSocketChannelImpl::GetBaseFetchContext() const {
  ResourceFetcher* resource_fetcher = execution_context_->Fetcher();
  return static_cast<BaseFetchContext*>(&resource_fetcher->Context());
}

void WebSocketChannelImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(blob_loader_);
  visitor->Trace(messages_);
  visitor->Trace(client_);
  visitor->Trace(execution_context_);
  WebSocketChannel::Trace(visitor);
}

std::ostream& operator<<(std::ostream& ostream,
                         const WebSocketChannelImpl* channel) {
  return ostream << "WebSocketChannelImpl "
                 << static_cast<const void*>(channel);
}

}  // namespace blink
