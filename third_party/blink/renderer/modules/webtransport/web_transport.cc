// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/web_transport.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_microtasks_scope.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_close_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_connection_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_datagram_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_hash.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_send_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_send_stream_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/core/fetch/headers.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/base_fetch_context.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_request.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_byte_source_base.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webtransport/bidirectional_stream.h"
#include "third_party/blink/renderer/modules/webtransport/datagram_duplex_stream.h"
#include "third_party/blink/renderer/modules/webtransport/receive_stream.h"
#include "third_party/blink/renderer/modules/webtransport/send_stream.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_datagrams_writable.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_error.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_receive_stream.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_send_group.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_send_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/format.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// The incoming max age to to be used when datagrams.incomingMaxAge is set to
// null.
constexpr base::TimeDelta kDefaultIncomingMaxAge = base::Seconds(60);

// The default datagrams.readable stream is created with a high water mark of
// zero so that it never buffers datagrams itself. Buffering and expiration stay
// in DatagramQueue, where incomingMaxBufferedDatagrams and incomingMaxAge keep
// applying to datagrams that have already arrived.
constexpr size_t kDatagramsReadableHighWaterMark = 0;

// Converts the Blink congestion control enum to its Mojo equivalent for
// renderer-to-browser IPC.
network::mojom::blink::WebTransportCongestionControl
BlinkCongestionControlToMojo(const V8WebTransportCongestionControl& cc) {
  switch (cc.AsEnum()) {
    case V8WebTransportCongestionControl::Enum::kDefault:
      return network::mojom::blink::WebTransportCongestionControl::kDefault;
    case V8WebTransportCongestionControl::Enum::kThroughput:
      return network::mojom::blink::WebTransportCongestionControl::kThroughput;
    case V8WebTransportCongestionControl::Enum::kLowLatency:
      return network::mojom::blink::WebTransportCongestionControl::kLowLatency;
  }
  NOTREACHED();
}

// Creates a mojo DataPipe with the options we use for our stream data pipes.
// Returns true on success.
bool CreateStreamDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                          mojo::ScopedDataPipeConsumerHandle* consumer) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  // TODO(ricea): Find an appropriate value for capacity_num_bytes.
  options.capacity_num_bytes = 0;

  MojoResult result = mojo::CreateDataPipe(&options, *producer, *consumer);
  return result == MOJO_RESULT_OK;
}

// Validates the conditions outlined in
// https://w3c.github.io/webtransport/#webtransport-constructor and returns an
// error message, or null string if the name is valid.
[[nodiscard]] String ValidateProtocolName(StringView protocol) {
  if (protocol.empty()) {
    return "Protocol name cannot be empty.";
  }
  if (!VisitCharacters(protocol, [](auto span) {
        for (const auto c : span) {
          // Protocol names are sf-strings, which are defined as sequences of
          // printable ASCII characters.
          // See <https://www.rfc-editor.org/rfc/rfc8941.html#name-strings>.
          if (c < 32 || c >= 127) {
            return false;
          }
        }
        return true;
      })) {
    return "Protocol name contains invalid characters.";
  }
  if (protocol.length() >= 512) {
    return "Protocol name is longer than 512 bytes.";
  }
  return String();
}

}  // namespace

// RecentlyForgottenStreamIdSet implementation
void WebTransport::RecentlyForgottenStreamIdSet::Insert(uint32_t stream_id) {
  auto result = id_set_.insert(stream_id);
  CHECK(result.is_new_entry);  // Should always be new given our call sites.
  if (id_set_.size() > kMaxSize) {
    id_set_.RemoveFirst();
  }
}

bool WebTransport::RecentlyForgottenStreamIdSet::Contains(
    uint32_t stream_id) const {
  return id_set_.Contains(stream_id);
}

void WebTransport::RecentlyForgottenStreamIdSet::Erase(uint32_t stream_id) {
  id_set_.erase(stream_id);
}

class WebTransport::PendingStreamCreation final
    : public GarbageCollected<PendingStreamCreation> {
 public:
  PendingStreamCreation(ScriptPromiseResolver<WritableStream>* resolver,
                        WebTransportSendGroup* send_group,
                        int64_t send_order)
      : unidirectional_resolver_(resolver),
        send_group_(send_group),
        send_order_(send_order) {}

  PendingStreamCreation(ScriptPromiseResolver<BidirectionalStream>* resolver,
                        WebTransportSendGroup* send_group,
                        int64_t send_order)
      : bidirectional_resolver_(resolver),
        send_group_(send_group),
        send_order_(send_order) {}

  void Start(WebTransport* web_transport) {
    CHECK(web_transport->transport_remote_.is_bound());
    constexpr char kInsufficientResourcesMessage[] = "Insufficient resources.";

    SendStreamOptions options;
    options.send_group = send_group_;
    options.send_order = send_order_;
    auto priority = BuildMojoPriority(options);

    if (unidirectional_resolver_) {
      mojo::ScopedDataPipeProducerHandle outgoing_producer;
      mojo::ScopedDataPipeConsumerHandle outgoing_consumer;
      if (!CreateStreamDataPipe(&outgoing_producer, &outgoing_consumer)) {
        unidirectional_resolver_->RejectWithDOMException(
            DOMExceptionCode::kUnknownError, kInsufficientResourcesMessage);
        return;
      }

      web_transport->create_stream_resolvers_.insert(unidirectional_resolver_);
      web_transport->transport_remote_->CreateStream(
          std::move(outgoing_consumer), mojo::ScopedDataPipeProducerHandle(),
          std::move(priority),
          BindOnce(&WebTransport::OnCreateSendStreamResponse,
                   WrapWeakPersistent(web_transport),
                   WrapWeakPersistent(unidirectional_resolver_.Get()),
                   std::move(outgoing_producer),
                   WrapPersistent(send_group_.Get()), send_order_));
      return;
    }

    CHECK(bidirectional_resolver_);
    mojo::ScopedDataPipeProducerHandle outgoing_producer;
    mojo::ScopedDataPipeConsumerHandle outgoing_consumer;
    mojo::ScopedDataPipeProducerHandle incoming_producer;
    mojo::ScopedDataPipeConsumerHandle incoming_consumer;
    if (!CreateStreamDataPipe(&outgoing_producer, &outgoing_consumer) ||
        !CreateStreamDataPipe(&incoming_producer, &incoming_consumer)) {
      bidirectional_resolver_->RejectWithDOMException(
          DOMExceptionCode::kUnknownError, kInsufficientResourcesMessage);
      return;
    }

    web_transport->create_stream_resolvers_.insert(bidirectional_resolver_);
    web_transport->transport_remote_->CreateStream(
        std::move(outgoing_consumer), std::move(incoming_producer),
        std::move(priority),
        BindOnce(&WebTransport::OnCreateBidirectionalStreamResponse,
                 WrapWeakPersistent(web_transport),
                 WrapWeakPersistent(bidirectional_resolver_.Get()),
                 std::move(outgoing_producer), std::move(incoming_consumer),
                 WrapPersistent(send_group_.Get()), send_order_));
  }

  void Reject(v8::Local<v8::Value> error) {
    if (unidirectional_resolver_) {
      unidirectional_resolver_->Reject(error);
      return;
    }
    CHECK(bidirectional_resolver_);
    bidirectional_resolver_->Reject(error);
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(unidirectional_resolver_);
    visitor->Trace(bidirectional_resolver_);
    visitor->Trace(send_group_);
  }

 private:
  Member<ScriptPromiseResolver<WritableStream>> unidirectional_resolver_;
  Member<ScriptPromiseResolver<BidirectionalStream>> bidirectional_resolver_;
  Member<WebTransportSendGroup> send_group_;
  const int64_t send_order_;
};

