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

#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
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
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/network/network_log.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"

namespace blink {

namespace {

enum WebSocketOpCode {
  kOpCodeText = 0x1,
  kOpCodeBinary = 0x2,
};

}  // namespace

class WebSocketChannelImpl::BlobLoader final
    : public GarbageCollected<WebSocketChannelImpl::BlobLoader>,
      public FileReaderLoaderClient {
 public:
  BlobLoader(scoped_refptr<BlobDataHandle>,
             WebSocketChannelImpl*,
             scoped_refptr<base::SingleThreadTaskRunner>);
  ~BlobLoader() override = default;

  void Cancel();

  // FileReaderLoaderClient functions.
  void DidStartLoading() override {}
  void DidReceiveData() override {}
  void DidFinishLoading() override;
  void DidFail(FileErrorCode) override;

  void Trace(blink::Visitor* visitor) { visitor->Trace(channel_); }

 private:
  Member<WebSocketChannelImpl> channel_;
  std::unique_ptr<FileReaderLoader> loader_;
};

class WebSocketChannelImpl::Message final
    : public GarbageCollected<WebSocketChannelImpl::Message> {
 public:
  Message(const std::string&, base::OnceClosure completion_callback);
  explicit Message(scoped_refptr<BlobDataHandle>);
  Message(DOMArrayBuffer*, base::OnceClosure completion_callback);
  // Close message
  Message(uint16_t code, const String& reason);

  void Trace(blink::Visitor* visitor) { visitor->Trace(array_buffer); }

  MessageType type;

  std::string text;
  scoped_refptr<BlobDataHandle> blob_data_handle;
  Member<DOMArrayBuffer> array_buffer;
  uint16_t code;
  String reason;
  base::OnceClosure completion_callback;
};

WebSocketChannelImpl::BlobLoader::BlobLoader(
    scoped_refptr<BlobDataHandle> blob_data_handle,
    WebSocketChannelImpl* channel,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : channel_(channel),
      loader_(std::make_unique<FileReaderLoader>(
          FileReaderLoader::kReadAsArrayBuffer,
          this,
          std::move(task_runner))) {
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

void WebSocketChannelImpl::BlobLoader::DidFail(FileErrorCode error_code) {
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
    ExecutionContext* execution_context,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location,
    std::unique_ptr<WebSocketHandshakeThrottle> handshake_throttle) {
  auto* channel = MakeGarbageCollected<WebSocketChannelImpl>(
      execution_context, client, std::move(location));
  channel->handshake_throttle_ = std::move(handshake_throttle);
  return channel;
}

// static
WebSocketChannelImpl* WebSocketChannelImpl::Create(
    ExecutionContext* execution_context,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location) {
  auto* channel = MakeGarbageCollected<WebSocketChannelImpl>(
      execution_context, client, std::move(location));
  channel->handshake_throttle_ =
      channel->GetBaseFetchContext()->CreateWebSocketHandshakeThrottle();
  return channel;
}

WebSocketChannelImpl::WebSocketChannelImpl(
    ExecutionContext* execution_context,
    WebSocketChannelClient* client,
    std::unique_ptr<SourceLocation> location)
    : client_(client),
      identifier_(CreateUniqueIdentifier()),
      message_chunks_(execution_context->GetTaskRunner(TaskType::kNetworking)),
      execution_context_(execution_context),
      location_at_construction_(std::move(location)),
      client_receiver_(this),
      readable_watcher_(
          FROM_HERE,
          mojo::SimpleWatcher::ArmingPolicy::MANUAL,
          execution_context->GetTaskRunner(TaskType::kNetworking)),
      file_reading_task_runner_(
          execution_context->GetTaskRunner(TaskType::kFileReading)) {
  if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_))
    scope->EnsureFetcher();
}

WebSocketChannelImpl::~WebSocketChannelImpl() = default;

bool WebSocketChannelImpl::Connect(const KURL& url, const String& protocol) {
  NETWORK_DVLOG(1) << this << " Connect()";

  if (GetBaseFetchContext()->ShouldBlockWebSocketByMixedContentCheck(url)) {
    has_initiated_opening_handshake_ = false;
    return false;
  }

  if (auto* scheduler = execution_context_->GetScheduler()) {
    feature_handle_for_scheduler_ = scheduler->RegisterFeature(
        SchedulingPolicy::Feature::kWebSocket,
        {SchedulingPolicy::DisableAggressiveThrottling(),
         SchedulingPolicy::RecordMetricsForBackForwardCache()});
  }

  if (MixedContentChecker::IsMixedContent(
          execution_context_->GetSecurityOrigin(), url)) {
    String message =
        "Connecting to a non-secure WebSocket server from a secure origin is "
        "deprecated.";
    execution_context_->AddConsoleMessage(
        ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript,
                               mojom::ConsoleMessageLevel::kWarning, message));
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

  mojo::Remote<mojom::blink::WebSocketConnector> connector;
  if (execution_context_->GetInterfaceProvider()) {
    execution_context_->GetInterfaceProvider()->GetInterface(
        connector.BindNewPipeAndPassReceiver(
            execution_context_->GetTaskRunner(TaskType::kWebSocket)));
  } else {
    // Create a fake request. This will lead to a closed WebSocket due to
    // a mojo connection error.
    ignore_result(connector.BindNewPipeAndPassReceiver());
  }

  connector->Connect(
      url, protocols, GetBaseFetchContext()->GetSiteForCookies(),
      execution_context_->UserAgent(),
      handshake_client_receiver_.BindNewPipeAndPassRemote(
          execution_context_->GetTaskRunner(TaskType::kWebSocket)));
  handshake_client_receiver_.set_disconnect_with_reason_handler(
      WTF::Bind(&WebSocketChannelImpl::OnConnectionError,
                WrapWeakPersistent(this), FROM_HERE));
  has_initiated_opening_handshake_ = true;

  if (handshake_throttle_) {
    // The use of WrapWeakPersistent is safe and motivated by the fact that if
    // the WebSocket is no longer referenced, there's no point in keeping it
    // alive just to receive the throttling result.
    handshake_throttle_->ThrottleHandshake(
        url, WTF::Bind(&WebSocketChannelImpl::OnCompletion,
                       WrapWeakPersistent(this)));
  } else {
    // Treat no throttle as success.
    throttle_passed_ = true;
  }

  TRACE_EVENT_INSTANT1("devtools.timeline", "WebSocketCreate",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorWebSocketCreateEvent::Data(
                           execution_context_, identifier_, url, protocol));
  probe::DidCreateWebSocket(execution_context_, identifier_, url, protocol);
  return true;
}

WebSocketChannel::SendResult WebSocketChannelImpl::Send(
    const std::string& message,
    base::OnceClosure completion_callback) {
  NETWORK_DVLOG(1) << this << " Send(" << message << ") (std::string argument)";
  probe::DidSendWebSocketMessage(execution_context_, identifier_,
                                 WebSocketOpCode::kOpCodeText, true,
                                 message.c_str(), message.length());
  if (messages_.empty() &&
      MaybeSendSynchronously(network::mojom::blink::WebSocketMessageType::TEXT,
                             message)) {
    return SendResult::SENT_SYNCHRONOUSLY;
  }

  messages_.push_back(
      MakeGarbageCollected<Message>(message, std::move(completion_callback)));

  ProcessSendQueue();

  // If we managed to flush this message synchronously after all, it would mean
  // that the callback was fired re-entrantly, which would be bad.
  DCHECK(!messages_.empty());

  return SendResult::CALLBACK_WILL_BE_CALLED;
}

void WebSocketChannelImpl::Send(
    scoped_refptr<BlobDataHandle> blob_data_handle) {
  NETWORK_DVLOG(1) << this << " Send(" << blob_data_handle->Uuid() << ", "
                   << blob_data_handle->GetType() << ", "
                   << blob_data_handle->size() << ") "
                   << "(BlobDataHandle argument)";
  // FIXME: We can't access the data here.
  // Since Binary data are not displayed in Inspector, this does not
  // affect actual behavior.
  probe::DidSendWebSocketMessage(execution_context_, identifier_,
                                 WebSocketOpCode::kOpCodeBinary, true, "", 0);
  messages_.push_back(
      MakeGarbageCollected<Message>(std::move(blob_data_handle)));
  ProcessSendQueue();
}

WebSocketChannel::SendResult WebSocketChannelImpl::Send(
    const DOMArrayBuffer& buffer,
    unsigned byte_offset,
    unsigned byte_length,
    base::OnceClosure completion_callback) {
  NETWORK_DVLOG(1) << this << " Send(" << buffer.Data() << ", " << byte_offset
                   << ", " << byte_length << ") "
                   << "(DOMArrayBuffer argument)";
  probe::DidSendWebSocketMessage(
      execution_context_, identifier_, WebSocketOpCode::kOpCodeBinary, true,
      static_cast<const char*>(buffer.Data()) + byte_offset, byte_length);
  if (messages_.empty() &&
      MaybeSendSynchronously(
          network::mojom::blink::WebSocketMessageType::BINARY,
          base::make_span(static_cast<const char*>(buffer.Data()) + byte_offset,
                          byte_length))) {
    return SendResult::SENT_SYNCHRONOUSLY;
  }

  // buffer.Slice copies its contents.
  messages_.push_back(MakeGarbageCollected<Message>(
      buffer.Slice(byte_offset, byte_offset + byte_length),
      std::move(completion_callback)));

  ProcessSendQueue();

  // If we managed to flush this message synchronously after all, it would mean
  // that the callback was fired re-entrantly, which would be bad.
  DCHECK(!messages_.empty());

  return SendResult::CALLBACK_WILL_BE_CALLED;
}

void WebSocketChannelImpl::Close(int code, const String& reason) {
  DCHECK_EQ(GetState(), State::kOpen);
  NETWORK_DVLOG(1) << this << " Close(" << code << ", " << reason << ")";
  uint16_t code_to_send = static_cast<uint16_t>(
      code == kCloseEventCodeNotSpecified ? kCloseEventCodeNoStatusRcvd : code);
  messages_.push_back(MakeGarbageCollected<Message>(code_to_send, reason));
  ProcessSendQueue();
}

void WebSocketChannelImpl::Fail(const String& reason,
                                mojom::ConsoleMessageLevel level,
                                std::unique_ptr<SourceLocation> location) {
  NETWORK_DVLOG(1) << this << " Fail(" << reason << ")";
  probe::DidReceiveWebSocketMessageError(execution_context_, identifier_,
                                         reason);
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

  execution_context_->AddConsoleMessage(
      ConsoleMessage::Create(mojom::ConsoleMessageSource::kJavaScript, level,
                             message, std::move(location)));
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
    probe::DidCloseWebSocket(execution_context_, identifier_);
  }

