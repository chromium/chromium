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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/websockets/websocket_channel_impl.h"

#include <string.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/strong_alias.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/websocket_handshake_throttle.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/base_fetch_context.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/websockets/inspector_websocket_events.h"
#include "third_party/blink/renderer/modules/websockets/websocket_channel_client.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// The number of connecting or connected WebSocketChannelImpl objects that
// currently exist. This needs to be threadsafe because there may also be
// Workers in the same process. This is default-initialised to 0 because it has
// static storage.
std::atomic_size_t g_connection_count;

enum WebSocketOpCode {
  kOpCodeText = 0x1,
  kOpCodeBinary = 0x2,
};

}  // namespace

WebSocketChannelImpl::MessageDataDeleter::MessageDataDeleter(
    v8::Isolate* isolate,
    size_t size)
    : isolate_(isolate), size_(size) {
  external_memory_accounter_.Increase(isolate, size);
}

void WebSocketChannelImpl::MessageDataDeleter::operator()(char* p) const {
  DCHECK(isolate_) << "Cannot call deleter when default constructor was used";
  external_memory_accounter_.Decrease(isolate_.get(), size_);
  WTF::Partitions::FastFree(p);
}

// static
WebSocketChannelImpl::MessageData WebSocketChannelImpl::CreateMessageData(
    v8::Isolate* isolate,
    size_t message_size) {
  return MessageData(
      static_cast<char*>(WTF::Partitions::FastMalloc(
          message_size, "blink::WebSockChannelImpl::MessageData")),
      MessageDataDeleter(isolate, message_size));
}

class WebSocketChannelImpl::BlobLoader final
    : public GarbageCollected<WebSocketChannelImpl::BlobLoader>,
      public FileReaderClient {
 public:
  BlobLoader(scoped_refptr<BlobDataHandle>,
             WebSocketChannelImpl*,
             scoped_refptr<base::SingleThreadTaskRunner>);
  ~BlobLoader() override = default;

  void Cancel();

  // FileReaderClient functions.
  FileErrorCode DidStartLoading(uint64_t) override;
  FileErrorCode DidReceiveData(base::span<const uint8_t> data) override;
  void DidFinishLoading() override;
  void DidFail(FileErrorCode) override;

  void Trace(Visitor* visitor) const override {
    FileReaderClient::Trace(visitor);
    visitor->Trace(channel_);
    visitor->Trace(loader_);
  }

 private:
  Member<WebSocketChannelImpl> channel_;
  Member<FileReaderLoader> loader_;
  // This doesn't use WTF::Vector because it doesn't currently support 64-bit
  // sizes.
  MessageData data_;
  size_t size_ = 0;
  size_t offset_ = 0;

  bool blob_too_large_ = false;
};