// Sends a datagram on write().
class WebTransport::DatagramUnderlyingSink final
    : public UnderlyingSinkBase,
      public WebTransportDatagramsWritable::Client {
  USING_PRE_FINALIZER(DatagramUnderlyingSink, Dispose);

 public:
  // `legacy` is true for the datagrams.writable stream, which sends through the
  // session and detaches from it when closed. A createWritable() sink owns a
  // Mojo remote for its own writable instead.
  DatagramUnderlyingSink(ScriptState* script_state,
                         WebTransport* web_transport,
                         DatagramDuplexStream* datagrams,
                         bool legacy)
      : script_state_(script_state),
        web_transport_(web_transport),
        datagrams_(datagrams),
        legacy_(legacy),
        writable_remote_(ExecutionContext::From(script_state)) {
    if (legacy_) {
      return;
    }
    // Bind the remote right away so that Datagrams and priority updates can be
    // queued on the pipe before the network service binds the receiver.
    pending_writable_receiver_ = writable_remote_.BindNewPipeAndPassReceiver(
        ExecutionContext::From(script_state)
            ->GetTaskRunner(TaskType::kNetworking));
    writable_remote_.set_disconnect_with_reason_handler(
        BindOnce(&DatagramUnderlyingSink::OnWritableDisconnected,
                 WrapWeakPersistent(this)));
  }

  ScriptPromise<IDLUndefined> start(ScriptState* script_state,
                                    WritableStreamDefaultController*,
                                    ExceptionState&) override {
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> write(ScriptState* script_state,
                                    ScriptValue chunk,
                                    WritableStreamDefaultController*,
                                    ExceptionState& exception_state) override {
    auto v8chunk = chunk.V8Value();
    auto* isolate = script_state->GetIsolate();

    if (v8chunk->IsArrayBuffer()) {
      DOMArrayBuffer* data = NativeValueTraits<DOMArrayBuffer>::NativeValue(
          isolate, v8chunk, exception_state);
      if (exception_state.HadException())
        return EmptyPromise();
      return SendDatagram(script_state, data->ByteSpan());
    }

    if (v8chunk->IsArrayBufferView()) {
      NotShared<DOMArrayBufferView> data =
          NativeValueTraits<NotShared<DOMArrayBufferView>>::NativeValue(
              isolate, v8chunk, exception_state);
      if (exception_state.HadException())
        return EmptyPromise();
      return SendDatagram(script_state, data->ByteSpan());
    }

    exception_state.ThrowTypeError(
        "Datagram is not an ArrayBuffer or ArrayBufferView type.");
    return EmptyPromise();
  }

  ScriptPromise<IDLUndefined> close(ScriptState* script_state,
                                    ExceptionState&) override {
    if (!web_transport_) {
      return ToResolvedUndefinedPromise(script_state);
    }
    if (legacy_) {
      web_transport_->ForgetDatagramUnderlyingSink(this);
      web_transport_ = nullptr;
    } else {
      close_requested_ = true;
      MaybeCloseWritableAfterDrain();
    }
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> abort(ScriptState* script_state,
                                    ScriptValue reason,
                                    ExceptionState&) override {
    while (!pending_datagrams_resolvers_.empty()) {
      pending_datagrams_resolvers_.TakeFirst()->Detach();
    }
    pending_datagrams_.clear();
    // Closing the pipe discards the Datagrams which are still queued in the
    // network service.
    writable_remote_.reset();
    if (web_transport_) {
      web_transport_->ForgetDatagramUnderlyingSink(this);
    }
    web_transport_ = nullptr;
    return ToResolvedUndefinedPromise(script_state);
  }

  // WebTransportDatagramsWritable::Client implementation:
  void SendDatagramWritablePriorityUpdate() override {
    if (!writable_remote_.is_bound()) {
      return;
    }
    writable_remote_->SetPriority(BuildPriority());
  }

  void SetStream(WritableStream* stream) { stream_ = stream; }

  // Hands the writable's PendingReceiver to the network service. This is
  // deferred until the session is connected, because a Datagram writable
  // cannot outlive the session it belongs to.
  void CreateNetworkWritableIfNeeded() {
    if (!pending_writable_receiver_ || !web_transport_ ||
        !web_transport_->transport_remote_.is_bound()) {
      return;
    }
    web_transport_->transport_remote_->CreateDatagramWritable(
        std::move(pending_writable_receiver_), BuildPriority());
  }

  void SendPendingDatagrams() {
    if (!web_transport_) {
      return;
    }
    DCHECK(web_transport_->transport_remote_.is_bound());
    CreateNetworkWritableIfNeeded();
    if (pending_datagrams_.empty()) {
      MaybeCloseWritableAfterDrain();
      return;
    }
    HeapDeque<Member<ScriptPromiseResolver<IDLUndefined>>>
        sent_datagram_resolvers;
    HeapVector<Member<ScriptPromiseResolver<IDLUndefined>>>
        dropped_datagram_resolvers;
    for (const auto& datagram : pending_datagrams_) {
      CHECK(!pending_datagrams_resolvers_.empty());
      auto resolver = pending_datagrams_resolvers_.TakeFirst();
      if (datagram.size() > datagrams_->maxDatagramSize()) {
        dropped_datagram_resolvers.push_back(resolver);
        continue;
      }
      sent_datagram_resolvers.push_back(resolver);
      SendToNetwork(base::span(datagram));
    }
    CHECK(pending_datagrams_resolvers_.empty());
    pending_datagrams_resolvers_.Swap(sent_datagram_resolvers);
    pending_datagrams_.clear();
    for (auto& resolver : dropped_datagram_resolvers) {
      resolver->Resolve();
    }
    MaybeReleasePendingWriteRetention();
    MaybeCloseWritableAfterDrain();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(web_transport_);
    visitor->Trace(datagrams_);
    visitor->Trace(stream_);
    visitor->Trace(pending_datagrams_resolvers_);
    visitor->Trace(writable_remote_);
    UnderlyingSinkBase::Trace(visitor);
    WebTransportDatagramsWritable::Client::Trace(visitor);
  }

  void Error(v8::Local<v8::Value> error) {
    writable_remote_.reset();
    ScriptState* script_state = script_state_.Get();
    if (!script_state->ContextIsValid()) {
      web_transport_ = nullptr;
      pending_datagrams_.clear();
      pending_datagrams_resolvers_.clear();
      return;
    }
    ScriptState::Scope scope(script_state);
    while (!pending_datagrams_resolvers_.empty()) {
      pending_datagrams_resolvers_.TakeFirst()->Reject(error);
    }
    pending_datagrams_.clear();
    web_transport_ = nullptr;
    if (stream_ && stream_->Controller()) {
      stream_->Controller()->error(
          script_state, ScriptValue(script_state->GetIsolate(), error));
    }
  }

 private:
  void Dispose() {
    writable_remote_.reset();
    web_transport_ = nullptr;
  }

  ScriptPromise<IDLUndefined> SendDatagram(ScriptState* script_state,
                                           base::span<const uint8_t> data) {
    if (!legacy_ &&
        disconnect_state_ == DisconnectState::kAwaitingSessionCleanup) {
      auto* resolver =
          MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
              script_state);
      pending_datagrams_resolvers_.push_back(resolver);
      web_transport_->RetainDatagramUnderlyingSinkWithPendingWrites(this);
      // The session error travels on a different pipe. Keep every write
      // pending, including an otherwise-discarded oversized Datagram, so
      // Cleanup() rejects it consistently.
      return resolver->Promise();
    }
    if (data.size() > datagrams_->maxDatagramSize()) {
      // The specification compares each write with the current maximum, even
      // while connecting, and silently discards oversized Datagrams.
      return ToResolvedUndefinedPromise(script_state);
    }
    if (!legacy_ && !writable_remote_.is_bound() &&
        disconnect_state_ == DisconnectState::kCreationRejected) {
      // The network service dropped this writable. Datagrams are unreliable, so
      // discard the write rather than stalling the stream.
      return ToResolvedUndefinedPromise(script_state);
    }

    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    pending_datagrams_resolvers_.push_back(resolver);
    if (!legacy_) {
      web_transport_->RetainDatagramUnderlyingSinkWithPendingWrites(this);
    }

    if (web_transport_->transport_remote_.is_bound()) {
      SendToNetwork(data);
    } else {
      Vector<uint8_t> datagram;
      datagram.append_range(data);
      pending_datagrams_.push_back(std::move(datagram));
    }
    uint32_t max_buffered_datagrams =
        datagrams_->outgoingMaxBufferedDatagrams();
    DCHECK_GT(max_buffered_datagrams, 0u);
    if (pending_datagrams_resolvers_.size() <
        static_cast<wtf_size_t>(max_buffered_datagrams)) {
      // In this case we pretend that the datagram is processed immediately, to
      // get more requests from the stream.
      resolver->Promise().MarkAsHandled();
      resolver->SuppressDetachCheck();
      return ToResolvedUndefinedPromise(script_state);
    }
    return resolver->Promise();
  }

  void OnDatagramProcessed(bool sent) {
    // Ignore a reply that arrives after connection cleanup rejected and
    // removed all pending writes.
    if (pending_datagrams_resolvers_.empty()) {
      return;
    }
    auto resolver = pending_datagrams_resolvers_.TakeFirst();
    ScriptState* script_state = script_state_.Get();
    if (!script_state->ContextIsValid()) {
      resolver->Detach();
      MaybeReleasePendingWriteRetention();
      MaybeCloseWritableAfterDrain();
      return;
    }
    resolver->Resolve();
    MaybeReleasePendingWriteRetention();
    MaybeCloseWritableAfterDrain();
  }

  // The network service discards whatever a writable has queued when its pipe
  // is closed, so a closed writable is drained by keeping the pipe open until
  // the last Datagram has been acknowledged.
  void MaybeCloseWritableAfterDrain() {
    if (close_requested_ && pending_datagrams_.empty() &&
        pending_datagrams_resolvers_.empty()) {
      writable_remote_.reset();
      pending_writable_receiver_.reset();
    }
  }

  // Called when the network service closes the writable, which it does when it
  // cannot keep it, for example because the session reached its writable limit.
  void OnWritableDisconnected(uint32_t custom_reason, const std::string&) {
    writable_remote_.reset();
    if (custom_reason != network::mojom::blink::WebTransportDatagramWritable::
                             kCreationRejectedDisconnectReason) {
      disconnect_state_ = DisconnectState::kAwaitingSessionCleanup;
      // Session notification and writable creation travel on different pipes.
      // An unexplained disconnect can mean that teardown destroyed an
      // undelivered creation request. Leave pending writes intact so Cleanup()
      // rejects them with the session error.
      return;
    }
    disconnect_state_ = DisconnectState::kCreationRejected;
    pending_datagrams_.clear();
    ScriptState* script_state = script_state_.Get();
    while (!pending_datagrams_resolvers_.empty()) {
      auto resolver = pending_datagrams_resolvers_.TakeFirst();
      if (script_state->ContextIsValid()) {
        // Datagrams are unreliable, so a discarded Datagram is still a
        // successful write.
        resolver->Resolve();
      } else {
        resolver->Detach();
      }
    }
    MaybeReleasePendingWriteRetention();
  }

  void MaybeReleasePendingWriteRetention() {
    if (!legacy_ && web_transport_ && pending_datagrams_resolvers_.empty()) {
      web_transport_->ReleaseDatagramUnderlyingSinkWithPendingWrites(this);
    }
  }

  network::mojom::blink::WebTransportStreamPriorityPtr BuildPriority() const {
    const auto* const writable =
        DynamicTo<WebTransportDatagramsWritable>(stream_.Get());
    CHECK(writable);
    const auto* send_group = writable->sendGroup();
    return network::mojom::blink::WebTransportStreamPriority::New(
        send_group ? std::make_optional<uint32_t>(send_group->group_id())
                   : std::nullopt,
        writable->sendOrder());
  }

  void SendToNetwork(base::span<const uint8_t> data) {
    auto callback = BindOnce(&DatagramUnderlyingSink::OnDatagramProcessed,
                             WrapWeakPersistent(this));
    if (legacy_) {
      web_transport_->transport_remote_->SendDatagram(data,
                                                      std::move(callback));
      return;
    }
    if (!writable_remote_.is_bound()) {
      // The pending writes have already been settled by
      // OnWritableDisconnected().
      return;
    }
    writable_remote_->SendDatagram(data, std::move(callback));
  }

  const Member<ScriptState> script_state_;
  Member<WebTransport> web_transport_;
  const Member<DatagramDuplexStream> datagrams_;
  enum class DisconnectState {
    kConnected,
    kCreationRejected,
    kAwaitingSessionCleanup,
  };
  // The legacy writable preserves its previous detach-on-close behavior and
  // sends through the session rather than through `writable_remote_`.
  const bool legacy_;
  // Used to propagate connection errors to the owning stream's controller.
  Member<WritableStream> stream_;
  bool close_requested_ = false;
  DisconnectState disconnect_state_ = DisconnectState::kConnected;
  Vector<Vector<uint8_t>> pending_datagrams_;
  HeapDeque<Member<ScriptPromiseResolver<IDLUndefined>>>
      pending_datagrams_resolvers_;
  // Null for the legacy writable. Resetting it tells the network service to
  // drop the writable and discard its queued Datagrams.
  HeapMojoRemote<network::mojom::blink::WebTransportDatagramWritable>
      writable_remote_;
  // Held until the session is connected; see CreateNetworkWritableIfNeeded().
  mojo::PendingReceiver<network::mojom::blink::WebTransportDatagramWritable>
      pending_writable_receiver_;
};

// Keeps incoming datagrams outside ReadableStream's internal queue so they can
// expire or be discarded when the configured buffer limit changes.
class WebTransport::DatagramQueue final
    : public GarbageCollected<DatagramQueue> {
 public:
  DatagramQueue(ScriptState* script_state,
                DatagramDuplexStream* datagram_duplex_stream)
      : script_state_(script_state),
        datagram_duplex_stream_(datagram_duplex_stream),
        expiry_timer_(ExecutionContext::From(script_state)
                          ->GetTaskRunner(TaskType::kNetworking),
                      this,
                      &DatagramQueue::ExpiryTimerFired) {}

  DOMUint8Array* TakeNextDatagram() {
    // incomingMaxBufferedDatagrams and incomingMaxAge can change at any time,
    // and the expiry timer only fires when it is scheduled, so both limits are
    // re-applied on every read.
    DiscardExcessDatagrams();
    MaybeExpireDatagrams();

    if (queue_.empty()) {
      return nullptr;
    }

    DOMUint8Array* datagram = queue_.front()->datagram;
    queue_.pop_front();
    if (queue_.empty()) {
      expiry_timer_.Stop();
    }
    return datagram;
  }

  void Push(base::span<const uint8_t> data) {
    DiscardExcessDatagrams();

    const wtf_size_t max_buffered_datagrams = MaxBufferedDatagrams();
    // A maximum of zero means datagrams are never buffered: they are only
    // delivered if a read is already waiting for them, which is handled by
    // DatagramSource before it reaches the queue.
    if (max_buffered_datagrams == 0) {
      DCHECK(queue_.empty());
      return;
    }

    if (queue_.size() == max_buffered_datagrams) {
      queue_.pop_front();
      ++dropped_datagram_count_;
    }

    auto now = base::TimeTicks::Now();
    queue_.push_back(
        MakeGarbageCollected<QueueEntry>(DOMUint8Array::Create(data), now));
    MaybeExpireDatagrams(now);
  }

  bool empty() const { return queue_.empty(); }

  void Clear() {
    queue_.clear();
    expiry_timer_.Stop();
  }

  uint64_t dropped_datagram_count() const { return dropped_datagram_count_; }

  void Trace(Visitor* visitor) const {
    visitor->Trace(script_state_);
    visitor->Trace(queue_);
    visitor->Trace(datagram_duplex_stream_);
    visitor->Trace(expiry_timer_);
  }

 private:
  struct QueueEntry : GarbageCollected<QueueEntry> {
    QueueEntry(DOMUint8Array* datagram, base::TimeTicks received_time)
        : datagram(datagram), received_time(received_time) {}

    const Member<DOMUint8Array> datagram;
    const base::TimeTicks received_time;

    void Trace(Visitor* visitor) const { visitor->Trace(datagram); }
  };

  void DiscardExcessDatagrams() {
    DVLOG(1) << "DatagramQueue::DiscardExcessDatagrams() queue_.size="
             << queue_.size();

    const wtf_size_t max_buffered_datagrams = MaxBufferedDatagrams();
    while (queue_.size() > max_buffered_datagrams) {
      queue_.pop_front();
      ++dropped_datagram_count_;
    }

    if (queue_.empty()) {
      DVLOG(1) << "DatagramQueue::DiscardExcessDatagrams() queue size now zero";
      expiry_timer_.Stop();
    }
  }

  void ExpiryTimerFired(TimerBase*) {
    DVLOG(1) << "DatagramQueue::ExpiryTimerFired()";
    MaybeExpireDatagrams();
  }

  void MaybeExpireDatagrams() { MaybeExpireDatagrams(base::TimeTicks::Now()); }

  void MaybeExpireDatagrams(base::TimeTicks now) {
    DVLOG(1) << "DatagramQueue::MaybeExpireDatagrams() now=" << now
             << " queue_.size=" << queue_.size();

    std::optional<double> optional_max_age =
        datagram_duplex_stream_->incomingMaxAge();
    bool max_age_is_default = false;
    base::TimeDelta max_age;
    if (optional_max_age.has_value()) {
      max_age = base::Milliseconds(optional_max_age.value());
    } else {
      max_age_is_default = true;
      max_age = kDefaultIncomingMaxAge;
    }

    // base::TimeTicks::Now() is far away from the origin of the monotonic
    // clock, so subtracting `max_age` cannot produce a bogus (saturated) value.
    DCHECK_GT(now, base::TimeTicks());
    const base::TimeTicks older_than = now - max_age;

    bool discarded = false;
    while (!queue_.empty() && queue_.front()->received_time < older_than) {
      discarded = true;
      queue_.pop_front();
    }

    if (discarded && max_age_is_default) {
      if (auto* execution_context = ExecutionContext::From(script_state_)) {
        execution_context->AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kNetwork,
                mojom::blink::ConsoleMessageLevel::kWarning,
                "Incoming datagram was discarded by WebTransport due to "
                "reaching default incomingMaxAge"),
            true);
      }
    }

    if (queue_.empty()) {
      DVLOG(1) << "DatagramQueue::MaybeExpireDatagrams() queue is now empty";
      expiry_timer_.Stop();
      return;
    }

    const base::TimeDelta age = now - queue_.front()->received_time;
    DCHECK_GE(max_age, age);
    base::TimeDelta time_until_next_expiry = max_age - age;
    // Waking up more often than once a second would waste power, and expiring
    // datagrams slightly late is harmless: reads discard expired datagrams
    // before returning them.
    if (time_until_next_expiry < base::Seconds(1)) {
      time_until_next_expiry = base::Seconds(1);
    }

    // An earlier wakeup is already scheduled, and it will reschedule the timer
    // for whatever remains in the queue.
    if (expiry_timer_.IsActive() &&
        expiry_timer_.NextFireInterval() <= time_until_next_expiry) {
      return;
    }

    expiry_timer_.StartOneShot(time_until_next_expiry, FROM_HERE);
  }

  wtf_size_t MaxBufferedDatagrams() const {
    return base::checked_cast<wtf_size_t>(
        datagram_duplex_stream_->incomingMaxBufferedDatagrams());
  }

  const Member<ScriptState> script_state_;
  HeapDeque<Member<const QueueEntry>> queue_;
  const Member<DatagramDuplexStream> datagram_duplex_stream_;
  HeapTaskRunnerTimer<DatagramQueue> expiry_timer_;
  uint64_t dropped_datagram_count_ = 0;
};