  AbortAsyncOperations();
  Dispose();
}

void WebSocketChannelImpl::CancelHandshake() {
  NETWORK_DVLOG(1) << this << " CancelHandshake()";
  if (GetState() != State::kConnecting)
    return;

  // This may still disconnect even if the handshake is complete if we haven't
  // got the message yet.
  // TODO(ricea): Plumb it through to the network stack to fix the race
  // condition.
  Disconnect();
}

void WebSocketChannelImpl::ApplyBackpressure() {
  backpressure_ = true;
}

void WebSocketChannelImpl::RemoveBackpressure() {
  backpressure_ = false;
  ConsumePendingDataFrames();
}

void WebSocketChannelImpl::OnOpeningHandshakeStarted(
    network::mojom::blink::WebSocketHandshakeRequestPtr request) {
  DCHECK_EQ(GetState(), State::kConnecting);
  NETWORK_DVLOG(1) << this << " OnOpeningHandshakeStarted("
                   << request->url.GetString() << ")";

  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "WebSocketSendHandshakeRequest",
      TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorWebSocketEvent::Data(execution_context_, identifier_));
  probe::WillSendWebSocketHandshakeRequest(execution_context_, identifier_,
                                           request.get());
  handshake_request_ = std::move(request);
}

void WebSocketChannelImpl::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::blink::WebSocket> websocket,
    mojo::PendingReceiver<network::mojom::blink::WebSocketClient>
        client_receiver,
    network::mojom::blink::WebSocketHandshakeResponsePtr response,
    mojo::ScopedDataPipeConsumerHandle readable) {
  DCHECK_EQ(GetState(), State::kConnecting);
  const String& protocol = response->selected_protocol;
  const String& extensions = response->extensions;
  NETWORK_DVLOG(1) << this << " OnConnectionEstablished(" << protocol << ", "
                   << extensions << ")";
  TRACE_EVENT_INSTANT1(
      "devtools.timeline", "WebSocketReceiveHandshakeResponse",
      TRACE_EVENT_SCOPE_THREAD, "data",
      InspectorWebSocketEvent::Data(execution_context_, identifier_));
  probe::DidReceiveWebSocketHandshakeResponse(execution_context_, identifier_,
                                              handshake_request_.get(),
                                              response.get());
  handshake_request_ = nullptr;

  // From now on, we will detect mojo errors via |client_receiver_|.
  handshake_client_receiver_.reset();
  client_receiver_.Bind(
      std::move(client_receiver),
      execution_context_->GetTaskRunner(TaskType::kNetworking));
  client_receiver_.set_disconnect_with_reason_handler(
      WTF::Bind(&WebSocketChannelImpl::OnConnectionError,
                WrapWeakPersistent(this), FROM_HERE));

  DCHECK(!websocket_);
  websocket_.Bind(std::move(websocket),
                  execution_context_->GetTaskRunner(TaskType::kNetworking));
  readable_ = std::move(readable);
  const MojoResult mojo_result = readable_watcher_.Watch(
      readable_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      WTF::BindRepeating(&WebSocketChannelImpl::OnReadable,
                         WrapWeakPersistent(this)));
  DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

  if (!throttle_passed_) {
    connect_info_ = std::make_unique<ConnectInfo>(protocol, extensions);
    return;
  }

  DCHECK_EQ(GetState(), State::kOpen);
  websocket_->StartReceiving();
  handshake_throttle_.reset();
  client_->DidConnect(protocol, extensions);
}