WebSocketChannelImpl::BlobLoader::BlobLoader(
    scoped_refptr<BlobDataHandle> blob_data_handle,
    WebSocketChannelImpl* channel,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : channel_(channel),
      loader_(MakeGarbageCollected<FileReaderLoader>(this,
                                                     std::move(task_runner))) {
  loader_->Start(std::move(blob_data_handle));
}

void WebSocketChannelImpl::BlobLoader::Cancel() {
  loader_->Cancel();
  loader_ = nullptr;
  data_ = nullptr;
}

FileErrorCode WebSocketChannelImpl::BlobLoader::DidStartLoading(uint64_t) {
  const std::optional<uint64_t> size = loader_->TotalBytes();
  DCHECK(size);
  if (size.value() > std::numeric_limits<size_t>::max()) {
    blob_too_large_ = true;
    return FileErrorCode::kAbortErr;
  }
  size_ = static_cast<size_t>(size.value());
  data_ = WebSocketChannelImpl::CreateMessageData(
      channel_->execution_context_->GetIsolate(), size_);
  return FileErrorCode::kOK;
}

FileErrorCode WebSocketChannelImpl::BlobLoader::DidReceiveData(
    base::span<const uint8_t> data) {
  auto remaining_message = base::span(data_.get(), size_).subspan(offset_);
  const size_t data_to_copy = std::min(remaining_message.size(), data.size());
  if (!data_to_copy) {
    return FileErrorCode::kOK;
  }
  remaining_message.copy_prefix_from(base::as_chars(data.first(data_to_copy)));
  offset_ += data_to_copy;
  return FileErrorCode::kOK;
}

void WebSocketChannelImpl::BlobLoader::DidFinishLoading() {
  // This is guaranteed by FileReaderLoader::OnDataPipeReadable.
  DCHECK_EQ(offset_, size_);
  channel_->DidFinishLoadingBlob(std::move(data_), size_);
  loader_ = nullptr;
}

void WebSocketChannelImpl::BlobLoader::DidFail(FileErrorCode error_code) {
  if (error_code == FileErrorCode::kAbortErr && blob_too_large_) {
    blob_too_large_ = false;
    channel_->BlobTooLarge();
  }
  channel_->DidFailLoadingBlob(error_code);
  loader_ = nullptr;
  data_ = nullptr;
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
      message_chunks_(MakeGarbageCollected<WebSocketMessageChunkAccumulator>(
          execution_context->GetTaskRunner(TaskType::kNetworking))),
      execution_context_(execution_context),
      location_at_construction_(std::move(location)),
      websocket_(execution_context),
      handshake_client_receiver_(this, execution_context),
      client_receiver_(this, execution_context),
      readable_watcher_(
          FROM_HERE,
          mojo::SimpleWatcher::ArmingPolicy::MANUAL,
          execution_context->GetTaskRunner(TaskType::kNetworking)),
      writable_watcher_(
          FROM_HERE,
          mojo::SimpleWatcher::ArmingPolicy::MANUAL,
          execution_context->GetTaskRunner(TaskType::kNetworking)),
      file_reading_task_runner_(
          execution_context->GetTaskRunner(TaskType::kFileReading)) {}

WebSocketChannelImpl::~WebSocketChannelImpl() = default;

bool WebSocketChannelImpl::Connect(const KURL& url, const String& protocol) {
  DVLOG(1) << this << " Connect()";

  if (GetBaseFetchContext()->ShouldBlockWebSocketByMixedContentCheck(url)) {
    has_initiated_opening_handshake_ = false;
    return false;
  }

  if (auto* scheduler = execution_context_->GetScheduler()) {
    // Two features are registered here:
    // - `kWebSocket`: a non-sticky feature that will disable BFCache for any
    // page. It will be reset after the `WebSocketChannel` is closed.
    // - `kWebSocketSticky`: a sticky feature that will only disable BFCache for
    // the page containing "Cache-Control: no-store" header. It won't be reset
    // even if the `WebSocketChannel` is closed.
    feature_handle_for_scheduler_ = scheduler->RegisterFeature(
        SchedulingPolicy::Feature::kWebSocket,
        SchedulingPolicy{SchedulingPolicy::DisableBackForwardCache()});
    scheduler->RegisterStickyFeature(
        SchedulingPolicy::Feature::kWebSocketSticky,
        SchedulingPolicy{SchedulingPolicy::DisableBackForwardCache()});
  }

  if (MixedContentChecker::IsMixedContent(
          execution_context_->GetSecurityOrigin(), url)) {
    String message =
        "Connecting to a non-secure WebSocket server from a secure origin is "
        "deprecated.";
    execution_context_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning, message));
  }

  url_ = url;
  Vector<String> protocols;
  // Avoid placing an empty token in the Vector when the protocol string is
  // empty.
  if (!protocol.empty()) {
    // Since protocol is already verified and escaped, we can simply split
    // it.
    protocol.Split(", ", true, protocols);
  }

  // If the connection needs to be filtered, asynchronously fail. Synchronous
  // failure blocks the worker thread which should be avoided. Note that
  // returning "true" just indicates that this was not a mixed content error.
  if (ShouldDisallowConnection(url)) {
    execution_context_->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&WebSocketChannelImpl::TearDownFailedConnection,
                          WrapPersistent(this)));
    return true;
  }

  // Restrict the number of simultaneous connections to avoid a DoS attack on
  // the browser process. Fail asynchronously, to match the behaviour when we
  // are throttled by the network service.
  if (connection_count_tracker_handle_.IncrementAndCheckStatus() ==
      ConnectionCountTrackerHandle::CountStatus::kShouldNotConnect) {
    StringBuilder message;
    message.Append("WebSocket connection to '");
    message.Append(url.GetString());
    message.Append("' failed: Insufficient resources");
    execution_context_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kNetwork,
        mojom::blink::ConsoleMessageLevel::kError, message.ToString()));
    execution_context_->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&WebSocketChannelImpl::TearDownFailedConnection,
                          WrapPersistent(this)));
    return true;
  }

  mojo::Remote<mojom::blink::WebSocketConnector> connector;
  execution_context_->GetBrowserInterfaceBroker().GetInterface(
      connector.BindNewPipeAndPassReceiver(
          execution_context_->GetTaskRunner(TaskType::kWebSocket)));

  std::optional<base::UnguessableToken> devtools_token;
  probe::WillCreateWebSocket(execution_context_, identifier_, url, protocol,
                             &devtools_token);

  connector->Connect(
      url, protocols, GetBaseFetchContext()->GetSiteForCookies(),
      execution_context_->UserAgent(),
      execution_context_->GetStorageAccessApiStatus(),
      handshake_client_receiver_.BindNewPipeAndPassRemote(
          execution_context_->GetTaskRunner(TaskType::kWebSocket)),
      /*throttling_profile_id=*/devtools_token);
  handshake_client_receiver_.set_disconnect_with_reason_handler(
      WTF::BindOnce(&WebSocketChannelImpl::OnConnectionError,
                    WrapWeakPersistent(this), FROM_HERE));
  has_initiated_opening_handshake_ = true;

  if (handshake_throttle_) {
    scoped_refptr<const SecurityOrigin> isolated_security_origin;
    const DOMWrapperWorld* world = execution_context_->GetCurrentWorld();
    // TODO(crbug.com/40511450): Current world can be null because of PPAPI.
    // Null check can be cleaned up once PPAPI support is removed.
    if (world && world->IsIsolatedWorld()) {
      isolated_security_origin = world->IsolatedWorldSecurityOrigin(
          execution_context_->GetAgentClusterID());
    }
    // The use of WrapWeakPersistent is safe and motivated by the fact that if
    // the WebSocket is no longer referenced, there's no point in keeping it
    // alive just to receive the throttling result.
    handshake_throttle_->ThrottleHandshake(
        url, WebSecurityOrigin(execution_context_->GetSecurityOrigin()),
        isolated_security_origin ? WebSecurityOrigin(isolated_security_origin)
                                 : WebSecurityOrigin(),
        WTF::BindOnce(&WebSocketChannelImpl::OnCompletion,
                      WrapWeakPersistent(this)));
  } else {
    // Treat no throttle as success.
    throttle_passed_ = true;
  }

  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "WebSocketCreate", InspectorWebSocketCreateEvent::Data,
      execution_context_.Get(), identifier_, url, protocol);
  return true;
}