// Owns the receive state machine shared by the default and byte
// datagrams.readable implementations.
class WebTransport::DatagramSource : public GarbageCollectedMixin {
 public:
  DatagramSource(ScriptState* script_state, DatagramQueue* queue)
      : script_state_(script_state), queue_(queue) {}
  virtual ~DatagramSource() = default;

  ScriptPromise<IDLUndefined> PullDatagram(ExceptionState& exception_state) {
    DVLOG(1) << "DatagramSource::PullDatagram()";

    if (waiting_for_datagrams_) {
      // This can happen if a second read is issued while a read is already
      // pending.
      DCHECK(queue_->empty());
      return ToResolvedUndefinedPromise(script_state_.Get());
    }

    DOMUint8Array* datagram = queue_->TakeNextDatagram();
    if (!datagram) {
      waiting_for_datagrams_ = true;
      return ToResolvedUndefinedPromise(script_state_.Get());
    }

    // The datagram has already been removed from the queue, because Enqueue()
    // runs script which can re-enter this object, for example by starting
    // another read.
    Enqueue(datagram, exception_state);

    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  ScriptPromise<IDLUndefined> CancelDatagrams(v8::Local<v8::Value> reason) {
    uint32_t code = 0;
    WebTransportError* exception =
        V8WebTransportError::ToWrappable(script_state_->GetIsolate(), reason);
    if (exception) {
      code = exception->streamErrorCode().value_or(0);
    }
    VLOG(1) << "DatagramSource::CancelDatagrams() with code " << code;

    waiting_for_datagrams_ = false;
    canceled_ = true;
    queue_->Clear();

    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  ScriptState* GetScriptState() const { return script_state_.Get(); }

  void Error(v8::Local<v8::Value> error) {
    DVLOG(1) << "DatagramSource::Error()";

    waiting_for_datagrams_ = false;
    queue_->Clear();
    ErrorController(error);
  }

  void OnDatagramReceived(base::span<const uint8_t> data) {
    DVLOG(1) << "DatagramSource::OnDatagramReceived() size=" << data.size();

    if (canceled_) {
      return;
    }

    DCHECK_GT(data.size(), 0u);

    if (waiting_for_datagrams_) {
      DCHECK(queue_->empty());
      waiting_for_datagrams_ = false;
      // The datagram is delivered directly to the stream, bypassing the queue,
      // so that a zero incomingMaxBufferedDatagrams still permits reads that
      // are already waiting to complete.
      NonThrowableExceptionState exception_state;
      EnqueueReceivedData(data, exception_state);
      return;
    }

    queue_->Push(data);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(queue_);
  }

 protected:
  virtual void Enqueue(DOMUint8Array*, ExceptionState&) = 0;
  virtual void EnqueueReceivedData(base::span<const uint8_t>,
                                   ExceptionState&) = 0;
  virtual void ErrorController(v8::Local<v8::Value>) = 0;

 private:
  const Member<ScriptState> script_state_;
  const Member<DatagramQueue> queue_;
  bool waiting_for_datagrams_ = false;
  bool canceled_ = false;
};

// Implements the default, non-byte datagrams.readable stream.
class WebTransport::DatagramUnderlyingSource final
    : public UnderlyingSourceBase,
      public DatagramSource {
 public:
  DatagramUnderlyingSource(ScriptState* script_state, DatagramQueue* queue)
      : UnderlyingSourceBase(script_state),
        DatagramSource(script_state, queue) {}

  ScriptPromise<IDLUndefined> Pull(ScriptState*,
                                   ExceptionState& exception_state) override {
    return PullDatagram(exception_state);
  }

  ScriptPromise<IDLUndefined> Cancel(ScriptState*,
                                     ScriptValue reason,
                                     ExceptionState&) override {
    return CancelDatagrams(reason.V8Value());
  }

  void Trace(Visitor* visitor) const override {
    UnderlyingSourceBase::Trace(visitor);
    DatagramSource::Trace(visitor);
  }

 private:
  void Enqueue(DOMUint8Array* datagram, ExceptionState&) override {
    Controller()->Enqueue(datagram);
  }

  void EnqueueReceivedData(base::span<const uint8_t> data,
                           ExceptionState&) override {
    Controller()->Enqueue(DOMUint8Array::Create(data));
  }

  void ErrorController(v8::Local<v8::Value> error) override {
    Controller()->Error(error);
  }
};

// Implements the byte datagrams.readable stream, including BYOB reads.
class WebTransport::DatagramUnderlyingByteSource final
    : public UnderlyingByteSourceBase,
      public DatagramSource {
 public:
  DatagramUnderlyingByteSource(ScriptState* script_state, DatagramQueue* queue)
      : DatagramSource(script_state, queue) {}

  ScriptPromise<IDLUndefined> Pull(ReadableByteStreamController* controller,
                                   ExceptionState& exception_state) override {
    DCHECK_EQ(controller_, controller);
    return PullDatagram(exception_state);
  }

  ScriptPromise<IDLUndefined> Cancel() override {
    return CancelDatagrams(v8::Undefined(GetScriptState()->GetIsolate()));
  }

  ScriptPromise<IDLUndefined> Cancel(v8::Local<v8::Value> reason) override {
    return CancelDatagrams(reason);
  }

  ScriptState* GetScriptState() override {
    return DatagramSource::GetScriptState();
  }

  void SetController(ReadableByteStreamController* controller) {
    controller_ = controller;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(controller_);
    UnderlyingByteSourceBase::Trace(visitor);
    DatagramSource::Trace(visitor);
  }

 private:
  void Enqueue(DOMUint8Array* datagram,
               ExceptionState& exception_state) override {
    controller_->enqueue(GetScriptState(), NotShared(datagram),
                         exception_state);
  }

  void EnqueueReceivedData(base::span<const uint8_t> data,
                           ExceptionState& exception_state) override {
    ScriptState::Scope scope(GetScriptState());
    if (ReadableStreamBYOBRequest* request = controller_->byobRequest()) {
      DOMArrayPiece view(request->view().Get());
      if (view.ByteLength() < data.size()) {
        controller_->error(
            GetScriptState(),
            ScriptValue(GetScriptState()->GetIsolate(),
                        V8ThrowException::CreateRangeError(
                            GetScriptState()->GetIsolate(),
                            "supplied view is not large enough.")));
        return;
      }
      view.ByteSpan().copy_prefix_from(data);
      request->respond(GetScriptState(), data.size(), exception_state);
      return;
    }

    controller_->enqueue(GetScriptState(),
                         NotShared(DOMUint8Array::Create(data)),
                         exception_state);
  }

  void ErrorController(v8::Local<v8::Value> error) override {
    controller_->error(GetScriptState(),
                       ScriptValue(GetScriptState()->GetIsolate(), error));
  }

  Member<ReadableByteStreamController> controller_;
};

class WebTransport::StreamVendingUnderlyingSource final
    : public UnderlyingSourceBase {
 public:
  class StreamVendor : public GarbageCollected<StreamVendor> {
   public:
    using EnqueueCallback = base::OnceCallback<void(ScriptWrappable*)>;
    virtual void RequestStream(EnqueueCallback) = 0;
    virtual void Trace(Visitor*) const {}
  };

  template <class VendorType>
  static StreamVendingUnderlyingSource* CreateWithVendor(
      ScriptState* script_state,
      WebTransport* web_transport) {
    auto* vendor =
        MakeGarbageCollected<VendorType>(script_state, web_transport);
    return MakeGarbageCollected<StreamVendingUnderlyingSource>(script_state,
                                                               vendor);
  }

  StreamVendingUnderlyingSource(ScriptState* script_state, StreamVendor* vendor)
      : UnderlyingSourceBase(script_state),
        script_state_(script_state),
        vendor_(vendor) {}

  ScriptPromise<IDLUndefined> Pull(ScriptState* script_state,
                                   ExceptionState&) override {
    if (!is_opened_) {
      is_pull_waiting_ = true;
      return ToResolvedUndefinedPromise(script_state);
    }

    vendor_->RequestStream(BindOnce(&StreamVendingUnderlyingSource::Enqueue,
                                    WrapWeakPersistent(this)));

    return ToResolvedUndefinedPromise(script_state);
  }

  // Used by WebTransport to error the stream.
  void Error(v8::Local<v8::Value> reason) { Controller()->Error(reason); }

  // Used by WebTransport to close the stream.
  void Close() { Controller()->Close(); }

  // Used by WebTransport to notify that the WebTransport interface is
  // available.
  void NotifyOpened() {
    is_opened_ = true;

    if (is_pull_waiting_) {
      ScriptState::Scope scope(script_state_);
      NonThrowableExceptionState exception_state;
      Pull(script_state_, exception_state);
      is_pull_waiting_ = false;
    }
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(vendor_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  void Enqueue(ScriptWrappable* stream) {
    Controller()->Enqueue(
        ToV8Traits<ScriptWrappable>::ToV8(script_state_, stream));
  }

  const Member<ScriptState> script_state_;
  const Member<StreamVendor> vendor_;
  bool is_opened_ = false;
  bool is_pull_waiting_ = false;
};

class WebTransport::ReceiveStreamVendor final
    : public WebTransport::StreamVendingUnderlyingSource::StreamVendor {
 public:
  ReceiveStreamVendor(ScriptState* script_state, WebTransport* web_transport)
      : script_state_(script_state), web_transport_(web_transport) {}

  void RequestStream(EnqueueCallback enqueue) override {
    web_transport_->transport_remote_->AcceptUnidirectionalStream(
        blink::BindOnce(
            &ReceiveStreamVendor::OnAcceptUnidirectionalStreamResponse,
            WrapWeakPersistent(this), std::move(enqueue)));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(web_transport_);
    StreamVendor::Trace(visitor);
  }

 private:
  void OnAcceptUnidirectionalStreamResponse(
      EnqueueCallback enqueue,
      uint32_t stream_id,
      mojo::ScopedDataPipeConsumerHandle readable) {
    ScriptState::Scope scope(script_state_);
    auto* isolate = script_state_->GetIsolate();
    V8DoNotRunMicrotasksScope microtasks_scope(script_state_);
    v8::TryCatch try_catch(isolate);

    // TODO(crbug.com/510589920): Remove the legacy ReceiveStream path when
    // WebTransportReceiveStream ships.
    ReadableStream* stream_to_enqueue = nullptr;
    IncomingStream* incoming_stream = nullptr;
    auto init_stream = [&](auto* s) {
      s->Init(PassThroughException(isolate));
      stream_to_enqueue = s;
      incoming_stream = s->GetIncomingStream();
    };
    if (RuntimeEnabledFeatures::WebTransportReceiveStreamEnabled(
            ExecutionContext::From(script_state_))) {
      init_stream(MakeGarbageCollected<WebTransportReceiveStream>(
          script_state_, web_transport_, stream_id, std::move(readable)));
    } else {
      init_stream(MakeGarbageCollected<ReceiveStream>(
          script_state_, web_transport_, stream_id, std::move(readable)));
    }

    if (try_catch.HasCaught()) {
      // Abandon the stream.
      return;
    }

    // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
    CHECK_LT(stream_id, 0xfffffffe);
    web_transport_->incoming_stream_map_.insert(stream_id, incoming_stream);

    web_transport_->ProcessPendingIncomingStreamClose(stream_id,
                                                      incoming_stream);

    std::move(enqueue).Run(stream_to_enqueue);
  }

  const Member<ScriptState> script_state_;
  const Member<WebTransport> web_transport_;
};

class WebTransport::BidirectionalStreamVendor final
    : public WebTransport::StreamVendingUnderlyingSource::StreamVendor {
 public:
  BidirectionalStreamVendor(ScriptState* script_state,
                            WebTransport* web_transport)
      : script_state_(script_state), web_transport_(web_transport) {}

  void RequestStream(EnqueueCallback enqueue) override {
    web_transport_->transport_remote_->AcceptBidirectionalStream(
        blink::BindOnce(
            &BidirectionalStreamVendor::OnAcceptBidirectionalStreamResponse,
            WrapWeakPersistent(this), std::move(enqueue)));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(web_transport_);
    StreamVendor::Trace(visitor);
  }

 private:
  void OnAcceptBidirectionalStreamResponse(
      EnqueueCallback enqueue,
      uint32_t stream_id,
      mojo::ScopedDataPipeConsumerHandle incoming_consumer,
      mojo::ScopedDataPipeProducerHandle outgoing_producer) {
    ScriptState::Scope scope(script_state_);
    auto* bidirectional_stream = MakeGarbageCollected<BidirectionalStream>(
        script_state_, web_transport_, stream_id, std::move(outgoing_producer),
        std::move(incoming_consumer));

    auto* isolate = script_state_->GetIsolate();
    V8DoNotRunMicrotasksScope microtasks_scope(script_state_);
    v8::TryCatch try_catch(isolate);
    bidirectional_stream->Init(PassThroughException(isolate));
    if (try_catch.HasCaught()) {
      // Just throw away the stream.
      return;
    }

    // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
    CHECK_LT(stream_id, 0xfffffffe);
    IncomingStream* incoming_stream = bidirectional_stream->GetIncomingStream();
    web_transport_->incoming_stream_map_.insert(stream_id, incoming_stream);
    web_transport_->outgoing_stream_map_.insert(
        stream_id, bidirectional_stream->GetOutgoingStream());

    web_transport_->ProcessPendingIncomingStreamClose(stream_id,
                                                      incoming_stream);

    std::move(enqueue).Run(bidirectional_stream);
  }

  const Member<ScriptState> script_state_;
  const Member<WebTransport> web_transport_;
};

WebTransport* WebTransport::Create(ScriptState* script_state,
                                   const String& url,
                                   WebTransportOptions* options,
                                   ExceptionState& exception_state) {
  DVLOG(1) << "WebTransport::Create() url=" << url;
  DCHECK(options);
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebTransport);
  auto* transport =
      MakeGarbageCollected<WebTransport>(PassKey(), script_state, url);
  transport->UpdateStateIfNeeded();
  transport->Init(url, *options, exception_state);
  return transport;
}

WebTransport::WebTransport(PassKey,
                           ScriptState* script_state,
                           const String& url)
    : WebTransport(script_state, url, ExecutionContext::From(script_state)) {}

WebTransport::WebTransport(ScriptState* script_state,
                           const String& url,
                           ExecutionContext* context)
    : ActiveScriptWrappable<WebTransport>({}),
      ExecutionContextLifecycleStateObserver(context),
      script_state_(script_state),
      url_(NullUrl(), url),
      connector_(context),
      transport_remote_(context),
      handshake_client_receiver_(this, context),
      client_receiver_(this, context),
      ready_(MakeGarbageCollected<ReadyProperty>(context)),
      closed_(MakeGarbageCollected<
              ScriptPromiseProperty<WebTransportCloseInfo, IDLAny>>(context)),
      draining_(MakeGarbageCollected<DrainingProperty>(context)),
      inspector_transport_id_(CreateUniqueIdentifier()) {}

ScriptPromise<WritableStream> WebTransport::createUnidirectionalStream(
    ScriptState* script_state,
    WebTransportSendStreamOptions* options,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebTransport::createUnidirectionalStream() this=" << this;
  CHECK(options);

  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kQuicTransportStreamApis);
  if (!RuntimeEnabledFeatures::WebTransportCreateStreamsBeforeReadyEnabled() &&
      !transport_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNetworkError,
                                      "No connection.");
    return EmptyPromise();
  }

  auto stream_options = ExtractSendStreamOptions(options, exception_state);
  if (!stream_options) {
    return EmptyPromise();
  }

  if (!connection_pending_ && !transport_remote_.is_bound()) {
    constexpr char kInvalidStateMessage[] =
        "The WebTransport connection is not open.";
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<WritableStream>>(
            script_state, exception_state.GetContext());
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kInvalidStateMessage);
    return resolver->Promise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<WritableStream>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  auto* pending = MakeGarbageCollected<PendingStreamCreation>(
      resolver, stream_options->send_group, stream_options->send_order);
  if (connection_pending_) {
    pending_stream_creations_.push_back(pending);
  } else {
    CHECK(transport_remote_.is_bound());
    pending->Start(this);
  }
  return promise;
}

ReadableStream* WebTransport::incomingUnidirectionalStreams() {
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kQuicTransportStreamApis);
  return received_streams_;
}