void WebSocketChannelImpl::OnDataFrame(
    bool fin,
    network::mojom::blink::WebSocketMessageType type,
    uint64_t data_length) {
  DCHECK_EQ(GetState(), State::kOpen);
  NETWORK_DVLOG(1) << this << " OnDataFrame(" << fin << ", " << type << ", "
                   << "(data_length = " << data_length << "))";
  pending_data_frames_.push_back(
      DataFrame(fin, type, static_cast<uint32_t>(data_length)));
  ConsumePendingDataFrames();
}

void WebSocketChannelImpl::AddSendFlowControlQuota(int64_t quota) {
  // TODO(yhirano): This should be DCHECK_EQ(GetState(), State::kOpen).
  DCHECK(GetState() == State::kOpen || GetState() == State::kConnecting);
  NETWORK_DVLOG(1) << this << " AddSendFlowControlQuota(" << quota << ")";

  sending_quota_ += quota;
  ProcessSendQueue();
}

void WebSocketChannelImpl::OnDropChannel(bool was_clean,
                                         uint16_t code,
                                         const String& reason) {
  // TODO(yhirano): This should be DCHECK_EQ(GetState(), State::kOpen).
  DCHECK(GetState() == State::kOpen || GetState() == State::kConnecting);
  NETWORK_DVLOG(1) << this << " OnDropChannel(" << was_clean << ", " << code
                   << ", " << reason << ")";

  if (identifier_) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "WebSocketDestroy", TRACE_EVENT_SCOPE_THREAD,
        "data", InspectorWebSocketEvent::Data(execution_context_, identifier_));
    probe::DidCloseWebSocket(execution_context_, identifier_);
    identifier_ = 0;
  }

  HandleDidClose(was_clean, code, reason);
}