WebSocketChannel::SendResult WebSocketChannelImpl::Send(
    const std::string& message,
    base::OnceClosure completion_callback) {
  DVLOG(1) << this << " Send(" << message << ") (std::string argument)";
  probe::DidSendWebSocketMessage(execution_context_, identifier_,
                                 WebSocketOpCode::kOpCodeText, true, message);
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
    "WebSocketSend", InspectorWebSocketTransferEvent::Data,
    execution_context_.Get(), identifier_, message.length());

  bool did_attempt_to_send = false;
  base::span<const char> data = message;
  if (messages_.empty() && !wait_for_writable_) {
    did_attempt_to_send = true;
    if (MaybeSendSynchronously(
            network::mojom::blink::WebSocketMessageType::TEXT, &data)) {
      return SendResult::kSentSynchronously;
    }
  }

  messages_.push_back(
      Message(execution_context_->GetIsolate(),
              message.substr(message.size() - data.size(), data.size()),
              std::move(completion_callback),
              Message::DidCallSendMessage(did_attempt_to_send)));

  // ProcessSendQueue() will do nothing when MaybeSendSynchronously() is called.
  ProcessSendQueue();

  // If we managed to flush this message synchronously after all, it would mean
  // that the callback was fired re-entrantly, which would be bad.
  DCHECK(!messages_.empty());

  return SendResult::kCallbackWillBeCalled;
}

void WebSocketChannelImpl::Send(
    scoped_refptr<BlobDataHandle> blob_data_handle) {
  DVLOG(1) << this << " Send(" << blob_data_handle->Uuid() << ", "
           << blob_data_handle->GetType() << ", " << blob_data_handle->size()
           << ") "
           << "(BlobDataHandle argument)";
  // FIXME: We can't access the data here.
  // Since Binary data are not displayed in Inspector, this does not
  // affect actual behavior.
  probe::DidSendWebSocketMessage(execution_context_, identifier_,
                                 WebSocketOpCode::kOpCodeBinary, true,
                                 base::span_from_cstring(""));
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
    "WebSocketSend", InspectorWebSocketTransferEvent::Data,
    execution_context_.Get(), identifier_, blob_data_handle->size());
  messages_.push_back(Message(std::move(blob_data_handle)));
  ProcessSendQueue();
}