ScriptPromise<BidirectionalStream> WebTransport::createBidirectionalStream(
    ScriptState* script_state,
    WebTransportSendStreamOptions* options,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebTransport::createBidirectionalStream() this=" << this;
  CHECK(options);

  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kQuicTransportStreamApis);
  if (!RuntimeEnabledFeatures::WebTransportCreateStreamsBeforeReadyEnabled() &&
      !transport_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNetworkError,
                                      "No connection.");
    return EmptyPromise();
  }

  auto stream_options = ExtractSendStreamOptions(options, exception_state);
  if (!stream_options) {
    return EmptyPromise();
  }

  if (!connection_pending_ && !transport_remote_.is_bound()) {
    constexpr char kInvalidStateMessage[] =
        "The WebTransport connection is not open.";
    auto* resolver =
        MakeGarbageCollected<ScriptPromiseResolver<BidirectionalStream>>(
            script_state, exception_state.GetContext());
    resolver->RejectWithDOMException(DOMExceptionCode::kInvalidStateError,
                                     kInvalidStateMessage);
    return resolver->Promise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<BidirectionalStream>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  auto* pending = MakeGarbageCollected<PendingStreamCreation>(
      resolver, stream_options->send_group, stream_options->send_order);
  if (connection_pending_) {
    pending_stream_creations_.push_back(pending);
  } else {
    CHECK(transport_remote_.is_bound());
    pending->Start(this);
  }
  return promise;
}

ReadableStream* WebTransport::incomingBidirectionalStreams() {
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kQuicTransportStreamApis);
  return received_bidirectional_streams_;
}

DatagramDuplexStream* WebTransport::datagrams() {
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kQuicTransportDatagramApis);
  return datagrams_;
}

WebTransportDatagramsWritable* WebTransport::CreateDatagramsWritable(
    ScriptState* script_state,
    WebTransportSendOptions* options,
    ExceptionState& exception_state) {
  CHECK(options);
  if (!connector_.is_bound() && !transport_remote_.is_bound()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "No connection.");
    return nullptr;
  }

  WebTransportSendGroup* send_group = options->sendGroup();
  if (send_group && send_group->GetTransport() != this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The sendGroup belongs to a different WebTransport instance.");
    return nullptr;
  }

  auto* sink = MakeGarbageCollected<DatagramUnderlyingSink>(
      script_state, this, datagrams_, /*legacy=*/false);
  auto* stream = MakeGarbageCollected<WebTransportDatagramsWritable>(
      script_state, this, sink, send_group, options->sendOrder());
  stream->Init(script_state, sink, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  sink->SetStream(stream);
  // The initial priority is read from `stream`, so this has to happen after
  // SetStream(). It does nothing while the session is still connecting;
  // OnConnectionEstablished() creates the writable in that case.
  sink->CreateNetworkWritableIfNeeded();
  datagram_underlying_sinks_.insert(sink);
  return stream;
}

void WebTransport::ForgetDatagramUnderlyingSink(DatagramUnderlyingSink* sink) {
  datagram_underlying_sinks_.erase(sink);
  ReleaseDatagramUnderlyingSinkWithPendingWrites(sink);
}

void WebTransport::RetainDatagramUnderlyingSinkWithPendingWrites(
    DatagramUnderlyingSink* sink) {
  datagram_underlying_sinks_with_pending_writes_.insert(sink);
}

void WebTransport::ReleaseDatagramUnderlyingSinkWithPendingWrites(
    DatagramUnderlyingSink* sink) {
  datagram_underlying_sinks_with_pending_writes_.erase(sink);
}

