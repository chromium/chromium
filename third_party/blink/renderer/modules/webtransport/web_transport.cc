// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/web_transport.h"

#include <stdint.h>

#include <optional>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_close_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_connection_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_datagram_stats.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_error.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_hash.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
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
#include "third_party/blink/renderer/modules/webtransport/web_transport_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// The incoming max age to to be used when datagrams.incomingMaxAge is set to
// null.
constexpr base::TimeDelta kDefaultIncomingMaxAge = base::Seconds(60);

// Creates a mojo DataPipe with the options we use for our stream data pipes. On
// success, returns true. On failure, throws an exception and returns false.
bool CreateStreamDataPipe(mojo::ScopedDataPipeProducerHandle* producer,
                          mojo::ScopedDataPipeConsumerHandle* consumer,
                          ExceptionState& exception_state) {
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
  options.element_num_bytes = 1;
  // TODO(ricea): Find an appropriate value for capacity_num_bytes.
  options.capacity_num_bytes = 0;

  MojoResult result = mojo::CreateDataPipe(&options, *producer, *consumer);
  if (result != MOJO_RESULT_OK) {
    // Probably out of resources.
    exception_state.ThrowDOMException(DOMExceptionCode::kUnknownError,
                                      "Insufficient resources.");
    return false;
  }

  return true;
}

}  // namespace