WebSocketChannel::SendResult WebSocketChannelImpl::Send(
    const DOMArrayBuffer& buffer,
    size_t byte_offset,
    size_t byte_length,
    base::OnceClosure completion_callback) {
  DVLOG(1) << this << " Send(" << buffer.Data() << ", " << byte_offset << ", "
           << byte_length << ") "
           << "(DOMArrayBuffer argument)";
  probe::DidSendWebSocketMessage(
      execution_context_, identifier_, WebSocketOpCode::kOpCodeBinary, true,
      base::as_chars(buffer.ByteSpan().subspan(byte_offset, byte_length)));
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
    "WebSocketSend", InspectorWebSocketTransferEvent::Data,
    execution_context_.Get(), identifier_, byte_length);
  bool did_attempt_to_send = false;
  base::span<const char> message = base::make_span(
      static_cast<const char*>(buffer.Data()) + byte_offset, byte_length);
  if (messages_.empty() && !wait_for_writable_) {
    did_attempt_to_send = true;
    if (MaybeSendSynchronously(
            network::mojom::blink::WebSocketMessageType::BINARY, &message)) {
      return SendResult::kSentSynchronously;
    }
  }

  messages_.push_back(Message(
      execution_context_->GetIsolate(), message, std::move(completion_callback),
      Message::DidCallSendMessage(did_attempt_to_send)));

  // ProcessSendQueue() will do nothing when MaybeSendSynchronously() is called.
  ProcessSendQueue();

  // If we managed to flush this message synchronously after all, it would mean
  // that the callback was fired re-entrantly, which would be bad.
  DCHECK(!messages_.empty());

  return SendResult::kCallbackWillBeCalled;
}

void WebSocketChannelImpl::Close(int code, const String& reason) {
  DCHECK_EQ(GetState(), State::kOpen);
  DCHECK(!execution_context_->IsContextDestroyed());
  DVLOG(1) << this << " Close(" << code << ", " << reason << ")";
  uint16_t code_to_send = static_cast<uint16_t>(
      code == kCloseEventCodeNotSpecified ? kCloseEventCodeNoStatusRcvd : code);
  messages_.push_back(Message(code_to_send, reason));
  ProcessSendQueue();
  // Make the page back/forward cache-able.
  feature_handle_for_scheduler_.reset();
}

void WebSocketChannelImpl::Fail(const String& reason,
                                mojom::ConsoleMessageLevel level,
                                std::unique_ptr<SourceLocation> location) {
  DVLOG(1) << this << " Fail(" << reason << ")";
  probe::DidReceiveWebSocketMessageError(execution_context_, identifier_,
                                         reason);
  const String message =
      "WebSocket connection to '" + url_.ElidedString() + "' failed: " + reason;

  std::unique_ptr<SourceLocation> captured_location = CaptureSourceLocation();
  if (!captured_location->IsUnknown()) {
    // If we are in JavaScript context, use the current location instead
    // of passed one - it's more precise.
    location = std::move(captured_location);
  } else if (location->IsUnknown()) {
    // No information is specified by the caller. Use the line number at the
    // connection.
    location = location_at_construction_->Clone();
  }

  execution_context_->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kNetwork, level, message,
      std::move(location)));
  // |reason| is only for logging and should not be provided for scripts,
  // hence close reason must be empty in tearDownFailedConnection.
  execution_context_->GetTaskRunner(TaskType::kNetworking)
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(&WebSocketChannelImpl::TearDownFailedConnection,
                               WrapPersistent(this)));
}

void WebSocketChannelImpl::Disconnect() {
  DVLOG(1) << this << " disconnect()";
  if (identifier_) {
    DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
        "WebSocketDestroy", InspectorWebSocketEvent::Data,
        execution_context_.Get(), identifier_);
    probe::DidCloseWebSocket(execution_context_.Get(), identifier_);
  }

  AbortAsyncOperations();
  Dispose();
}

void WebSocketChannelImpl::CancelHandshake() {
  DVLOG(1) << this << " CancelHandshake()";
  if (GetState() != State::kConnecting)
    return;

  // This may still disconnect even if the handshake is complete if we haven't
  // got the message yet.
  // TODO(ricea): Plumb it through to the network stack to fix the race
  // condition.
  Disconnect();
}

void WebSocketChannelImpl::ApplyBackpressure() {
  DVLOG(1) << this << " ApplyBackpressure";
  backpressure_ = true;
}

void WebSocketChannelImpl::RemoveBackpressure() {
  DVLOG(1) << this << " RemoveBackpressure";
  if (backpressure_) {
    backpressure_ = false;
    ConsumePendingDataFrames();
  }
}

void WebSocketChannelImpl::OnOpeningHandshakeStarted(
    network::mojom::blink::WebSocketHandshakeRequestPtr request) {
  DCHECK_EQ(GetState(), State::kConnecting);
  DVLOG(1) << this << " OnOpeningHandshakeStarted(" << request->url.GetString()
           << ")";

  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT("WebSocketSendHandshakeRequest",
                                        InspectorWebSocketEvent::Data,
                                        execution_context_, identifier_);
  probe::WillSendWebSocketHandshakeRequest(execution_context_, identifier_,
                                           request.get());
  handshake_request_ = std::move(request);
}