WritableStream* WebTransport::datagramWritable() {
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kQuicTransportDatagramApis);
  return outgoing_datagrams_;
}

ReadableStream* WebTransport::datagramReadable() {
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kQuicTransportDatagramApis);
  return received_datagrams_;
}

void WebTransport::close(WebTransportCloseInfo* close_info) {
  DVLOG(1) << "WebTransport::close() this=" << this;
  v8::Isolate* isolate = script_state_->GetIsolate();
  if (!connector_.is_bound() && !transport_remote_.is_bound()) {
    // This session has been closed or errored.
    return;
  }

  if (!transport_remote_.is_bound()) {
    // The state is "connecting".
    v8::Local<v8::Value> error =
        WebTransportError::Create(isolate, /*stream_error_code=*/std::nullopt,
                                  "close() is called while connecting.",
                                  V8WebTransportErrorSource::Enum::kSession);
    Cleanup(nullptr, error, /*abruptly=*/true);
    return;
  }

  v8::Local<v8::Value> error = WebTransportError::Create(
      isolate, /*stream_error_code=*/std::nullopt, "The session is closed.",
      V8WebTransportErrorSource::Enum::kSession);

  network::mojom::blink::WebTransportCloseInfoPtr close_info_to_pass;
  if (close_info) {
    close_info_to_pass = network::mojom::blink::WebTransportCloseInfo::New(
        close_info->closeCode(), close_info->reason());
  }

  transport_remote_->Close(std::move(close_info_to_pass));

  Cleanup(close_info ? close_info : WebTransportCloseInfo::Create(), error,
          /*abruptly=*/false);
}

void WebTransport::setDatagramWritableQueueExpirationDuration(double duration) {
  outgoing_datagram_expiration_duration_ = base::Milliseconds(duration);
  if (transport_remote_.is_bound()) {
    transport_remote_->SetOutgoingDatagramExpirationDuration(
        outgoing_datagram_expiration_duration_);
  }
}

ScriptPromise<IDLUndefined> WebTransport::ready(ScriptState* script_state) {
  return ready_->Promise(script_state->World());
}

ScriptPromise<WebTransportCloseInfo> WebTransport::closed(
    ScriptState* script_state) {
  return closed_->Promise(script_state->World());
}

ScriptPromise<IDLUndefined> WebTransport::draining(ScriptState* script_state) {
  return draining_->Promise(script_state->World());
}

ScriptPromise<WebTransportConnectionStats> WebTransport::getStats(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<WebTransportConnectionStats>>(
          script_state);
  if (!transport_remote_.is_bound() && !connection_pending_) {
    auto promise = resolver->Promise();
    if (latest_stats_) {
      resolver->Resolve(latest_stats_);
    } else {
      resolver->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot retreive stats on a failed connection.");
    }
    return promise;
  }

  const bool request_already_sent = !pending_get_stats_resolvers_.empty();
  pending_get_stats_resolvers_.push_back(resolver);
  if (transport_remote_.is_bound() && !request_already_sent) {
    transport_remote_->GetStats(
        BindOnce(&WebTransport::OnGetStatsResponse, WrapWeakPersistent(this)));
  }
  return resolver->Promise();
}

void WebTransport::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::blink::WebTransport> web_transport,
    mojo::PendingReceiver<network::mojom::blink::WebTransportClient>
        client_receiver,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers,
    const String& selected_application_protocol,
    network::mojom::blink::WebTransportStatsPtr initial_stats,
    std::optional<uint32_t> max_datagram_size) {
  DVLOG(1) << "WebTransport::OnConnectionEstablished() this=" << this;
  connector_.reset();
  handshake_client_receiver_.reset();

  probe::WebTransportConnectionEstablished(GetExecutionContext(),
                                           inspector_transport_id_);

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking);

  client_receiver_.Bind(std::move(client_receiver), task_runner);
  client_receiver_.set_disconnect_handler(
      BindOnce(&WebTransport::OnConnectionError, WrapWeakPersistent(this)));

  DCHECK(!transport_remote_.is_bound());
  transport_remote_.Bind(std::move(web_transport), task_runner);
  // Connection errors are handled by `client_receiver_`'s disconnect handler,
  // which keeps the ordering of a clean close intact. All this handler has to
  // do is complete receive-stream stats requests whose responses can no longer
  // arrive.
  transport_remote_.set_disconnect_handler(
      BindOnce(&WebTransport::RunPendingReceiveStreamStatsCallbacks,
               WrapWeakPersistent(this)));

  if (outgoing_datagram_expiration_duration_ != base::TimeDelta()) {
    transport_remote_->SetOutgoingDatagramExpirationDuration(
        outgoing_datagram_expiration_duration_);
  }

  if (!selected_application_protocol.IsNull()) {
    selected_application_protocol_ = selected_application_protocol;
  }

  if (RuntimeEnabledFeatures::WebTransportHeadersEnabled()) {
    auto* header_list = MakeGarbageCollected<FetchHeaderList>();
    size_t iter = 0;
    std::string name;
    std::string value;
    while (response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
      const String header_name = WebString::FromLatin1(name);
      if (
          // https://w3c.github.io/webtransport/#process-a-webtransport-fetch-response
          EqualIgnoringAsciiCase(header_name, "wt-protocol") ||
          // https://fetch.spec.whatwg.org/#forbidden-response-header-name
          EqualIgnoringAsciiCase(header_name, "set-cookie") ||
          EqualIgnoringAsciiCase(header_name, "set-cookie2")) {
        continue;
      }
      header_list->Append(header_name, WebString::FromLatin1(value));
    }
    response_headers_ = MakeGarbageCollected<Headers>(header_list);
    response_headers_->SetGuard(Headers::kImmutableGuard);
  }

  latest_stats_ = ConvertStatsFromMojom(std::move(initial_stats));
  if (max_datagram_size) {
    datagrams_->SetMaxDatagramSize(*max_datagram_size);
  }

  for (auto& sink : datagram_underlying_sinks_) {
    if (sink) {
      sink->SendPendingDatagrams();
    }
  }

  received_streams_underlying_source_->NotifyOpened();
  received_bidirectional_streams_underlying_source_->NotifyOpened();

  // Chromium only establishes WebTransport sessions over HTTP/3 connections
  // that negotiated H3 Datagram support.
  reliability_ = V8WebTransportReliabilityMode(
      V8WebTransportReliabilityMode::Enum::kSupportsUnreliable);
  connection_pending_ = false;
  ready_->ResolveWithUndefined();
  StartPendingStreamCreations();

  HeapVector<Member<ScriptPromiseResolver<WebTransportConnectionStats>>>
      stats_resolvers;
  pending_get_stats_resolvers_.swap(stats_resolvers);
  for (auto& resolver : stats_resolvers) {
    resolver->Resolve(latest_stats_);
  }
}

WebTransport::~WebTransport() = default;

void WebTransport::OnBeforeConnect(const net::IPEndPoint& server_address) {
  // |server_address| should be invalid from security/privacy reasons.
  DCHECK_EQ(server_address, net::IPEndPoint());
}

void WebTransport::OnHandshakeFailed(
    network::mojom::blink::WebTransportErrorPtr error) {
  // |error| should be null from security/privacy reasons.
  DCHECK(!error);
  DVLOG(1) << "WebTransport::OnHandshakeFailed() this=" << this;
  ScriptState::Scope scope(script_state_);
  v8::Local<v8::Value> error_to_pass = WebTransportError::Create(
      script_state_->GetIsolate(),
      /*stream_error_code=*/std::nullopt, "Opening handshake failed.",
      V8WebTransportErrorSource::Enum::kSession);
  Cleanup(nullptr, error_to_pass, /*abruptly=*/true);
}

void WebTransport::OnDatagramReceived(base::span<const uint8_t> data) {
  datagram_source_->OnDatagramReceived(data);
}

void WebTransport::OnIncomingStreamClosed(uint32_t stream_id,
                                          bool fin_received,
                                          uint64_t bytes_received) {
  DVLOG(1) << "WebTransport::OnIncomingStreamClosed(" << stream_id << ", "
           << fin_received << ") this=" << this;
  // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
  CHECK_LT(stream_id, 0xfffffffe);

  if (recently_forgotten_incoming_stream_ids_.Contains(stream_id)) {
    recently_forgotten_incoming_stream_ids_.Erase(stream_id);
    DVLOG(1) << "WebTransport::OnIncomingStreamClosed() correctly ignoring "
                "close on recently forgotten stream_id="
             << stream_id;
    DCHECK(incoming_stream_map_.find(stream_id) == incoming_stream_map_.end());
    return;
  }

  auto it = incoming_stream_map_.find(stream_id);

  if (it == incoming_stream_map_.end()) {
    // We reach here from two reasons.
    // 1) The stream may have already been removed from the map because of races
    //    between different ways of closing bidirectional streams.
    // 2) The stream is a server created incoming stream, and we haven't created
    //    it yet.
    // For the second case, we need to store `stream_id` and `fin_received` and
    // dispatch them later.
    DCHECK(closed_potentially_pending_streams_.find(stream_id) ==
           closed_potentially_pending_streams_.end());
    closed_potentially_pending_streams_.insert(
        stream_id, PendingIncomingStreamClose{fin_received, bytes_received});
    return;
  }

  IncomingStream* stream = it->value;
  stream->UpdateNetworkBytesReceived(bytes_received);
  // Note: stream->OnIncomingStreamClosed() will eventually trigger
  // ForgetIncomingStream() via the on_abort_ callback, which handles removal
  // from incoming_stream_map_. We don't need to record this close because
  // OnIncomingStreamClosed() won't be called again for the same stream_id.
  stream->OnIncomingStreamClosed(fin_received);
}

bool WebTransport::HasPendingClosedStreamForTesting(uint32_t stream_id) const {
  return closed_potentially_pending_streams_.Contains(stream_id);
}

wtf_size_t WebTransport::DatagramSinksWithPendingWritesSizeForTesting() const {
  return datagram_underlying_sinks_with_pending_writes_.size();
}

void WebTransport::ProcessPendingIncomingStreamClose(uint32_t stream_id,
                                                     IncomingStream* stream) {
  auto it = closed_potentially_pending_streams_.find(stream_id);
  if (it == closed_potentially_pending_streams_.end()) {
    return;
  }

  const PendingIncomingStreamClose close = it->value;
  closed_potentially_pending_streams_.erase(it);

  stream->UpdateNetworkBytesReceived(close.bytes_received);

  // This can run JavaScript. ProcessPendingIncomingStreamClose is called
  // before stream is exposed to application code.
  stream->OnIncomingStreamClosed(close.fin_received);
}

void WebTransport::OnReceivedResetStream(uint32_t stream_id,
                                         uint32_t stream_error_code,
                                         uint64_t bytes_received) {
  DVLOG(1) << "WebTransport::OnReceivedResetStream(" << stream_id << ", "
           << stream_error_code << ") this=" << this;
  auto it = incoming_stream_map_.find(stream_id);
  if (it == incoming_stream_map_.end()) {
    return;
  }
  IncomingStream* stream = it->value;
  stream->UpdateNetworkBytesReceived(bytes_received);

  ScriptState::Scope scope(script_state_);
  v8::Local<v8::Value> error = WebTransportError::Create(
      script_state_->GetIsolate(), stream_error_code, "Received RESET_STREAM.",
      V8WebTransportErrorSource::Enum::kStream);
  stream->Error(ScriptValue(script_state_->GetIsolate(), error));
}