void WebSocketChannelImpl::OnClosingHandshake() {
  DCHECK_EQ(GetState(), State::kOpen);
  NETWORK_DVLOG(1) << this << " OnClosingHandshake()";

  client_->DidStartClosingHandshake();
}

ExecutionContext* WebSocketChannelImpl::GetExecutionContext() {
  return execution_context_;
}

void WebSocketChannelImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(blob_loader_);
  visitor->Trace(messages_);
  visitor->Trace(client_);
  visitor->Trace(execution_context_);
  WebSocketChannel::Trace(visitor);
}

WebSocketChannelImpl::Message::Message(const std::string& text,
                                       base::OnceClosure completion_callback)
    : type(kMessageTypeText),
      text(text),
      completion_callback(std::move(completion_callback)) {}

WebSocketChannelImpl::Message::Message(
    scoped_refptr<BlobDataHandle> blob_data_handle)
    : type(kMessageTypeBlob), blob_data_handle(std::move(blob_data_handle)) {}

WebSocketChannelImpl::Message::Message(DOMArrayBuffer* array_buffer,
                                       base::OnceClosure completion_callback)
    : type(kMessageTypeArrayBuffer),
      array_buffer(array_buffer),
      completion_callback(std::move(completion_callback)) {}

WebSocketChannelImpl::Message::Message(uint16_t code, const String& reason)
    : type(kMessageTypeClose), code(code), reason(reason) {}