void WebSocketChannelImpl::OnFailure(const WTF::String& message,
                                     int net_error,
                                     int response_code) {
  DVLOG(1) << this << " OnFailure(" << message << ", " << net_error << ", "
           << response_code << ")";
  failure_message_ = message;
}

void WebSocketChannelImpl::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::blink::WebSocket> websocket,
    mojo::PendingReceiver<network::mojom::blink::WebSocketClient>
        client_receiver,
    network::mojom::blink::WebSocketHandshakeResponsePtr response,
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable) {
  DCHECK_EQ(GetState(), State::kConnecting);
  const String& protocol = response->selected_protocol;
  const String& extensions = response->extensions;
  DVLOG(1) << this << " OnConnectionEstablished(" << protocol << ", "
           << extensions << ")";
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT("WebSocketReceiveHandshakeResponse",
                                        InspectorWebSocketEvent::Data,
                                        execution_context_, identifier_);
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
      WTF::BindOnce(&WebSocketChannelImpl::OnConnectionError,
                    WrapWeakPersistent(this), FROM_HERE));

  DCHECK(!websocket_.is_bound());
  websocket_.Bind(std::move(websocket),
                  execution_context_->GetTaskRunner(TaskType::kNetworking));
  readable_ = std::move(readable);
  // TODO(suzukikeita): Implement upload via |writable_| instead of SendFrame.
  writable_ = std::move(writable);
  const MojoResult mojo_result = readable_watcher_.Watch(
      readable_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      WTF::BindRepeating(&WebSocketChannelImpl::OnReadable,
                         WrapWeakPersistent(this)));
  DCHECK_EQ(mojo_result, MOJO_RESULT_OK);

  const MojoResult mojo_writable_result = writable_watcher_.Watch(
      writable_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      WTF::BindRepeating(&WebSocketChannelImpl::OnWritable,
                         WrapWeakPersistent(this)));
  DCHECK_EQ(mojo_writable_result, MOJO_RESULT_OK);

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
  DVLOG(1) << this << " OnDataFrame(" << fin << ", " << type << ", "
           << "(data_length = " << data_length << "))";
  pending_data_frames_.push_back(
      DataFrame(fin, type, static_cast<uint32_t>(data_length)));
  ConsumePendingDataFrames();
}

void WebSocketChannelImpl::OnDropChannel(bool was_clean,
                                         uint16_t code,
                                         const String& reason) {
  // TODO(yhirano): This should be DCHECK_EQ(GetState(), State::kOpen).
  DCHECK(GetState() == State::kOpen || GetState() == State::kConnecting);
  DVLOG(1) << this << " OnDropChannel(" << was_clean << ", " << code << ", "
           << reason << ")";

  if (identifier_) {
    DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT("WebSocketDestroy",
                                          InspectorWebSocketEvent::Data,
                                          execution_context_, identifier_);
    probe::DidCloseWebSocket(execution_context_, identifier_);
    identifier_ = 0;
  }

  HandleDidClose(was_clean, code, reason);
}

void WebSocketChannelImpl::OnClosingHandshake() {
  DCHECK_EQ(GetState(), State::kOpen);
  DVLOG(1) << this << " OnClosingHandshake()";

  client_->DidStartClosingHandshake();
}

void WebSocketChannelImpl::Trace(Visitor* visitor) const {
  visitor->Trace(blob_loader_);
  visitor->Trace(client_);
  visitor->Trace(execution_context_);
  visitor->Trace(websocket_);
  visitor->Trace(handshake_client_receiver_);
  visitor->Trace(client_receiver_);
  visitor->Trace(message_chunks_);
  WebSocketChannel::Trace(visitor);
}

WebSocketChannelImpl::Message::Message(v8::Isolate* isolate,
                                       const std::string& text,
                                       base::OnceClosure completion_callback,
                                       DidCallSendMessage did_call_send_message)
    : message_data_(
          WebSocketChannelImpl::CreateMessageData(isolate, text.length())),
      type_(kMessageTypeText),
      did_call_send_message_(did_call_send_message),
      completion_callback_(std::move(completion_callback)) {
  memcpy(message_data_.get(), text.data(), text.length());
  pending_payload_ = base::make_span(message_data_.get(), text.length());
}

WebSocketChannelImpl::Message::Message(
    scoped_refptr<BlobDataHandle> blob_data_handle)
    : type_(kMessageTypeBlob), blob_data_handle_(std::move(blob_data_handle)) {}