void WebTransport::OnReceivedStopSending(uint32_t stream_id,
                                         uint32_t stream_error_code) {
  DVLOG(1) << "WebTransport::OnReceivedStopSending(" << stream_id << ", "
           << stream_error_code << ") this=" << this;

  auto it = outgoing_stream_map_.find(stream_id);
  if (it == outgoing_stream_map_.end()) {
    return;
  }
  OutgoingStream* stream = it->value;

  ScriptState::Scope scope(script_state_);
  v8::Local<v8::Value> error = WebTransportError::Create(
      script_state_->GetIsolate(), stream_error_code, "Received STOP_SENDING.",
      V8WebTransportErrorSource::Enum::kStream);
  stream->Error(ScriptValue(script_state_->GetIsolate(), error));
}

void WebTransport::OnClosed(
    network::mojom::blink::WebTransportCloseInfoPtr close_info,
    network::mojom::blink::WebTransportStatsPtr final_stats) {
  ScriptState::Scope scope(script_state_);
  v8::Isolate* isolate = script_state_->GetIsolate();

  latest_stats_ = ConvertStatsFromMojom(std::move(final_stats));

  auto* idl_close_info = MakeGarbageCollected<WebTransportCloseInfo>();
  if (close_info) {
    idl_close_info->setCloseCode(close_info->code);
    idl_close_info->setReason(close_info->reason);
  }

  v8::Local<v8::Value> error = WebTransportError::Create(
      isolate, /*stream_error_code=*/std::nullopt, "The session is closed.",
      V8WebTransportErrorSource::Enum::kSession);

  Cleanup(idl_close_info, error, /*abruptly=*/false);
}

void WebTransport::OnDraining() {
  DVLOG(1) << "WebTransport::OnDraining() this=" << this;
  if (draining_->GetState() == DrainingProperty::State::kPending) {
    draining_->ResolveWithUndefined();
  }
}

void WebTransport::OnOutgoingStreamClosed(uint32_t stream_id) {
  DVLOG(1) << "WebTransport::OnOutgoingStreamClosed(" << stream_id
           << ") this=" << this;
  auto it = outgoing_stream_map_.find(stream_id);

  // If a close is aborted, we may get the close response on a stream we've
  // already erased.
  if (it == outgoing_stream_map_.end())
    return;

  OutgoingStream* stream = it->value;
  DCHECK(stream);

  // We do this deletion first because OnOutgoingStreamClosed may run JavaScript
  // and so modify |outgoing_stream_map_|. |stream| is kept alive by being on
  // the stack.
  outgoing_stream_map_.erase(it);

  stream->OnOutgoingStreamClosed();
}

void WebTransport::ContextDestroyed() {
  DVLOG(1) << "WebTransport::ContextDestroyed() this=" << this;
  // Child streams must be reset first to ensure that garbage collection
  // ordering is safe. ContextDestroyed() is required not to execute JavaScript,
  // so this loop will not be re-entered.
  for (IncomingStream* stream : incoming_stream_map_.Values()) {
    stream->ContextDestroyed();
  }
  for (OutgoingStream* stream : outgoing_stream_map_.Values()) {
    stream->ContextDestroyed();
  }
  Dispose();
}

void WebTransport::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  if (state == mojom::blink::FrameLifecycleState::kFrozen) {
    if (!connector_.is_bound() && !transport_remote_.is_bound()) {
      // This session has been closed or errored.
      return;
    }

    if (transport_remote_.is_bound()) {
      // The state is "connected".
      transport_remote_->Close(nullptr);
    }
    DVLOG(1) << "WebTransport::ContextLifecycleStateChanged() frozen, closing "
                "connection. this="
             << this;
    GetExecutionContext()
        ->GetTaskRunner(TaskType::kNetworking)
        ->PostTask(
            FROM_HERE,
            BindOnce(
                [](WebTransport* transport) {
                  if (!transport ||
                      !transport->script_state_->ContextIsValid()) {
                    return;
                  }
                  ScriptState::Scope scope(transport->script_state_);
                  v8::Isolate* isolate = transport->script_state_->GetIsolate();
                  v8::Local<v8::Value> error = WebTransportError::Create(
                      isolate, std::nullopt, "Page entered back/forward cache.",
                      V8WebTransportErrorSource::Enum::kSession);
                  transport->Cleanup(nullptr, error, /*abruptly=*/true);
                },
                WrapWeakPersistent(this)));
  }
}

bool WebTransport::HasPendingActivity() const {
  DVLOG(1) << "WebTransport::HasPendingActivity() this=" << this;
  return handshake_client_receiver_.is_bound() || client_receiver_.is_bound();
}

void WebTransport::SendFin(uint32_t stream_id) {
  DVLOG(1) << "WebTransport::SendFin() this=" << this
           << ", stream_id=" << stream_id;
  transport_remote_->SendFin(stream_id);
}

void WebTransport::ResetStream(uint32_t stream_id, uint32_t code) {
  VLOG(0) << "WebTransport::ResetStream(" << stream_id << ", "
          << static_cast<uint32_t>(code) << ") this = " << this;
  transport_remote_->AbortStream(stream_id, code);
}

void WebTransport::StopSending(uint32_t stream_id, uint32_t code) {
  DVLOG(1) << "WebTransport::StopSending(" << stream_id << ", " << code
           << ") this = " << this;
  transport_remote_->StopSending(stream_id, code);
}

void WebTransport::SetStreamPriority(
    uint32_t stream_id,
    network::mojom::blink::WebTransportStreamPriorityPtr priority) {
  CHECK(priority);
  DVLOG(1) << "WebTransport::SetStreamPriority() this=" << this
           << ", stream_id=" << stream_id
           << ", has_send_group_id=" << priority->send_group_id.has_value()
           << ", send_group_id=" << priority->send_group_id.value_or(0)
           << ", send_order=" << priority->send_order;
  if (!transport_remote_.is_bound()) {
    return;
  }
  transport_remote_->SetStreamPriority(stream_id, std::move(priority));
}

void WebTransport::ForgetIncomingStream(uint32_t stream_id,
                                        bool has_received_close) {
  DVLOG(1) << "WebTransport::ForgetIncomingStream() this=" << this
           << ", stream_id=" << stream_id
           << ", has_received_close=" << has_received_close;
  incoming_stream_map_.erase(stream_id);
  // Only record if we haven't received OnIncomingStreamClosed() for this
  // stream. If we have, we know it won't be called again, so no need to track
  // it.
  if (!has_received_close) {
    recently_forgotten_incoming_stream_ids_.Insert(stream_id);
  }
}

void WebTransport::ForgetOutgoingStream(uint32_t stream_id) {
  DVLOG(1) << "WebTransport::ForgetOutgoingStream() this=" << this
           << ", stream_id=" << stream_id;
  outgoing_stream_map_.erase(stream_id);
}

void WebTransport::MaybeGetReceiveStreamStats(uint32_t stream_id) {
  auto it = incoming_stream_map_.find(stream_id);
  if (it != incoming_stream_map_.end()) {
    GetReceiveStreamStats(
        stream_id,
        BindOnce(
            [](IncomingStream* stream,
               network::mojom::blink::WebTransportReceiveStreamStatsPtr stats) {
              if (stream && stats) {
                stream->UpdateNetworkBytesReceived(stats->bytes_received);
              }
            },
            WrapWeakPersistent(it->value.Get())));
  }
}

void WebTransport::GetReceiveStreamStats(uint32_t stream_id,
                                         ReceiveStreamStatsCallback callback) {
  if (!transport_remote_.is_bound() || cleanup_started_) {
    std::move(callback).Run(nullptr);
    return;
  }

  const uint64_t request_id = next_receive_stream_stats_request_id_++;
  pending_receive_stream_stats_callbacks_.insert(request_id,
                                                 std::move(callback));
  transport_remote_->GetReceiveStreamStats(
      stream_id, BindOnce(&WebTransport::OnReceiveStreamStatsResponse,
                          WrapWeakPersistent(this), request_id));
}

void WebTransport::OnReceiveStreamStatsResponse(
    uint64_t request_id,
    network::mojom::blink::WebTransportReceiveStreamStatsPtr stats) {
  auto callback = pending_receive_stream_stats_callbacks_.Take(request_id);
  if (callback) {
    std::move(callback).Run(std::move(stats));
  }
}