WebSocketChannelImpl::State WebSocketChannelImpl::GetState() const {
  if (!has_initiated_opening_handshake_) {
    return State::kConnecting;
  }
  if (client_receiver_.is_bound() && throttle_passed_) {
    return State::kOpen;
  }
  if (handshake_client_receiver_.is_bound() || client_receiver_.is_bound()) {
    return State::kConnecting;
  }
  return State::kDisconnected;
}

void WebSocketChannelImpl::SendInternal(
    network::mojom::blink::WebSocketMessageType message_type,
    const char* data,
    wtf_size_t total_size,
    uint64_t* consumed_buffered_amount) {
  network::mojom::blink::WebSocketMessageType frame_type =
      sent_size_of_top_message_
          ? network::mojom::blink::WebSocketMessageType::CONTINUATION
          : message_type;
  DCHECK_GE(total_size, sent_size_of_top_message_);
  // The first cast is safe since the result of min() never exceeds
  // the range of wtf_size_t. The second cast is necessary to compile
  // min() on ILP32.
  wtf_size_t size = static_cast<wtf_size_t>(
      std::min(sending_quota_,
               static_cast<uint64_t>(total_size - sent_size_of_top_message_)));
  bool final = (sent_size_of_top_message_ + size == total_size);

  SendAndAdjustQuota(final, frame_type,
                     base::make_span(data + sent_size_of_top_message_, size),
                     consumed_buffered_amount);

  sent_size_of_top_message_ += size;

  if (final) {
    base::OnceClosure completion_callback =
        std::move(messages_.front()->completion_callback);
    if (!completion_callback.is_null())
      std::move(completion_callback).Run();
    messages_.pop_front();
    sent_size_of_top_message_ = 0;
  }
}

void WebSocketChannelImpl::SendAndAdjustQuota(
    bool fin,
    network::mojom::blink::WebSocketMessageType type,
    base::span<const char> data,
    uint64_t* consumed_buffered_amount) {
  base::span<const uint8_t> data_to_pass(
      reinterpret_cast<const uint8_t*>(data.data()), data.size());
  websocket_->SendFrame(fin, type, data_to_pass);

  sending_quota_ -= data.size();
  *consumed_buffered_amount += data.size();
}

bool WebSocketChannelImpl::MaybeSendSynchronously(
    network::mojom::blink::WebSocketMessageType frame_type,
    base::span<const char> data) {
  DCHECK(messages_.empty());
  if (data.size() > sending_quota_)
    return false;

  uint64_t consumed_buffered_amount = 0;
  SendAndAdjustQuota(true, frame_type, data, &consumed_buffered_amount);
  if (client_ && consumed_buffered_amount > 0)
    client_->DidConsumeBufferedAmount(consumed_buffered_amount);

  return true;
}