WebSocketChannelImpl::Message::Message(MessageData data, size_t size)
    : message_data_(std::move(data)),
      type_(kMessageTypeArrayBuffer),
      pending_payload_(base::make_span(message_data_.get(), size)) {}

WebSocketChannelImpl::Message::Message(v8::Isolate* isolate,
                                       base::span<const char> message,
                                       base::OnceClosure completion_callback,
                                       DidCallSendMessage did_call_send_message)
    : message_data_(CreateMessageData(isolate, message.size())),
      type_(kMessageTypeArrayBuffer),
      did_call_send_message_(did_call_send_message),
      completion_callback_(std::move(completion_callback)) {
  memcpy(message_data_.get(), message.data(), message.size());
  pending_payload_ = base::make_span(message_data_.get(), message.size());
}

WebSocketChannelImpl::Message::Message(uint16_t code, const String& reason)
    : type_(kMessageTypeClose), code_(code), reason_(reason) {}

WebSocketChannelImpl::Message::Message(MessageType type,
                                       base::span<const char> pending_payload,
                                       base::OnceClosure completion_callback)
    : type_(type),
      pending_payload_(pending_payload),
      completion_callback_(std::move(completion_callback)) {}

WebSocketChannelImpl::Message::Message(Message&&) = default;

WebSocketChannelImpl::Message& WebSocketChannelImpl::Message::operator=(
    Message&&) = default;

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

WebSocketChannelImpl::MessageType WebSocketChannelImpl::Message::Type() const {
  return type_;
}

scoped_refptr<BlobDataHandle>
WebSocketChannelImpl::Message::GetBlobDataHandle() {
  return blob_data_handle_;
}

base::span<const char>& WebSocketChannelImpl::Message::MutablePendingPayload() {
  return pending_payload_;
}

WebSocketChannelImpl::Message::DidCallSendMessage
WebSocketChannelImpl::Message::GetDidCallSendMessage() const {
  return did_call_send_message_;
}

void WebSocketChannelImpl::Message::SetDidCallSendMessage(
    WebSocketChannelImpl::Message::DidCallSendMessage did_call_send_message) {
  did_call_send_message_ = did_call_send_message;
}

uint16_t WebSocketChannelImpl::Message::Code() const {
  return code_;
}

String WebSocketChannelImpl::Message::Reason() const {
  return reason_;
}

base::OnceClosure WebSocketChannelImpl::Message::CompletionCallback() {
  return std::move(completion_callback_);
}

// This could be done directly in WebSocketChannelImpl, but is a separate class
// to make it easier to verify correctness.
WebSocketChannelImpl::ConnectionCountTrackerHandle::CountStatus
WebSocketChannelImpl::ConnectionCountTrackerHandle::IncrementAndCheckStatus() {
  DCHECK(!incremented_);
  incremented_ = true;
  const size_t old_count =
      g_connection_count.fetch_add(1, std::memory_order_relaxed);
  return old_count >= kMaxWebSocketsPerRenderProcess
             ? CountStatus::kShouldNotConnect
             : CountStatus::kOkayToConnect;
}

void WebSocketChannelImpl::ConnectionCountTrackerHandle::Decrement() {
  if (incremented_) {
    incremented_ = false;
    const size_t old_count =
        g_connection_count.fetch_sub(1, std::memory_order_relaxed);
    DCHECK_NE(old_count, 0u);
  }
}

bool WebSocketChannelImpl::MaybeSendSynchronously(
    network::mojom::blink::WebSocketMessageType frame_type,
    base::span<const char>* data) {
  DCHECK(messages_.empty());
  DCHECK(!wait_for_writable_);

  websocket_->SendMessage(frame_type, data->size());
  return SendMessageData(data);
}