void WebTransport::Trace(Visitor* visitor) const {
  visitor->Trace(datagrams_);
  visitor->Trace(received_datagrams_);
  visitor->Trace(datagram_queue_);
  visitor->Trace(datagram_source_);
  visitor->Trace(outgoing_datagrams_);
  visitor->Trace(datagram_underlying_sinks_);
  visitor->Trace(datagram_underlying_sinks_with_pending_writes_);
  visitor->Trace(script_state_);
  visitor->Trace(create_stream_resolvers_);
  visitor->Trace(pending_stream_creations_);
  visitor->Trace(connector_);
  visitor->Trace(transport_remote_);
  visitor->Trace(handshake_client_receiver_);
  visitor->Trace(client_receiver_);
  visitor->Trace(ready_);
  visitor->Trace(closed_);
  visitor->Trace(draining_);
  visitor->Trace(latest_stats_);
  visitor->Trace(pending_get_stats_resolvers_);
  visitor->Trace(incoming_stream_map_);
  visitor->Trace(outgoing_stream_map_);
  visitor->Trace(received_streams_);
  visitor->Trace(received_streams_underlying_source_);
  visitor->Trace(received_bidirectional_streams_);
  visitor->Trace(received_bidirectional_streams_underlying_source_);
  visitor->Trace(send_groups_);
  visitor->Trace(response_headers_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void WebTransport::Init(const String& url_for_diagnostics,
                        const WebTransportOptions& options,
                        ExceptionState& exception_state) {
  DVLOG(1) << "WebTransport::Init() url=" << url_for_diagnostics
           << " this=" << this;
  // This is an intentional spec violation due to our limited support for
  // detached realms.
  if (!script_state_->ContextIsValid()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Frame is detached.");
    return;
  }
  if (!url_.IsValid()) {
    // Do not use `url_` in the error message, since we want to display the
    // original URL and not the canonicalized version stored in `url_`.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        StrCat({"The URL '", url_for_diagnostics, "' is invalid."}));
    return;
  }

  if (!url_.ProtocolIs("https")) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        StrCat({"The URL's scheme must be 'https'. '", url_.Protocol(),
                "' is not allowed."}));
    return;
  }

  if (url_.HasFragmentIdentifier()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        StrCat({"The URL contains a fragment identifier ('#",
                url_.FragmentIdentifier(),
                "'). Fragment identifiers are not allowed in WebTransport "
                "URLs."}));
    return;
  }

  auto* execution_context = GetExecutionContext();

  bool is_url_blocked = false;
  if (!execution_context->GetContentSecurityPolicyForCurrentWorld()
           ->AllowConnectToSource(url_, url_, RedirectStatus::kNoRedirect)) {
    ScriptValue error(
        script_state_->GetIsolate(),
        WebTransportError::Create(
            script_state_->GetIsolate(),
            /*stream_error_code=*/std::nullopt,
            StrCat({"Refused to connect to '", url_.ElidedString(),
                    "' because it violates the document's Content Security "
                    "Policy"}),
            V8WebTransportErrorSource::Enum::kSession));

    connection_pending_ = false;
    ready_->Reject(error);
    closed_->Reject(error);

    is_url_blocked = true;
  }

  Vector<network::mojom::blink::WebTransportCertificateFingerprintPtr>
      fingerprints;
  if (options.hasServerCertificateHashes()) {
    for (const auto& hash : options.serverCertificateHashes()) {
      if (!hash->hasAlgorithm() || !hash->hasValue())
        continue;
      StringBuilder value_builder;
      DOMArrayPiece array_piece(hash->value());

      auto data = array_piece.ByteSpan();
      for (size_t i = 0; i < data.size(); ++i) {
        if (i > 0) {
          value_builder.Append(":");
        }
        FormatTo(value_builder, "{:02X}", data[i]);
      }

      fingerprints.push_back(
          network::mojom::blink::WebTransportCertificateFingerprint::New(
              hash->algorithm(), value_builder.ToString()));
    }
  }
  if (!fingerprints.empty()) {
    execution_context->CountUse(
        WebFeature::kWebTransportServerCertificateHashes);
  }

  if (options.hasProtocols()) {
    HashSet<String> encountered_protocols;
    for (const String& protocol : options.protocols()) {
      String validation_error = ValidateProtocolName(protocol);
      if (!validation_error.IsNull()) {
        exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                          validation_error);
        return;
      }
      HashSet<String>::AddResult add_result =
          encountered_protocols.insert(protocol);
      if (!add_result.is_new_entry) {
        exception_state.ThrowDOMException(
            DOMExceptionCode::kSyntaxError,
            "Duplicate protocols are not allowed.");
        return;
      }
    }
  }

  if (RuntimeEnabledFeatures::WebTransportCongestionControlEnabled(
          execution_context) &&
      options.hasCongestionControl()) {
    congestion_control_ =
        V8WebTransportCongestionControl(options.congestionControl());
  }

  if (RuntimeEnabledFeatures::
          WebTransportAnticipatedConcurrentIncomingStreamsEnabled(
              execution_context)) {
    anticipated_concurrent_incoming_unidirectional_streams_ =
        options.anticipatedConcurrentIncomingUnidirectionalStreams();
    anticipated_concurrent_incoming_bidirectional_streams_ =
        options.anticipatedConcurrentIncomingBidirectionalStreams();
  }

  net::HttpRequestHeaders::HeaderVector additional_headers;
  if (RuntimeEnabledFeatures::WebTransportHeadersEnabled() &&
      options.hasHeaders()) {
    auto* parsed_headers =
        Headers::Create(script_state_, options.headers(), exception_state);
    if (exception_state.HadException()) {
      return;
    }

    const auto& header_list = parsed_headers->HeaderList()->List();
    additional_headers.reserve(header_list.size());
    for (const auto& [name, value] : header_list) {
      if (EqualIgnoringAsciiCase(name, "wt-available-protocols")) {
        exception_state.ThrowTypeError(
            "The 'wt-available-protocols' header cannot be set.");
        return;
      }
      // Silently drop forbidden request headers per Fetch spec.
      if (cors::IsForbiddenRequestHeader(name, value)) {
        continue;
      }
      additional_headers.emplace_back(name.Latin1(), value.Latin1());
    }
  }

  if (auto* scheduler = execution_context->GetScheduler()) {
    // Two features are registered here:
    // - `kWebTransport`: a non-sticky feature that will disable aggressive
    // throttling for any page. It will be reset after the `WebTransport` is
    // disposed.
    // - `kWebTransportSticky`: a sticky feature that will only disable BFCache
    // for the page containing "Cache-Control: no-store" header. It won't be
    // reset even if the `WebTransport` is disposed.
    feature_handle_for_scheduler_ = scheduler->RegisterFeature(
        SchedulingPolicy::Feature::kWebTransport,
        SchedulingPolicy{SchedulingPolicy::DisableAggressiveThrottling()});
    scheduler->RegisterStickyFeature(
        SchedulingPolicy::Feature::kWebTransportSticky,
        SchedulingPolicy{SchedulingPolicy::DisableBackForwardCache()});
  }

  if (DoesSubresourceFilterBlockConnection(url_)) {
    // SubresourceFilter::ReportLoad() may report an actual message.
    ScriptValue dom_exception(
        script_state_->GetIsolate(),
        V8ThrowDOMException::CreateOrEmpty(
            script_state_->GetIsolate(), DOMExceptionCode::kNetworkError, ""));

    connection_pending_ = false;
    ready_->Reject(dom_exception);
    closed_->Reject(dom_exception);
    is_url_blocked = true;
  }

  if (!is_url_blocked) {
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        connector_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kNetworking)));

    connector_->Connect(
        url_, std::move(fingerprints),
        options.hasProtocols() ? options.protocols() : Vector<String>(),
        BlinkCongestionControlToMojo(congestion_control_),
        anticipated_concurrent_incoming_unidirectional_streams_,
        anticipated_concurrent_incoming_bidirectional_streams_,
        std::move(additional_headers),
        handshake_client_receiver_.BindNewPipeAndPassRemote(
            execution_context->GetTaskRunner(TaskType::kNetworking)));

    handshake_client_receiver_.set_disconnect_handler(
        BindOnce(&WebTransport::OnConnectionError, WrapWeakPersistent(this)));
  }

  probe::WebTransportCreated(execution_context, inspector_transport_id_, url_);

  uint32_t outgoing_max_buffered_datagrams = 1;
  datagrams_ = MakeGarbageCollected<DatagramDuplexStream>(
      this, outgoing_max_buffered_datagrams);

  datagram_queue_ =
      MakeGarbageCollected<DatagramQueue>(script_state_, datagrams_);
  // datagrams.readable was a byte stream before `datagramsReadableType` was
  // added, so keep that behavior when the feature is disabled. With the feature
  // enabled it is a default stream unless "bytes" is explicitly requested.
  const bool use_byte_stream =
      !RuntimeEnabledFeatures::WebTransportDatagramsReadableTypeEnabled(
          execution_context) ||
      (options.hasDatagramsReadableType() &&
       options.datagramsReadableType().AsEnum() ==
           V8ReadableStreamType::Enum::kBytes);
  if (use_byte_stream) {
    auto* byte_source = MakeGarbageCollected<DatagramUnderlyingByteSource>(
        script_state_, datagram_queue_);
    datagram_source_ = byte_source;
    received_datagrams_ =
        ReadableStream::CreateByteStream(script_state_, byte_source);
    byte_source->SetController(
        To<ReadableByteStreamController>(received_datagrams_->GetController()));
  } else {
    auto* source = MakeGarbageCollected<DatagramUnderlyingSource>(
        script_state_, datagram_queue_);
    datagram_source_ = source;
    received_datagrams_ = ReadableStream::CreateWithCountQueueingStrategy(
        script_state_, source, kDatagramsReadableHighWaterMark);
  }

  // We create a WritableStream with high water mark 1 and try to mimic the
  // given max buffered datagram count in the Sink, for two reasons:
  // 1. This is better because we can hide the RTT between the renderer and the
  //    network service.
  // 2. Keeping datagrams in the renderer would be confusing for the timer for
  //    the datagram queue in the network service, because the timestamp is
  //    taken when the datagram is added to the queue.
  auto* datagram_underlying_sink = MakeGarbageCollected<DatagramUnderlyingSink>(
      script_state_, this, datagrams_,
      /*legacy=*/true);
  outgoing_datagrams_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state_, datagram_underlying_sink, 1);
  datagram_underlying_sink->SetStream(outgoing_datagrams_);
  datagram_underlying_sinks_.insert(datagram_underlying_sink);

  received_streams_underlying_source_ =
      StreamVendingUnderlyingSource::CreateWithVendor<ReceiveStreamVendor>(
          script_state_, this);
  received_streams_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state_, received_streams_underlying_source_, 1);

  received_bidirectional_streams_underlying_source_ =
      StreamVendingUnderlyingSource::CreateWithVendor<
          BidirectionalStreamVendor>(script_state_, this);

  received_bidirectional_streams_ =
      ReadableStream::CreateWithCountQueueingStrategy(
          script_state_, received_bidirectional_streams_underlying_source_, 1);
}

bool WebTransport::DoesSubresourceFilterBlockConnection(const KURL& url) {
  ResourceFetcher* resource_fetcher = GetExecutionContext()->Fetcher();
  SubresourceFilter* subresource_filter =
      static_cast<BaseFetchContext*>(&resource_fetcher->Context())
          ->GetSubresourceFilter();
  return subresource_filter &&
         !subresource_filter->AllowWebTransportConnection(url);
}

void WebTransport::Dispose() {
  DVLOG(1) << "WebTransport::Dispose() this=" << this;
  cleanup_started_ = true;
  probe::WebTransportClosed(GetExecutionContext(), inspector_transport_id_);
  incoming_stream_map_.clear();
  outgoing_stream_map_.clear();
  // Note: recently_forgotten_incoming_stream_ids_ is not cleared explicitly;
  // let the garbage collector free the memory.
  // Clear pending close notifications.
  closed_potentially_pending_streams_.clear();
  pending_receive_stream_stats_callbacks_.clear();
  pending_stream_creations_.clear();
  send_groups_.clear();
  connector_.reset();
  transport_remote_.reset();
  handshake_client_receiver_.reset();
  client_receiver_.reset();
  // Make the page back/forward cache-able.
  feature_handle_for_scheduler_.reset();
}

// https://w3c.github.io/webtransport/#webtransport-cleanup
void WebTransport::Cleanup(WebTransportCloseInfo* info,
                           v8::Local<v8::Value> error,
                           bool abruptly) {
  CHECK_EQ(!info, abruptly);
  cleanup_started_ = true;
  v8::Isolate* isolate = script_state_->GetIsolate();

  constexpr char kInvalidStateMessage[] =
      "The WebTransport connection is not open.";
  v8::Local<v8::Value> stream_error = V8ThrowDOMException::CreateOrEmpty(
      isolate, DOMExceptionCode::kInvalidStateError, kInvalidStateMessage);
  RejectPendingStreamCreations(stream_error);
  RejectPendingStreamResolvers(stream_error);
  HandlePendingGetStatsResolvers(error);
  RunPendingReceiveStreamStatsCallbacks();
  ScriptValue error_value(isolate, error);
  datagram_source_->Error(error);
  // Error() enters V8 and may trigger GC. Keep strong references so every sink
  // registered when cleanup starts is processed and its pending write promises
  // are rejected. A WeakMember-only snapshot could lose a later sink during
  // that GC.
  HeapVector<Member<DatagramUnderlyingSink>> datagram_underlying_sinks;
  datagram_underlying_sinks.ReserveInitialCapacity(
      datagram_underlying_sinks_.size());
  for (auto& sink : datagram_underlying_sinks_) {
    if (sink) {
      datagram_underlying_sinks.push_back(sink);
    }
  }
  datagram_underlying_sinks_.clear();
  datagram_underlying_sinks_with_pending_writes_.clear();
  for (auto& sink : datagram_underlying_sinks) {
    sink->Error(error);
  }

  // We use local variables to avoid re-entrant problems.
  auto* incoming_bidirectional_streams_source =
      received_bidirectional_streams_underlying_source_.Get();
  auto* incoming_unidirectional_streams_source =
      received_streams_underlying_source_.Get();
  auto incoming_stream_map = std::move(incoming_stream_map_);
  auto outgoing_stream_map = std::move(outgoing_stream_map_);

  Dispose();

  for (const auto& kv : incoming_stream_map) {
    kv.value->Error(error_value);
  }
  for (const auto& kv : outgoing_stream_map) {
    kv.value->Error(error_value);
  }

  if (abruptly) {
    connection_pending_ = false;
    closed_->Reject(ScriptValue(isolate, error));
    if (ready_->GetState() == ReadyProperty::kPending) {
      ready_->Reject(ScriptValue(isolate, error));
    }
    incoming_bidirectional_streams_source->Error(error);
    incoming_unidirectional_streams_source->Error(error);
  } else {
    CHECK(info);
    closed_->Resolve(info);
    DCHECK_EQ(ready_->GetState(), ReadyProperty::kResolved);
    incoming_bidirectional_streams_source->Close();
    incoming_unidirectional_streams_source->Close();
  }
}

void WebTransport::OnConnectionError() {
  DVLOG(1) << "WebTransport::OnConnectionError() this=" << this;
  v8::Isolate* isolate = script_state_->GetIsolate();

  ScriptState::Scope scope(script_state_);
  v8::Local<v8::Value> error = WebTransportError::Create(
      isolate,
      /*stream_error_code=*/std::nullopt, "Connection lost.",
      V8WebTransportErrorSource::Enum::kSession);

  Cleanup(nullptr, error, /*abruptly=*/true);
}