void WebSocketChannelImpl::ProcessSendQueue() {
  // TODO(yhirano): This should be DCHECK_EQ(GetState(), State::kOpen).
  DCHECK(GetState() == State::kOpen || GetState() == State::kConnecting);
  uint64_t consumed_buffered_amount = 0;
  while (!messages_.IsEmpty() && !blob_loader_) {
    Message* message = messages_.front().Get();
    CHECK(message);
    if (sending_quota_ == 0 && message->type != kMessageTypeClose)
      break;
    switch (message->type) {
      case kMessageTypeText:
        SendInternal(network::mojom::blink::WebSocketMessageType::TEXT,
                     message->text.data(),
                     static_cast<wtf_size_t>(message->text.length()),
                     &consumed_buffered_amount);
        break;
      case kMessageTypeBlob:
        CHECK(!blob_loader_);
        CHECK(message);
        CHECK(message->blob_data_handle);
        blob_loader_ = MakeGarbageCollected<BlobLoader>(
            message->blob_data_handle, this, file_reading_task_runner_);
        break;
      case kMessageTypeArrayBuffer:
        CHECK(message->array_buffer);
        SendInternal(network::mojom::blink::WebSocketMessageType::BINARY,
                     static_cast<const char*>(message->array_buffer->Data()),
                     message->array_buffer->DeprecatedByteLengthAsUnsigned(),
                     &consumed_buffered_amount);
        break;
      case kMessageTypeClose: {
        // No message should be sent from now on.
        DCHECK_EQ(messages_.size(), 1u);
        DCHECK_EQ(sent_size_of_top_message_, 0u);
        handshake_throttle_.reset();
        websocket_->StartClosingHandshake(
            message->code,
            message->reason.IsNull() ? g_empty_string : message->reason);
        messages_.pop_front();
        break;
      }
    }
  }
  if (client_ && consumed_buffered_amount > 0)
    client_->DidConsumeBufferedAmount(consumed_buffered_amount);
}

void WebSocketChannelImpl::AbortAsyncOperations() {
  if (blob_loader_) {
    blob_loader_->Cancel();
    blob_loader_.Clear();
  }
}

void WebSocketChannelImpl::HandleDidClose(bool was_clean,
                                          uint16_t code,
                                          const String& reason) {
  DCHECK_NE(GetState(), State::kDisconnected);
  WebSocketChannelClient::ClosingHandshakeCompletionStatus status =
      was_clean ? WebSocketChannelClient::kClosingHandshakeComplete
                : WebSocketChannelClient::kClosingHandshakeIncomplete;
  client_->DidClose(status, code, reason);
  AbortAsyncOperations();
  Dispose();
}

void WebSocketChannelImpl::OnCompletion(
    const base::Optional<WebString>& console_message) {
  DCHECK(!throttle_passed_);
  DCHECK(handshake_throttle_);
  handshake_throttle_ = nullptr;

  if (GetState() == State::kDisconnected) {
    return;
  }
  DCHECK_EQ(GetState(), State::kConnecting);
  if (console_message) {
    FailAsError(*console_message);
    return;
  }

  throttle_passed_ = true;
  if (connect_info_) {
    websocket_->StartReceiving();
    client_->DidConnect(std::move(connect_info_->selected_protocol),
                        std::move(connect_info_->extensions));
    connect_info_.reset();
    DCHECK_EQ(GetState(), State::kOpen);
  }
}

void WebSocketChannelImpl::DidFinishLoadingBlob(DOMArrayBuffer* buffer) {
  DCHECK_EQ(GetState(), State::kOpen);

  blob_loader_.Clear();
  // The loaded blob is always placed on |messages_[0]|.
  DCHECK_GT(messages_.size(), 0u);
  DCHECK_EQ(messages_.front()->type, kMessageTypeBlob);
  // We replace it with the loaded blob.
  messages_.front() =
      MakeGarbageCollected<Message>(buffer, base::OnceClosure());
  ProcessSendQueue();
}

void WebSocketChannelImpl::DidFailLoadingBlob(FileErrorCode error_code) {
  DCHECK_EQ(GetState(), State::kOpen);

  blob_loader_.Clear();
  if (error_code == FileErrorCode::kAbortErr) {
    // The error is caused by cancel().
    return;
  }
  // FIXME: Generate human-friendly reason message.
  FailAsError("Failed to load Blob: error code = " +
              String::Number(static_cast<unsigned>(error_code)));
}