void WebSocketChannelImpl::ProcessSendQueue() {
  // TODO(yhirano): This should be DCHECK_EQ(GetState(), State::kOpen).
  DCHECK(GetState() == State::kOpen || GetState() == State::kConnecting);
  DCHECK(!execution_context_->IsContextDestroyed());
  while (!messages_.empty() && !blob_loader_ && !wait_for_writable_) {
    Message& message = messages_.front();
    network::mojom::blink::WebSocketMessageType message_type =
        network::mojom::blink::WebSocketMessageType::BINARY;
    switch (message.Type()) {
      case kMessageTypeText:
        message_type = network::mojom::blink::WebSocketMessageType::TEXT;
        [[fallthrough]];
      case kMessageTypeArrayBuffer: {
        base::span<const char>& data_frame = message.MutablePendingPayload();
        if (!message.GetDidCallSendMessage()) {
          websocket_->SendMessage(message_type, data_frame.size());
          message.SetDidCallSendMessage(Message::DidCallSendMessage(true));
        }
        if (!SendMessageData(&data_frame))
          return;
        base::OnceClosure completion_callback =
            messages_.front().CompletionCallback();
        if (!completion_callback.is_null())
          std::move(completion_callback).Run();
        messages_.pop_front();
        break;
      }
      case kMessageTypeBlob:
        CHECK(!blob_loader_);
        CHECK(message.GetBlobDataHandle());
        blob_loader_ = MakeGarbageCollected<BlobLoader>(
            message.GetBlobDataHandle(), this, file_reading_task_runner_);
        break;
      case kMessageTypeClose: {
        // No message should be sent from now on.
        DCHECK_EQ(messages_.size(), 1u);
        DCHECK_EQ(sent_size_of_top_message_, 0u);
        handshake_throttle_.reset();
        websocket_->StartClosingHandshake(
            message.Code(),
            message.Reason().IsNull() ? g_empty_string : message.Reason());
        messages_.pop_front();
        break;
      }
    }
  }
}

bool WebSocketChannelImpl::SendMessageData(base::span<const char>* data) {
  if (data->size() > 0) {
    uint64_t consumed_buffered_amount = 0;
    ProduceData(data, &consumed_buffered_amount);
    if (client_ && consumed_buffered_amount > 0)
      client_->DidConsumeBufferedAmount(consumed_buffered_amount);
    if (data->size() > 0) {
      // The |writable_| datapipe is full.
      wait_for_writable_ = true;
      if (writable_) {
        writable_watcher_.ArmOrNotify();
      } else {
        // This is to maintain backwards compatibility with the legacy
        // code, where it requires Send to be complete even if the
        // datapipe is closed. To overcome this, call
        // DidConsumeBufferedAmount() and ack as the message is correctly
        // passed on to the network service.
        //
        // The corresponding bug for this is
        // https://bugs.chromium.org/p/chromium/issues/detail?id=937790
        // The corresponding test case is
        // browser_tests WebRequestApiTest.WebSocketCleanClose.
        if (client_) {
          client_->DidConsumeBufferedAmount(data->size());
        }
      }
      return false;
    }
  }
  return true;
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
    const std::optional<WebString>& console_message) {
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

void WebSocketChannelImpl::DidFinishLoadingBlob(MessageData data, size_t size) {
  DCHECK_EQ(GetState(), State::kOpen);

  blob_loader_.Clear();
  // The loaded blob is always placed on |messages_[0]|.
  DCHECK_GT(messages_.size(), 0u);
  DCHECK_EQ(messages_.front().Type(), kMessageTypeBlob);

  // We replace it with the loaded blob.
  messages_.front() = Message(std::move(data), size);

  ProcessSendQueue();
}

void WebSocketChannelImpl::BlobTooLarge() {
  DCHECK_EQ(GetState(), State::kOpen);

  blob_loader_.Clear();

  FailAsError("Blob too large: cannot load into memory");
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
  DVLOG(2) << this << " OnReadable mojo_result=" << result;
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
    DVLOG(2) << " ConsumePendingDataFrame frame=(" << data_frame.fin << ", "
             << data_frame.type << ", (data_length = " << data_frame.data_length
             << "))";
    if (data_frame.data_length == 0) {
      ConsumeDataFrame(data_frame.fin, data_frame.type, nullptr, 0);
      pending_data_frames_.pop_front();
      continue;
    }

    base::span<const uint8_t> buffer;
    const MojoResult begin_result =
        readable_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    if (begin_result == MOJO_RESULT_SHOULD_WAIT) {
      readable_watcher_.ArmOrNotify();
      return;
    }
    if (begin_result == MOJO_RESULT_FAILED_PRECONDITION) {
      // |client_receiver_| will catch the connection error.
      return;
    }
    DCHECK_EQ(begin_result, MOJO_RESULT_OK);

    std::string_view chars = base::as_string_view(buffer);
    if (buffer.size() >= data_frame.data_length) {
      ConsumeDataFrame(data_frame.fin, data_frame.type, chars.data(),
                       data_frame.data_length);
      const MojoResult end_result =
          readable_->EndReadData(data_frame.data_length);
      DCHECK_EQ(end_result, MOJO_RESULT_OK);
      pending_data_frames_.pop_front();
      continue;
    }

    DCHECK_LT(chars.size(), data_frame.data_length);
    ConsumeDataFrame(false, data_frame.type, chars.data(), chars.size());
    const MojoResult end_result = readable_->EndReadData(buffer.size());
    DCHECK_EQ(end_result, MOJO_RESULT_OK);
    data_frame.type = network::mojom::blink::WebSocketMessageType::CONTINUATION;
    data_frame.data_length -= chars.size();
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
      DCHECK_EQ(message_chunks_->GetSize(), 0u);
      receiving_message_type_is_text_ = true;
      break;
    case network::mojom::blink::WebSocketMessageType::BINARY:
      DCHECK_EQ(message_chunks_->GetSize(), 0u);
      receiving_message_type_is_text_ = false;
      break;
  }

  const size_t message_size_so_far = message_chunks_->GetSize();
  if (message_size_so_far > std::numeric_limits<wtf_size_t>::max()) {
    message_chunks_->Clear();
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
    message_chunks_->Append(base::make_span(data, size));
    return;
  }

  Vector<base::span<const char>> chunks = message_chunks_->GetView();
  if (size > 0) {
    chunks.push_back(base::make_span(data, size));
  }
  auto opcode = receiving_message_type_is_text_
                    ? WebSocketOpCode::kOpCodeText
                    : WebSocketOpCode::kOpCodeBinary;
  probe::DidReceiveWebSocketMessage(execution_context_, identifier_, opcode,
                                    false, chunks);
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
    "WebSocketReceive", InspectorWebSocketTransferEvent::Data,
    execution_context_.Get(), identifier_, size);
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
  message_chunks_->Clear();
  received_text_is_all_ascii_ = true;
}