void WebTransport::StartPendingStreamCreations() {
  HeapVector<Member<PendingStreamCreation>> pending;
  pending.swap(pending_stream_creations_);
  for (PendingStreamCreation* stream_creation : pending) {
    stream_creation->Start(this);
  }
}

void WebTransport::RejectPendingStreamCreations(v8::Local<v8::Value> error) {
  HeapVector<Member<PendingStreamCreation>> pending;
  pending.swap(pending_stream_creations_);
  for (PendingStreamCreation* stream_creation : pending) {
    stream_creation->Reject(error);
  }
}

void WebTransport::RunPendingReceiveStreamStatsCallbacks() {
  HashMap<uint64_t, ReceiveStreamStatsCallback,
          IntWithZeroKeyHashTraits<uint64_t>>
      callbacks;
  callbacks.swap(pending_receive_stream_stats_callbacks_);
  for (auto& entry : callbacks) {
    std::move(entry.value).Run(nullptr);
  }
}

void WebTransport::RejectPendingStreamResolvers(v8::Local<v8::Value> error) {
  HeapHashSet<Member<ScriptPromiseResolverBase>> create_stream_resolvers;
  create_stream_resolvers_.swap(create_stream_resolvers);
  for (ScriptPromiseResolverBase* resolver : create_stream_resolvers) {
    resolver->Reject(error);
  }
}

void WebTransport::HandlePendingGetStatsResolvers(v8::Local<v8::Value> error) {
  HeapVector<Member<ScriptPromiseResolver<WebTransportConnectionStats>>>
      stats_resolvers;
  stats_resolvers.swap(pending_get_stats_resolvers_);
  for (auto& resolver : stats_resolvers) {
    if (latest_stats_) {
      // "If transport.[[State]] is "closed", resolve p with the most recent
      // stats available for the connection [...]"
      resolver->Resolve(latest_stats_);
    } else {
      // `latest_stats_` is always set upon connection being established,
      // meaning that this only happens when the connection failed before being
      // established.
      resolver->RejectWithDOMException(
          DOMExceptionCode::kInvalidStateError,
          "Cannot retreive stats on a failed connection.");
    }
  }
}

void WebTransport::OnCreateSendStreamResponse(
    ScriptPromiseResolver<WritableStream>* resolver,
    mojo::ScopedDataPipeProducerHandle producer,
    WebTransportSendGroup* send_group,
    int64_t send_order,
    bool succeeded,
    uint32_t stream_id) {
  DVLOG(1) << "WebTransport::OnCreateSendStreamResponse() this=" << this
           << " succeeded=" << succeeded << " stream_id=" << stream_id;

  // Shouldn't resolve the promise if the execution context has gone away.
  if (!GetExecutionContext())
    return;

  // Shouldn't resolve the promise if the mojo interface is disconnected.
  if (!resolver || !create_stream_resolvers_.Take(resolver))
    return;

  ScriptState::Scope scope(script_state_);
  if (!succeeded) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        script_state_->GetIsolate(), DOMExceptionCode::kNetworkError,
        "Failed to create send stream."));
    return;
  }

  // TODO(crbug.com/487117768): Remove old SendStream path when
  // WebTransportSendGroup ships.
  WritableStream* writable_stream = nullptr;
  OutgoingStream* outgoing_stream = nullptr;
  auto* isolate = script_state_->GetIsolate();
  V8DoNotRunMicrotasksScope microtasks_scope(script_state_);
  v8::TryCatch try_catch(isolate);
  if (RuntimeEnabledFeatures::WebTransportSendGroupEnabled(
          GetExecutionContext())) {
    auto* send_stream = MakeGarbageCollected<WebTransportSendStream>(
        script_state_, this, stream_id, std::move(producer));
    send_stream->Init(PassThroughException(isolate));
    if (!try_catch.HasCaught()) {
      send_stream->ApplySendStreamOptions(send_group, send_order);
      outgoing_stream = send_stream->GetOutgoingStream();
      writable_stream = send_stream;
    }
  } else {
    auto* send_stream = MakeGarbageCollected<SendStream>(
        script_state_, this, stream_id, std::move(producer));
    send_stream->Init(PassThroughException(isolate));
    if (!try_catch.HasCaught()) {
      outgoing_stream = send_stream->GetOutgoingStream();
      writable_stream = send_stream;
    }
  }
  if (try_catch.HasCaught()) {
    resolver->Reject(try_catch.Exception());
    return;
  }

  // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
  CHECK_LT(stream_id, 0xfffffffe);
  outgoing_stream_map_.insert(stream_id, outgoing_stream);

  resolver->Resolve(writable_stream);
}

void WebTransport::OnCreateBidirectionalStreamResponse(
    ScriptPromiseResolver<BidirectionalStream>* resolver,
    mojo::ScopedDataPipeProducerHandle outgoing_producer,
    mojo::ScopedDataPipeConsumerHandle incoming_consumer,
    WebTransportSendGroup* send_group,
    int64_t send_order,
    bool succeeded,
    uint32_t stream_id) {
  DVLOG(1) << "WebTransport::OnCreateBidirectionalStreamResponse() this="
           << this << " succeeded=" << succeeded << " stream_id=" << stream_id;

  // Shouldn't resolve the promise if the execution context has gone away.
  if (!GetExecutionContext())
    return;

  // Shouldn't resolve the promise if the mojo interface is disconnected.
  if (!resolver || !create_stream_resolvers_.Take(resolver))
    return;

  ScriptState::Scope scope(script_state_);
  auto* isolate = script_state_->GetIsolate();
  if (!succeeded) {
    resolver->Reject(V8ThrowDOMException::CreateOrEmpty(
        isolate, DOMExceptionCode::kNetworkError,
        "Failed to create bidirectional stream."));
    return;
  }

  auto* bidirectional_stream = MakeGarbageCollected<BidirectionalStream>(
      script_state_, this, stream_id, std::move(outgoing_producer),
      std::move(incoming_consumer));

  V8DoNotRunMicrotasksScope microtasks_scope(script_state_);
  v8::TryCatch try_catch(isolate);
  bidirectional_stream->Init(PassThroughException(isolate));

  if (!try_catch.HasCaught()) {
    if (auto* send_stream = DynamicTo<WebTransportSendStream>(
            bidirectional_stream->writable())) {
      send_stream->ApplySendStreamOptions(send_group, send_order);
    }
  }

  if (try_catch.HasCaught()) {
    resolver->Reject(try_catch.Exception());
    // Don't insert into stream maps — the stream is in an inconsistent state.
    return;
  }

  // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
  CHECK_LT(stream_id, 0xfffffffe);
  incoming_stream_map_.insert(stream_id,
                              bidirectional_stream->GetIncomingStream());
  outgoing_stream_map_.insert(stream_id,
                              bidirectional_stream->GetOutgoingStream());

  resolver->Resolve(bidirectional_stream);
}

void WebTransport::OnGetStatsResponse(
    network::mojom::blink::WebTransportStatsPtr stats) {
  auto* idl_stats = ConvertStatsFromMojom(std::move(stats));
  latest_stats_ = idl_stats;
  HeapVector<Member<ScriptPromiseResolver<WebTransportConnectionStats>>>
      resolvers;
  pending_get_stats_resolvers_.swap(resolvers);
  for (auto& resolver : resolvers) {
    resolver->Resolve(idl_stats);
  }
}

WebTransportConnectionStats* WebTransport::ConvertStatsFromMojom(
    network::mojom::blink::WebTransportStatsPtr in) {
  auto* out = MakeGarbageCollected<WebTransportConnectionStats>();
  out->setMinRtt(in->min_rtt.InMillisecondsF());
  out->setSmoothedRtt(in->smoothed_rtt.InMillisecondsF());
  out->setRttVariation(in->rtt_variation.InMillisecondsF());
  if (in->estimated_send_rate_bps > 0) {
    out->setEstimatedSendRate(in->estimated_send_rate_bps);
  } else {
    out->setEstimatedSendRate(std::nullopt);
  }
  auto* datagram_stats = MakeGarbageCollected<WebTransportDatagramStats>();
  datagram_stats->setExpiredOutgoing(in->datagrams_expired_outgoing);
  datagram_stats->setLostOutgoing(in->datagrams_lost_outgoing);
  if (datagram_queue_) {
    datagram_stats->setDroppedIncoming(
        datagram_queue_->dropped_datagram_count());
  }
  out->setDatagrams(datagram_stats);
  return out;
}

const String& WebTransport::protocol() {
  return selected_application_protocol_;
}

V8WebTransportReliabilityMode WebTransport::reliability() const {
  return reliability_;
}

V8WebTransportCongestionControl WebTransport::congestionControl() const {
  // TODO(crbug.com/501268547): Per the W3C spec, this attribute should reflect
  // whether the UA *satisfied* the application's congestion control preference.
  // Currently, we always return the value that was set in the constructor
  // options. This is correct when the per-connection hint is honored (the
  // normal case), but if the global kWebTransportCongestionControl
  // base::Feature overrides the hint in the network layer, this attribute would
  // incorrectly report the original preference instead of what was actually
  // applied. To fix this properly, the effective congestion control value
  // should be plumbed back from the network service via
  // OnConnectionEstablished.
  return congestion_control_;
}

std::optional<uint16_t>
WebTransport::anticipatedConcurrentIncomingUnidirectionalStreams() const {
  return anticipated_concurrent_incoming_unidirectional_streams_;
}

void WebTransport::setAnticipatedConcurrentIncomingUnidirectionalStreams(
    std::optional<uint16_t> value) {
  anticipated_concurrent_incoming_unidirectional_streams_ = value;
  // Per spec, the setter only updates the internal slot. The value is used
  // during session establishment (via Connect()), not sent post-handshake.
}

std::optional<uint16_t>
WebTransport::anticipatedConcurrentIncomingBidirectionalStreams() const {
  return anticipated_concurrent_incoming_bidirectional_streams_;
}

void WebTransport::setAnticipatedConcurrentIncomingBidirectionalStreams(
    std::optional<uint16_t> value) {
  anticipated_concurrent_incoming_bidirectional_streams_ = value;
  // Per spec, the setter only updates the internal slot. The value is used
  // during session establishment (via Connect()), not sent post-handshake.
}

WebTransportSendGroup* WebTransport::createSendGroup(
    ExceptionState& exception_state) {
  if (next_send_group_id_ == std::numeric_limits<uint32_t>::max()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Too many send groups.");
    return nullptr;
  }
  uint32_t group_id = next_send_group_id_;
  next_send_group_id_ = base::CheckAdd(next_send_group_id_, 1).ValueOrDie();
  auto* group = MakeGarbageCollected<WebTransportSendGroup>(this, group_id);
  send_groups_.insert(group);
  return group;
}

// static
network::mojom::blink::WebTransportStreamPriorityPtr
WebTransport::BuildMojoPriority(const SendStreamOptions& options) {
  if (!options.send_group && options.send_order == 0) {
    return nullptr;
  }
  return network::mojom::blink::WebTransportStreamPriority::New(
      options.send_group
          ? std::make_optional<uint32_t>(options.send_group->group_id())
          : std::nullopt,
      options.send_order);
}

std::optional<WebTransport::SendStreamOptions>
WebTransport::ExtractSendStreamOptions(
    const WebTransportSendStreamOptions* options,
    ExceptionState& exception_state) {
  CHECK(options);
  SendStreamOptions result;

  result.send_group = options->sendGroup();
  if (result.send_group && result.send_group->GetTransport() != this) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The sendGroup belongs to a different WebTransport instance.");
    return std::nullopt;
  }
  result.send_order = options->sendOrder();
  return result;
}

Headers* WebTransport::responseHeaders() const {
  return response_headers_.Get();
}

// static
bool WebTransport::supportsReliableOnly() {
  // Chromium only supports WebTransport over HTTP/3 connections that
  // negotiated H3 Datagram support.
  return false;
}

}  // namespace blink