void WebSocketChannelImpl::TearDownFailedConnection() {
  if (GetState() == State::kDisconnected) {
    return;
  }
  client_->DidError();
  if (GetState() == State::kDisconnected) {
    return;
  }
  HandleDidClose(false, kCloseEventCodeAbnormalClosure, String());
}

bool WebSocketChannelImpl::ShouldDisallowConnection(const KURL& url) {
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

void WebSocketChannelImpl::OnReadable(MojoResult result,
                                      const mojo::HandleSignalsState& state) {
  DCHECK_EQ(GetState(), State::kOpen);
  NETWORK_DVLOG(2) << this << " OnReadable mojo_result=" << result;
  if (result != MOJO_RESULT_OK) {
    // We don't detect mojo errors on data pipe. Mojo connection errors will
    // be detected via |client_receiver_|.
    return;
  }
  ConsumePendingDataFrames();
}

void WebSocketChannelImpl::ConsumePendingDataFrames() {
  DCHECK_EQ(GetState(), State::kOpen);
  while (!pending_data_frames_.empty() && !backpressure_ &&
         GetState() == State::kOpen) {
    DataFrame& data_frame = pending_data_frames_.front();
    NETWORK_DVLOG(2) << " ConsumePendingDataFrame frame=(" << data_frame.fin
                     << ", " << data_frame.type
                     << ", (data_length = " << data_frame.data_length << "))";
    if (data_frame.data_length == 0) {
      ConsumeDataFrame(data_frame.fin, data_frame.type, nullptr, 0);
      pending_data_frames_.pop_front();
      continue;
    }

    const void* buffer;
    uint32_t readable_size;
    const MojoResult begin_result = readable_->BeginReadData(
        &buffer, &readable_size, MOJO_READ_DATA_FLAG_NONE);
    if (begin_result == MOJO_RESULT_SHOULD_WAIT) {
      readable_watcher_.ArmOrNotify();
      return;
    }
    if (begin_result == MOJO_RESULT_FAILED_PRECONDITION) {
      // |client_receiver_| will catch the connection error.
      return;
    }
    DCHECK_EQ(begin_result, MOJO_RESULT_OK);

    if (readable_size >= data_frame.data_length) {
      ConsumeDataFrame(data_frame.fin, data_frame.type,
                       static_cast<const char*>(buffer),
                       data_frame.data_length);
      const MojoResult end_result =
          readable_->EndReadData(data_frame.data_length);
      DCHECK_EQ(end_result, MOJO_RESULT_OK);
      pending_data_frames_.pop_front();
      continue;
    }

    DCHECK_LT(readable_size, data_frame.data_length);
    ConsumeDataFrame(false, data_frame.type, static_cast<const char*>(buffer),
                     readable_size);
    const MojoResult end_result = readable_->EndReadData(readable_size);
    DCHECK_EQ(end_result, MOJO_RESULT_OK);
    data_frame.type = network::mojom::blink::WebSocketMessageType::CONTINUATION;
    data_frame.data_length -= readable_size;
  }
}

void WebSocketChannelImpl::ConsumeDataFrame(
    bool fin,
    network::mojom::blink::WebSocketMessageType type,
    const char* data,
    size_t size) {
  DCHECK_EQ(GetState(), State::kOpen);
  DCHECK(!backpressure_);
  // Non-final frames cannot be empty.
  DCHECK(fin || size > 0);

  switch (type) {
    case network::mojom::blink::WebSocketMessageType::CONTINUATION:
      break;
    case network::mojom::blink::WebSocketMessageType::TEXT:
      DCHECK_EQ(message_chunks_.GetSize(), 0u);
      receiving_message_type_is_text_ = true;
      break;
    case network::mojom::blink::WebSocketMessageType::BINARY:
      DCHECK_EQ(message_chunks_.GetSize(), 0u);
      receiving_message_type_is_text_ = false;
      break;
  }

  const size_t message_size_so_far = message_chunks_.GetSize();
  if (message_size_so_far > std::numeric_limits<wtf_size_t>::max()) {
    message_chunks_.Clear();
    FailAsError("Message size is too large.");
    return;
  }

  // TODO(yoichio): Do this after EndReadData by reading |message_chunks_|
  // instead.
  if (receiving_message_type_is_text_ && received_text_is_all_ascii_) {
    for (size_t i = 0; i < size; i++) {
      if (!IsASCII(data[i])) {
        received_text_is_all_ascii_ = false;
        break;
      }
    }
  }

  if (!fin) {
    message_chunks_.Append(base::make_span(data, size));
    return;
  }

  Vector<base::span<const char>> chunks = message_chunks_.GetView();
  if (size > 0) {
    chunks.push_back(base::make_span(data, size));
  }
  auto opcode = receiving_message_type_is_text_
                    ? WebSocketOpCode::kOpCodeText
                    : WebSocketOpCode::kOpCodeBinary;
  probe::DidReceiveWebSocketMessage(execution_context_, identifier_, opcode,
                                    false, chunks);

  if (receiving_message_type_is_text_) {
    String message = GetTextMessage(
        chunks, static_cast<wtf_size_t>(message_size_so_far + size));
    if (message.IsNull()) {
      FailAsError("Could not decode a text frame as UTF-8.");
    } else {
      client_->DidReceiveTextMessage(message);
    }
  } else {
    client_->DidReceiveBinaryMessage(chunks);
  }
  message_chunks_.Clear();
  received_text_is_all_ascii_ = true;
}

String WebSocketChannelImpl::GetTextMessage(
    const Vector<base::span<const char>>& chunks,
    wtf_size_t size) {
  DCHECK(receiving_message_type_is_text_);

  if (size == 0) {
    return g_empty_string;
  }

  // We can skip UTF8 encoding if received text contains only ASCII.
  // We do this in order to avoid constructing a temporary buffer.
  if (received_text_is_all_ascii_) {
    LChar* buffer;
    scoped_refptr<StringImpl> string_impl =
        StringImpl::CreateUninitialized(size, buffer);
    size_t index = 0;
    for (const auto& chunk : chunks) {
      DCHECK_LE(index + chunk.size(), size);
      memcpy(buffer + index, chunk.data(), chunk.size());
      index += chunk.size();
    }
    DCHECK_EQ(index, size);
    return String(std::move(string_impl));
  }

  Vector<char> flatten;
  base::span<const char> span;
  if (chunks.size() > 1) {
    flatten.ReserveCapacity(size);
    for (const auto& chunk : chunks) {
      flatten.Append(chunk.data(), static_cast<wtf_size_t>(chunk.size()));
    }
    span = base::make_span(flatten.data(), flatten.size());
  } else if (chunks.size() == 1) {
    span = chunks[0];
  }
  DCHECK_EQ(span.size(), size);
  return String::FromUTF8(span.data(), span.size());
}

void WebSocketChannelImpl::OnConnectionError(const base::Location& set_from,
                                             uint32_t custom_reason,
                                             const std::string& description) {
  DCHECK_NE(GetState(), State::kDisconnected);
  NETWORK_DVLOG(1) << " OnConnectionError("
                   << " reason: " << custom_reason
                   << ", description:" << description
                   << "), set_from:" << set_from.ToString();
  String message = "Unknown reason";
  if (custom_reason == network::mojom::blink::WebSocket::kInternalFailure) {
    message = String::FromUTF8(description.c_str(), description.size());
  }

  // This function is called when the implementation in the network service is
  // required to fail the WebSocket connection. Hence we fail this channel by
  // calling FailAsError function.
  FailAsError(message);
}

void WebSocketChannelImpl::Dispose() {
  has_initiated_opening_handshake_ = true;
  feature_handle_for_scheduler_.reset();
  handshake_throttle_.reset();
  websocket_.reset();
  readable_watcher_.Cancel();
  handshake_client_receiver_.reset();
  client_receiver_.reset();
  identifier_ = 0;
}

std::ostream& operator<<(std::ostream& ostream,
                         const WebSocketChannelImpl* channel) {
  return ostream << "WebSocketChannelImpl "
                 << static_cast<const void*>(channel);
}

}  // namespace blink