void WebSocketChannelImpl::OnWritable(MojoResult result,
                                      const mojo::HandleSignalsState& state) {
  DCHECK_EQ(GetState(), State::kOpen);
  DVLOG(2) << this << " OnWritable mojo_result=" << result;
  if (result != MOJO_RESULT_OK) {
    // We don't detect mojo errors on data pipe. Mojo connection errors will
    // be detected via |client_receiver_|.
    return;
  }
  wait_for_writable_ = false;
  ProcessSendQueue();
}

MojoResult WebSocketChannelImpl::ProduceData(
    base::span<const char>* data,
    uint64_t* consumed_buffered_amount) {
  MojoResult begin_result = MOJO_RESULT_OK;
  base::span<uint8_t> buffer;
  while (!data->empty() && (begin_result = writable_->BeginWriteData(
                                data->size(), MOJO_WRITE_DATA_FLAG_NONE,
                                buffer)) == MOJO_RESULT_OK) {
    const size_t size_to_write = std::min(buffer.size(), data->size());
    DCHECK_GT(size_to_write, 0u);

    base::as_writable_chars(buffer).copy_prefix_from(
        data->first(size_to_write));
    *data = data->subspan(size_to_write);

    const MojoResult end_result = writable_->EndWriteData(size_to_write);
    DCHECK_EQ(end_result, MOJO_RESULT_OK);
    *consumed_buffered_amount += size_to_write;
  }
  if (begin_result != MOJO_RESULT_OK &&
      begin_result != MOJO_RESULT_SHOULD_WAIT) {
    DVLOG(1) << "WebSocket::OnWritable mojo error=" << begin_result;
    DCHECK_EQ(begin_result, MOJO_RESULT_FAILED_PRECONDITION);
    writable_.reset();
  }
  return begin_result;
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
    StringBuffer<LChar> ascii_string_buffer(size);
    auto ascii_buffer = base::as_writable_chars(ascii_string_buffer.Span());
    for (const auto& chunk : chunks) {
      auto [copy_dest, rest] = ascii_buffer.split_at(chunk.size());
      copy_dest.copy_from(chunk);
      ascii_buffer = rest;
    }
    DCHECK(ascii_buffer.empty());
    return String(ascii_string_buffer.Release());
  }

  Vector<char> flatten;
  base::span<const char> span;
  if (chunks.size() > 1) {
    flatten.reserve(size);
    for (const auto& chunk : chunks) {
      flatten.AppendSpan(chunk);
    }
    span = base::span(flatten);
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
  DVLOG(1) << " OnConnectionError("
           << ", description:" << description
           << ", failure_message:" << failure_message_
           << "), set_from:" << set_from.ToString();
  String message;
  if (description.empty()) {
    message = failure_message_;
  } else {
    message = String::FromUTF8(description);
  }

  // This function is called when the implementation in the network service is
  // required to fail the WebSocket connection. Hence we fail this channel by
  // calling FailAsError function.
  FailAsError(message);
}

void WebSocketChannelImpl::Dispose() {
  connection_count_tracker_handle_.Decrement();
  message_chunks_->Reset();
  has_initiated_opening_handshake_ = true;
  feature_handle_for_scheduler_.reset();
  handshake_throttle_.reset();
  websocket_.reset();
  readable_watcher_.Cancel();
  writable_watcher_.Cancel();
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