// Sends a datagram on write().
class WebTransport::DatagramUnderlyingSink final : public UnderlyingSinkBase {
 public:
  DatagramUnderlyingSink(WebTransport* web_transport,
                         DatagramDuplexStream* datagrams)
      : web_transport_(web_transport), datagrams_(datagrams) {}

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
      return SendDatagram(data->ByteSpan());
    }

    if (v8chunk->IsArrayBufferView()) {
      NotShared<DOMArrayBufferView> data =
          NativeValueTraits<NotShared<DOMArrayBufferView>>::NativeValue(
              isolate, v8chunk, exception_state);
      if (exception_state.HadException())
        return EmptyPromise();
      return SendDatagram(data->ByteSpan());
    }

    exception_state.ThrowTypeError(
        "Datagram is not an ArrayBuffer or ArrayBufferView type.");
    return EmptyPromise();
  }

  ScriptPromise<IDLUndefined> close(ScriptState* script_state,
                                    ExceptionState&) override {
    web_transport_ = nullptr;
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> abort(ScriptState* script_state,
                                    ScriptValue reason,
                                    ExceptionState&) override {
    web_transport_ = nullptr;
    return ToResolvedUndefinedPromise(script_state);
  }

  void SendPendingDatagrams() {
    DCHECK(web_transport_->transport_remote_.is_bound());
    for (const auto& datagram : pending_datagrams_) {
      web_transport_->transport_remote_->SendDatagram(
          base::make_span(datagram),
          WTF::BindOnce(&DatagramUnderlyingSink::OnDatagramProcessed,
                        WrapWeakPersistent(this)));
    }
    pending_datagrams_.clear();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(web_transport_);
    visitor->Trace(datagrams_);
    visitor->Trace(pending_datagrams_resolvers_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  ScriptPromise<IDLUndefined> SendDatagram(base::span<const uint8_t> data) {
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
        web_transport_->script_state_);
    // This resolver is for the return value of this function. When the
    // WebTransport is closed, the stream (for datagrams) is errored and
    // resolvers in `pending_datagrams_resolvers_` are released without
    // neither resolved nor rejected. That's fine, because the WritableStream
    // takes care of the case and reject all the pending promises when the
    // stream is errored. So we call SuppressDetachCheck here.
    resolver->SuppressDetachCheck();
    pending_datagrams_resolvers_.push_back(resolver);

    if (web_transport_->transport_remote_.is_bound()) {
      web_transport_->transport_remote_->SendDatagram(
          data, WTF::BindOnce(&DatagramUnderlyingSink::OnDatagramProcessed,
                              WrapWeakPersistent(this)));
    } else {
      Vector<uint8_t> datagram;
      datagram.AppendSpan(data);
      pending_datagrams_.push_back(std::move(datagram));
    }
    int high_water_mark = datagrams_->outgoingHighWaterMark();
    DCHECK_GT(high_water_mark, 0);
    if (pending_datagrams_resolvers_.size() <
        static_cast<wtf_size_t>(high_water_mark)) {
      // In this case we pretend that the datagram is processed immediately, to
      // get more requests from the stream.
      return ToResolvedUndefinedPromise(web_transport_->script_state_.Get());
    }
    return resolver->Promise();
  }

  void OnDatagramProcessed(bool sent) {
    DCHECK(!pending_datagrams_resolvers_.empty());
    pending_datagrams_resolvers_.TakeFirst()->Resolve();
  }

  Member<WebTransport> web_transport_;
  const Member<DatagramDuplexStream> datagrams_;
  Vector<Vector<uint8_t>> pending_datagrams_;
  HeapDeque<Member<ScriptPromiseResolver<IDLUndefined>>>
      pending_datagrams_resolvers_;
};

// Passes incoming datagrams to the datagrams.readable stream. It maintains its
// own internal queue of datagrams so that stale datagrams won't remain in
// ReadableStream's queue.
class WebTransport::DatagramUnderlyingSource final
    : public UnderlyingByteSourceBase {
 public:
  DatagramUnderlyingSource(ScriptState* script_state,
                           DatagramDuplexStream* datagram_duplex_stream)
      : UnderlyingByteSourceBase(),
        script_state_(script_state),
        datagram_duplex_stream_(datagram_duplex_stream),
        expiry_timer_(ExecutionContext::From(script_state)
                          ->GetTaskRunner(TaskType::kNetworking),
                      this,
                      &DatagramUnderlyingSource::ExpiryTimerFired) {}

  // Implementation of UnderlyingByteSourceBase.
  ScriptPromise<IDLUndefined> Pull(ReadableByteStreamController* controller,
                                   ExceptionState& exception_state) override {
    DVLOG(1) << "DatagramUnderlyingSource::pull()";

    if (waiting_for_datagrams_) {
      // This can happen if a second read is issued while a read is already
      // pending.
      DCHECK(queue_.empty());
      return ToResolvedUndefinedPromise(script_state_.Get());
    }

    // If high water mark is reset to 0 and then read() is called, it should
    // block waiting for a new datagram. So we may need to discard datagrams
    // here.
    DiscardExcessDatagrams();

    MaybeExpireDatagrams();

    if (queue_.empty()) {
      if (close_when_queue_empty_) {
        controller->close(script_state_, exception_state);
        return ToResolvedUndefinedPromise(script_state_.Get());
      }

      waiting_for_datagrams_ = true;
      return ToResolvedUndefinedPromise(script_state_.Get());
    }

    const QueueEntry* entry = queue_.front();
    queue_.pop_front();

    if (queue_.empty()) {
      expiry_timer_.Stop();
    }

    // This has to go after any mutations as it may run JavaScript, leading to
    // re-entry.
    controller->enqueue(script_state_,
                        NotShared<DOMUint8Array>(entry->datagram),
                        exception_state);
    if (exception_state.HadException()) {
      return ToResolvedUndefinedPromise(script_state_.Get());
    }

    // JavaScript could have called some other method at this point.
    // However, this is safe, because |close_when_queue_empty_| only ever
    // changes from false to true, and once it is true no more datagrams will
    // be added to |queue_|.
    if (close_when_queue_empty_ && queue_.empty()) {
      controller->close(script_state_, exception_state);
    }

    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  ScriptPromise<IDLUndefined> Cancel() override {
    return Cancel(v8::Undefined(script_state_->GetIsolate()));
  }

  ScriptPromise<IDLUndefined> Cancel(v8::Local<v8::Value> reason) override {
    uint32_t code = 0;
    WebTransportError* exception =
        V8WebTransportError::ToWrappable(script_state_->GetIsolate(), reason);
    if (exception) {
      code = exception->streamErrorCode().value_or(0);
    }
    VLOG(1) << "DatagramUnderlyingSource::Cancel() with code " << code;

    waiting_for_datagrams_ = false;
    canceled_ = true;
    DiscardQueue();

    return ToResolvedUndefinedPromise(script_state_.Get());
  }

  ScriptState* GetScriptState() override { return script_state_.Get(); }

  // Interface for use by WebTransport.
  void Close(ReadableByteStreamController* controller,
             ExceptionState& exception_state) {
    DVLOG(1) << "DatagramUnderlyingSource::Close()";

    if (queue_.empty()) {
      controller->close(script_state_, exception_state);
    } else {
      close_when_queue_empty_ = true;
    }
  }

  void Error(ReadableByteStreamController* controller,
             v8::Local<v8::Value> error) {
    DVLOG(1) << "DatagramUnderlyingSource::Error()";

    waiting_for_datagrams_ = false;
    DiscardQueue();
    controller->error(script_state_,
                      ScriptValue(script_state_->GetIsolate(), error));
  }

  void OnDatagramReceived(ReadableByteStreamController* controller,
                          base::span<const uint8_t> data) {
    DVLOG(1) << "DatagramUnderlyingSource::OnDatagramReceived() size="
             << data.size();

    // We should not receive any datagrams after Close() was called.
    DCHECK(!close_when_queue_empty_);

    if (canceled_) {
      return;
    }

    DCHECK_GT(data.size(), 0u);

    // This fast path is expected to be hit frequently. Avoid the queue.
    if (waiting_for_datagrams_) {
      DCHECK(queue_.empty());
      waiting_for_datagrams_ = false;
      // This may run JavaScript, so it has to be called immediately before
      // returning to avoid confusion caused by re-entrant usage.
      ScriptState::Scope scope(script_state_);
      // |enqueue| and |respond| throw if close has been requested, stream state
      // is not readable, or buffer is invalid. We checked
      // |close_when_queue_empty_| and data.size() so stream is readable and
      // buffer size is not 0.
      // |respond| also throws if controller is undefined or destination's
      // buffer size is not large enough. Controller is defined because
      // the BYOB request is a property of the given controller. If
      // destination's buffer size is not large enough, stream is errored before
      // respond.
      NonThrowableExceptionState exception_state;

      if (ReadableStreamBYOBRequest* request = controller->byobRequest()) {
        DOMArrayPiece view(request->view().Get());
        // If the view supplied is not large enough, error the stream to avoid
        // splitting a datagram.
        if (view.ByteLength() < data.size()) {
          controller->error(
              script_state_,
              ScriptValue(script_state_->GetIsolate(),
                          V8ThrowException::CreateRangeError(
                              script_state_->GetIsolate(),
                              "supplied view is not large enough.")));
          return;
        }
        view.ByteSpan().copy_prefix_from(data);
        request->respond(script_state_, data.size(), exception_state);
        return;
      }

      auto* datagram = DOMUint8Array::Create(data);
      controller->enqueue(script_state_, NotShared(datagram), exception_state);
      return;
    }

    DiscardExcessDatagrams();

    auto high_water_mark = HighWaterMark();

    // A high water mark of 0 has the semantics that all datagrams are discarded
    // unless there is read pending. This might be useful to someone, so support
    // it.
    if (high_water_mark == 0) {
      DCHECK(queue_.empty());
      return;
    }

    if (queue_.size() == high_water_mark) {
      // Need to get rid of an entry for the new one to replace.
      queue_.pop_front();
      ++dropped_datagram_count_;
    }

    auto* datagram = DOMUint8Array::Create(data);
    auto now = base::TimeTicks::Now();
    queue_.push_back(MakeGarbageCollected<QueueEntry>(datagram, now));
    MaybeExpireDatagrams(now);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(script_state_);
    visitor->Trace(queue_);
    visitor->Trace(datagram_duplex_stream_);
    visitor->Trace(expiry_timer_);
    UnderlyingByteSourceBase::Trace(visitor);
  }

  uint64_t dropped_datagram_count() const { return dropped_datagram_count_; }

 private:
  struct QueueEntry : GarbageCollected<QueueEntry> {
    QueueEntry(DOMUint8Array* datagram, base::TimeTicks received_time)
        : datagram(datagram), received_time(received_time) {}

    const Member<DOMUint8Array> datagram;
    const base::TimeTicks received_time;

    void Trace(Visitor* visitor) const { visitor->Trace(datagram); }
  };

  void DiscardExcessDatagrams() {
    DVLOG(1)
        << "DatagramUnderlyingSource::DiscardExcessDatagrams() queue_.size="
        << queue_.size();

    wtf_size_t high_water_mark = HighWaterMark();

    // The high water mark may have been set to a lower value, so the size can
    // be greater.
    while (queue_.size() > high_water_mark) {
      // TODO(ricea): Maybe free the memory associated with the array
      // buffer?
      queue_.pop_front();
      ++dropped_datagram_count_;
    }

    if (queue_.empty()) {
      DVLOG(1) << "DiscardExcessDatagrams: queue size now zero";
      expiry_timer_.Stop();
    }
  }

  void DiscardQueue() {
    queue_.clear();
    expiry_timer_.Stop();
  }

  void ExpiryTimerFired(TimerBase*) {
    DVLOG(1) << "DatagramUnderlyingSource::ExpiryTimerFired()";

    MaybeExpireDatagrams();
  }

  void MaybeExpireDatagrams() { MaybeExpireDatagrams(base::TimeTicks::Now()); }

  void MaybeExpireDatagrams(base::TimeTicks now) {
    DVLOG(1) << "DatagramUnderlyingSource::MaybeExpireDatagrams() now=" << now
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

    DCHECK_GT(now, base::TimeTicks());

    // base::TimeTicks can take negative values, so this subtraction won't
    // underflow even if MaxAge() is huge.
    base::TimeTicks older_than = now - max_age;

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
      DVLOG(1) << "MaybeExpireDatagrams queue is now empty";
      expiry_timer_.Stop();
      return;
    }

    base::TimeDelta age = now - queue_.front()->received_time;
    DCHECK_GE(max_age, age);
    base::TimeDelta time_until_next_expiry = max_age - age;

    // To reduce the number of wakeups, don't try to expire any more datagrams
    // for at least a second.
    if (time_until_next_expiry < base::Seconds(1)) {
      time_until_next_expiry = base::Seconds(1);
    }

    if (expiry_timer_.IsActive() &&
        expiry_timer_.NextFireInterval() <= time_until_next_expiry) {
      return;
    }

    expiry_timer_.StartOneShot(time_until_next_expiry, FROM_HERE);
  }

  wtf_size_t HighWaterMark() const {
    return base::checked_cast<wtf_size_t>(
        datagram_duplex_stream_->incomingHighWaterMark());
  }

  const Member<ScriptState> script_state_;
  HeapDeque<Member<const QueueEntry>> queue_;
  const Member<DatagramDuplexStream> datagram_duplex_stream_;
  HeapTaskRunnerTimer<DatagramUnderlyingSource> expiry_timer_;
  bool waiting_for_datagrams_ = false;
  bool canceled_ = false;
  bool close_when_queue_empty_ = false;
  uint64_t dropped_datagram_count_ = 0;
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

  ScriptPromiseUntyped Pull(ScriptState* script_state,
                            ExceptionState&) override {
    if (!is_opened_) {
      is_pull_waiting_ = true;
      return ToResolvedUndefinedPromise(script_state);
    }

    vendor_->RequestStream(WTF::BindOnce(
        &StreamVendingUnderlyingSource::Enqueue, WrapWeakPersistent(this)));

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
    web_transport_->transport_remote_->AcceptUnidirectionalStream(WTF::BindOnce(
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
    auto* receive_stream = MakeGarbageCollected<ReceiveStream>(
        script_state_, web_transport_, stream_id, std::move(readable));
    auto* isolate = script_state_->GetIsolate();
    v8::MicrotasksScope microtasks_scope(
        isolate, ToMicrotaskQueue(script_state_),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::TryCatch try_catch(isolate);
    receive_stream->Init(PassThroughException(isolate));

    if (try_catch.HasCaught()) {
      // Abandon the stream.
      return;
    }

    // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
    CHECK_LT(stream_id, 0xfffffffe);
    web_transport_->incoming_stream_map_.insert(
        stream_id, receive_stream->GetIncomingStream());

    auto it =
        web_transport_->closed_potentially_pending_streams_.find(stream_id);
    if (it != web_transport_->closed_potentially_pending_streams_.end()) {
      // The stream has already been closed in the network service.
      const bool fin_received = it->value;
      web_transport_->closed_potentially_pending_streams_.erase(it);

      // This can run JavaScript. This is safe because `receive_stream` hasn't
      // been exposed yet.
      receive_stream->GetIncomingStream()->OnIncomingStreamClosed(fin_received);
    }

    std::move(enqueue).Run(receive_stream);
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
    web_transport_->transport_remote_->AcceptBidirectionalStream(WTF::BindOnce(
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
    v8::MicrotasksScope microtasks_scope(
        isolate, ToMicrotaskQueue(script_state_),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::TryCatch try_catch(isolate);
    bidirectional_stream->Init(PassThroughException(isolate));
    if (try_catch.HasCaught()) {
      // Just throw away the stream.
      return;
    }

    // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
    CHECK_LT(stream_id, 0xfffffffe);
    web_transport_->incoming_stream_map_.insert(
        stream_id, bidirectional_stream->GetIncomingStream());
    web_transport_->outgoing_stream_map_.insert(
        stream_id, bidirectional_stream->GetOutgoingStream());

    auto it =
        web_transport_->closed_potentially_pending_streams_.find(stream_id);
    if (it != web_transport_->closed_potentially_pending_streams_.end()) {
      // The stream has already been closed in the network service.
      const bool fin_received = it->value;
      web_transport_->closed_potentially_pending_streams_.erase(it);

      // This can run JavaScript. This is safe because `receive_stream` hasn't
      // been exposed yet.
      bidirectional_stream->GetIncomingStream()->OnIncomingStreamClosed(
          fin_received);
    }

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
      ExecutionContextLifecycleObserver(context),
      script_state_(script_state),
      url_(NullURL(), url),
      connector_(context),
      transport_remote_(context),
      handshake_client_receiver_(this, context),
      client_receiver_(this, context),
      ready_(MakeGarbageCollected<ReadyProperty>(context)),
      closed_(MakeGarbageCollected<
              ScriptPromiseProperty<WebTransportCloseInfo, IDLAny>>(context)),
      inspector_transport_id_(CreateUniqueIdentifier()) {}

ScriptPromise<WritableStream> WebTransport::createUnidirectionalStream(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebTransport::createUnidirectionalStream() this=" << this;

  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kQuicTransportStreamApis);
  if (!transport_remote_.is_bound()) {
    // TODO(ricea): Should we wait if we're still connecting?
    exception_state.ThrowDOMException(DOMExceptionCode::kNetworkError,
                                      "No connection.");
    return EmptyPromise();
  }

  mojo::ScopedDataPipeProducerHandle data_pipe_producer;
  mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;

  if (!CreateStreamDataPipe(&data_pipe_producer, &data_pipe_consumer,
                            exception_state)) {
    return EmptyPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<WritableStream>>(
      script_state, exception_state.GetContext());
  create_stream_resolvers_.insert(resolver);
  transport_remote_->CreateStream(
      std::move(data_pipe_consumer), mojo::ScopedDataPipeProducerHandle(),
      WTF::BindOnce(&WebTransport::OnCreateSendStreamResponse,
                    WrapWeakPersistent(this), WrapWeakPersistent(resolver),
                    std::move(data_pipe_producer)));

  return resolver->Promise();
}

ReadableStream* WebTransport::incomingUnidirectionalStreams() {
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kQuicTransportStreamApis);
  return received_streams_;
}

ScriptPromise<BidirectionalStream> WebTransport::createBidirectionalStream(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DVLOG(1) << "WebTransport::createBidirectionalStream() this=" << this;

  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kQuicTransportStreamApis);
  if (!transport_remote_.is_bound()) {
    // TODO(ricea): We should wait if we are still connecting.
    exception_state.ThrowDOMException(DOMExceptionCode::kNetworkError,
                                      "No connection.");
    return EmptyPromise();
  }

  mojo::ScopedDataPipeProducerHandle outgoing_producer;
  mojo::ScopedDataPipeConsumerHandle outgoing_consumer;
  if (!CreateStreamDataPipe(&outgoing_producer, &outgoing_consumer,
                            exception_state)) {
    return EmptyPromise();
  }

  mojo::ScopedDataPipeProducerHandle incoming_producer;
  mojo::ScopedDataPipeConsumerHandle incoming_consumer;
  if (!CreateStreamDataPipe(&incoming_producer, &incoming_consumer,
                            exception_state)) {
    return EmptyPromise();
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<BidirectionalStream>>(
          script_state, exception_state.GetContext());
  create_stream_resolvers_.insert(resolver);
  transport_remote_->CreateStream(
      std::move(outgoing_consumer), std::move(incoming_producer),
      WTF::BindOnce(&WebTransport::OnCreateBidirectionalStreamResponse,
                    WrapWeakPersistent(this), WrapWeakPersistent(resolver),
                    std::move(outgoing_producer),
                    std::move(incoming_consumer)));

  return resolver->Promise();
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
    transport_remote_->GetStats(WTF::BindOnce(&WebTransport::OnGetStatsResponse,
                                              WrapWeakPersistent(this)));
  }
  return resolver->Promise();
}

void WebTransport::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::blink::WebTransport> web_transport,
    mojo::PendingReceiver<network::mojom::blink::WebTransportClient>
        client_receiver,
    network::mojom::blink::HttpResponseHeadersPtr response_headers,
    network::mojom::blink::WebTransportStatsPtr initial_stats) {
  DVLOG(1) << "WebTransport::OnConnectionEstablished() this=" << this;
  connector_.reset();
  handshake_client_receiver_.reset();

  probe::WebTransportConnectionEstablished(GetExecutionContext(),
                                           inspector_transport_id_);

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kNetworking);

  client_receiver_.Bind(std::move(client_receiver), task_runner);
  client_receiver_.set_disconnect_handler(WTF::BindOnce(
      &WebTransport::OnConnectionError, WrapWeakPersistent(this)));

  DCHECK(!transport_remote_.is_bound());
  transport_remote_.Bind(std::move(web_transport), task_runner);

  if (outgoing_datagram_expiration_duration_ != base::TimeDelta()) {
    transport_remote_->SetOutgoingDatagramExpirationDuration(
        outgoing_datagram_expiration_duration_);
  }

  latest_stats_ = ConvertStatsFromMojom(std::move(initial_stats));

  datagram_underlying_sink_->SendPendingDatagrams();

  received_streams_underlying_source_->NotifyOpened();
  received_bidirectional_streams_underlying_source_->NotifyOpened();

  connection_pending_ = false;
  ready_->ResolveWithUndefined();

  HeapVector<Member<ScriptPromiseResolver<WebTransportConnectionStats>>>
      stats_resolvers;
  pending_get_stats_resolvers_.swap(stats_resolvers);
  for (auto& resolver : stats_resolvers) {
    resolver->Resolve(latest_stats_);
  }
}

WebTransport::~WebTransport() = default;

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
  datagram_underlying_source_->OnDatagramReceived(
      received_datagrams_controller_, data);
}

void WebTransport::OnIncomingStreamClosed(uint32_t stream_id,
                                          bool fin_received) {
  DVLOG(1) << "WebTransport::OnIncomingStreamClosed(" << stream_id << ", "
           << fin_received << ") this=" << this;
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
    closed_potentially_pending_streams_.insert(stream_id, fin_received);
    return;
  }

  IncomingStream* stream = it->value;
  stream->OnIncomingStreamClosed(fin_received);
}

void WebTransport::OnReceivedResetStream(uint32_t stream_id,
                                         uint32_t stream_error_code) {
  DVLOG(1) << "WebTransport::OnReceivedResetStream(" << stream_id << ", "
           << stream_error_code << ") this=" << this;
  auto it = incoming_stream_map_.find(stream_id);
  if (it == incoming_stream_map_.end()) {
    return;
  }
  IncomingStream* stream = it->value;

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

void WebTransport::ForgetIncomingStream(uint32_t stream_id) {
  DVLOG(1) << "WebTransport::ForgetIncomingStream() this=" << this
           << ", stream_id=" << stream_id;
  incoming_stream_map_.erase(stream_id);
}

void WebTransport::ForgetOutgoingStream(uint32_t stream_id) {
  DVLOG(1) << "WebTransport::ForgetOutgoingStream() this=" << this
           << ", stream_id=" << stream_id;
  outgoing_stream_map_.erase(stream_id);
}

void WebTransport::Trace(Visitor* visitor) const {
  visitor->Trace(datagrams_);
  visitor->Trace(received_datagrams_);
  visitor->Trace(received_datagrams_controller_);
  visitor->Trace(datagram_underlying_source_);
  visitor->Trace(outgoing_datagrams_);
  visitor->Trace(datagram_underlying_sink_);
  visitor->Trace(script_state_);
  visitor->Trace(create_stream_resolvers_);
  visitor->Trace(connector_);
  visitor->Trace(transport_remote_);
  visitor->Trace(handshake_client_receiver_);
  visitor->Trace(client_receiver_);
  visitor->Trace(ready_);
  visitor->Trace(closed_);
  visitor->Trace(latest_stats_);
  visitor->Trace(pending_get_stats_resolvers_);
  visitor->Trace(incoming_stream_map_);
  visitor->Trace(outgoing_stream_map_);
  visitor->Trace(received_streams_);
  visitor->Trace(received_streams_underlying_source_);
  visitor->Trace(received_bidirectional_streams_);
  visitor->Trace(received_bidirectional_streams_underlying_source_);
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
        "The URL '" + url_for_diagnostics + "' is invalid.");
    return;
  }

  if (!url_.ProtocolIs("https")) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The URL's scheme must be 'https'. '" +
                                          url_.Protocol() +
                                          "' is not allowed.");
    return;
  }

  if (url_.HasFragmentIdentifier()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL contains a fragment identifier ('#" +
            url_.FragmentIdentifier() +
            "'). Fragment identifiers are not allowed in WebTransport URLs.");
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
            "Refused to connect to '" + url_.ElidedString() +
                "' because it violates the document's Content Security Policy",
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
        value_builder.AppendFormat("%02X", data[i]);
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

  if (auto* scheduler = execution_context->GetScheduler()) {
    // Two features are registered with `DisableBackForwardCache` policy here:
    // - `kWebTransport`: a non-sticky feature that will disable BFCache for any
    // page. It will be reset after the `WebTransport` is disposed.
    // - `kWebTransportSticky`: a sticky feature that will only disable BFCache
    // for the page containing "Cache-Control: no-store" header. It won't be
    // reset even if the `WebTransport` is disposed.
    feature_handle_for_scheduler_ = scheduler->RegisterFeature(
        SchedulingPolicy::Feature::kWebTransport,
        SchedulingPolicy{SchedulingPolicy::DisableAggressiveThrottling(),
                         SchedulingPolicy::DisableBackForwardCache()});
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
        handshake_client_receiver_.BindNewPipeAndPassRemote(
            execution_context->GetTaskRunner(TaskType::kNetworking)));

    handshake_client_receiver_.set_disconnect_handler(WTF::BindOnce(
        &WebTransport::OnConnectionError, WrapWeakPersistent(this)));
  }

  probe::WebTransportCreated(execution_context, inspector_transport_id_, url_);

  int outgoing_datagrams_high_water_mark = 1;
  datagrams_ = MakeGarbageCollected<DatagramDuplexStream>(
      this, outgoing_datagrams_high_water_mark);

  datagram_underlying_source_ =
      MakeGarbageCollected<DatagramUnderlyingSource>(script_state_, datagrams_);
  received_datagrams_ = ReadableStream::CreateByteStream(
      script_state_, datagram_underlying_source_);
  received_datagrams_controller_ =
      To<ReadableByteStreamController>(received_datagrams_->GetController());

  // We create a WritableStream with high water mark 1 and try to mimic the
  // given high water mark in the Sink, from two reasons:
  // 1. This is better because we can hide the RTT between the renderer and the
  //    network service.
  // 2. Keeping datagrams in the renderer would be confusing for the timer for
  // the datagram
  //    queue in the network service, because the timestamp is taken when the
  //    datagram is added to the queue.
  datagram_underlying_sink_ =
      MakeGarbageCollected<DatagramUnderlyingSink>(this, datagrams_);
  outgoing_datagrams_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state_, datagram_underlying_sink_, 1);

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
  probe::WebTransportClosed(GetExecutionContext(), inspector_transport_id_);
  incoming_stream_map_.clear();
  outgoing_stream_map_.clear();
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
  v8::Isolate* isolate = script_state_->GetIsolate();

  RejectPendingStreamResolvers(error);
  HandlePendingGetStatsResolvers(error);
  ScriptValue error_value(isolate, error);
  datagram_underlying_source_->Error(received_datagrams_controller_, error);
  outgoing_datagrams_->Controller()->error(script_state_, error_value);

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

  auto* send_stream = MakeGarbageCollected<SendStream>(
      script_state_, this, stream_id, std::move(producer));

  auto* isolate = script_state_->GetIsolate();
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state_),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch try_catch(isolate);
  send_stream->Init(PassThroughException(isolate));
  if (try_catch.HasCaught()) {
    resolver->Reject(try_catch.Exception());
    return;
  }

  // 0xfffffffe and 0xffffffff are reserved values in stream_map_.
  CHECK_LT(stream_id, 0xfffffffe);
  outgoing_stream_map_.insert(stream_id, send_stream->GetOutgoingStream());

  resolver->Resolve(send_stream);
}

void WebTransport::OnCreateBidirectionalStreamResponse(
    ScriptPromiseResolver<BidirectionalStream>* resolver,
    mojo::ScopedDataPipeProducerHandle outgoing_producer,
    mojo::ScopedDataPipeConsumerHandle incoming_consumer,
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

  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state_),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch try_catch(isolate);
  bidirectional_stream->Init(PassThroughException(isolate));
  if (try_catch.HasCaught()) {
    resolver->Reject(try_catch.Exception());
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
  if (datagram_underlying_source_) {
    datagram_stats->setDroppedIncoming(
        datagram_underlying_source_->dropped_datagram_count());
  }
  out->setDatagrams(datagram_stats);
  return out;
}

}  // namespace blink
