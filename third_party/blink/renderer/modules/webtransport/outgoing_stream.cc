// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/outgoing_stream.h"

#include <cstring>
#include <utility>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_error.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

class SendStreamAbortAlgorithm final : public AbortSignal::Algorithm {
 public:
  explicit SendStreamAbortAlgorithm(OutgoingStream* stream) : stream_(stream) {}
  ~SendStreamAbortAlgorithm() override = default;

  void Run() override { stream_->AbortAlgorithm(stream_); }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(stream_);
    Algorithm::Trace(visitor);
  }

 private:
  Member<OutgoingStream> stream_;
};

}  // namespace

class OutgoingStream::UnderlyingSink final : public UnderlyingSinkBase {
 public:
  explicit UnderlyingSink(OutgoingStream* outgoing_stream)
      : outgoing_stream_(outgoing_stream) {}

  // Implementation of UnderlyingSinkBase
  ScriptPromise start(ScriptState* script_state,
                      WritableStreamDefaultController* controller,
                      ExceptionState&) override {
    DVLOG(1) << "OutgoingStream::UnderlyinkSink::start() outgoing_stream_="
             << outgoing_stream_;

    outgoing_stream_->controller_ = controller;
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise write(ScriptState* script_state,
                      ScriptValue chunk,
                      WritableStreamDefaultController*,
                      ExceptionState& exception_state) override {
    DVLOG(1) << "OutgoingStream::UnderlyingSink::write() outgoing_stream_="
             << outgoing_stream_;

    // OutgoingStream::SinkWrite() is a separate method rather than being
    // inlined here because it makes many accesses to outgoing_stream_ member
    // variables.
    return outgoing_stream_->SinkWrite(script_state, chunk, exception_state);
  }

  ScriptPromise close(ScriptState* script_state, ExceptionState&) override {
    DVLOG(1) << "OutgoingStream::UnderlingSink::close() outgoing_stream_="
             << outgoing_stream_;

    // The streams specification guarantees that this will only be called after
    // all pending writes have been completed.
    DCHECK(!outgoing_stream_->write_promise_resolver_);

    DCHECK(!outgoing_stream_->close_promise_resolver_);

    outgoing_stream_->close_promise_resolver_ =
        MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    outgoing_stream_->pending_operation_ =
        outgoing_stream_->close_promise_resolver_;

    // In some cases (when the stream is aborted by a network error for
    // example), there may not be a call to OnOutgoingStreamClose. In that case
    // we will not be able to resolve the promise, but that will be taken care
    // by streams so we don't care.
    outgoing_stream_->close_promise_resolver_->SuppressDetachCheck();

    DCHECK_EQ(outgoing_stream_->state_, State::kOpen);
    outgoing_stream_->state_ = State::kSentFin;
    outgoing_stream_->client_->SendFin();

    // Close the data pipe to signal to the network service that no more data
    // will be sent.
    outgoing_stream_->ResetPipe();

    return outgoing_stream_->close_promise_resolver_->Promise();
  }

  ScriptPromise abort(ScriptState* script_state,
                      ScriptValue reason,
                      ExceptionState& exception_state) override {
    DVLOG(1) << "OutgoingStream::UnderlyingSink::abort() outgoing_stream_="
             << outgoing_stream_;
    DCHECK(!reason.IsEmpty());

    uint8_t code = 0;
    WebTransportError* exception = V8WebTransportError::ToWrappable(
        script_state->GetIsolate(), reason.V8Value());
    if (exception) {
      code = exception->streamErrorCode().value_or(0);
    }
    outgoing_stream_->client_->Reset(code);
    outgoing_stream_->AbortAndReset();

    return ScriptPromise::CastUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(outgoing_stream_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  const Member<OutgoingStream> outgoing_stream_;
};

OutgoingStream::CachedDataBuffer::CachedDataBuffer(v8::Isolate* isolate,
                                                   const uint8_t* data,
                                                   size_t length)
    : isolate_(isolate), length_(length) {
  // We use the BufferPartition() allocator here to allow big enough
  // allocations, and to do proper accounting of the used memory. If
  // BufferPartition() will ever not be able to provide big enough allocations,
  // e.g. because bigger ArrayBuffers get supported, then we have to switch to
  // another allocator, e.g. the ArrayBuffer allocator.
  buffer_ = reinterpret_cast<uint8_t*>(
      WTF::Partitions::BufferPartition()->Alloc(length, "OutgoingStream"));
  memcpy(buffer_, data, length);
  isolate_->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(length));
}

OutgoingStream::CachedDataBuffer::~CachedDataBuffer() {
  WTF::Partitions::BufferPartition()->Free(buffer_);
  isolate_->AdjustAmountOfExternalAllocatedMemory(
      -static_cast<int64_t>(length_));
}

OutgoingStream::OutgoingStream(ScriptState* script_state,
                               Client* client,
                               mojo::ScopedDataPipeProducerHandle handle)
    : script_state_(script_state),
      client_(client),
      data_pipe_(std::move(handle)),
      write_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      close_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC) {}

OutgoingStream::~OutgoingStream() = default;

void OutgoingStream::Init(ExceptionState& exception_state) {
  DVLOG(1) << "OutgoingStream::Init() this=" << this;
  auto* stream = MakeGarbageCollected<WritableStream>();
  InitWithExistingWritableStream(stream, exception_state);
}

void OutgoingStream::InitWithExistingWritableStream(
    WritableStream* stream,
    ExceptionState& exception_state) {
  write_watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                       MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                       WTF::BindRepeating(&OutgoingStream::OnHandleReady,
                                          WrapWeakPersistent(this)));
  close_watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                       MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                       WTF::BindRepeating(&OutgoingStream::OnPeerClosed,
                                          WrapWeakPersistent(this)));

  writable_ = stream;
  stream->InitWithCountQueueingStrategy(
      script_state_, MakeGarbageCollected<UnderlyingSink>(this), 1,
      /*optimizer=*/nullptr, exception_state);
  send_stream_abort_handle_ = controller_->signal()->AddAlgorithm(
      MakeGarbageCollected<SendStreamAbortAlgorithm>(this));
}

void OutgoingStream::AbortAlgorithm(OutgoingStream* stream) {
  send_stream_abort_handle_.Clear();

  // Step 6 of https://w3c.github.io/webtransport/#sendstream-create
  // 1. If stream's [[PendingOperation]] is null, then abort these steps.
  if (!stream->pending_operation_) {
    return;
  }

  // 2. Let reason be stream’s [[controller]]'s [[signal]]'s abort reason.
  ScriptValue reason = stream->controller_->signal()->reason(script_state_);

  // 3. Let abortPromise be the result of aborting stream with reason.
  // ASSERT_NO_EXCEPTION is used as OutgoingStream::UnderlyingSink::abort()
  // does not throw an exception, and hence a proper ExceptionState does not
  // have to be passed since it is not used.
  auto* underlying_sink = MakeGarbageCollected<UnderlyingSink>(stream);
  ScriptPromise abort_promise =
      underlying_sink->abort(script_state_, reason, ASSERT_NO_EXCEPTION);

  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  class ResolveFunction final : public PromiseHandler {
    // 4. Upon fulfillment of abortPromise,
   public:
    explicit ResolveFunction(ScriptValue reason,
                             ScriptPromiseResolver* resolver)
        : reason_(reason), resolver_(resolver) {}

    void CallWithLocal(ScriptState*, v8::Local<v8::Value>) override {
      //    reject promise with reason.
      resolver_->Reject(reason_);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(reason_);
      visitor->Trace(resolver_);
      PromiseHandler::Trace(visitor);
    }

   private:
    ScriptValue reason_;
    Member<ScriptPromiseResolver> resolver_;
  };

  StreamThenPromise(script_state_->GetContext(), abort_promise.V8Promise(),
                    MakeGarbageCollected<ScriptFunction>(
                        script_state_, MakeGarbageCollected<ResolveFunction>(
                                           reason, resolver)));

  // 5. Let pendingOperation be stream’s [[PendingOperation]].
  ScriptPromiseResolver* pending_operation = stream->pending_operation_;

  // 6. Set stream’s [[PendingOperation]] to null.
  stream->pending_operation_ = nullptr;

  // 7. Resolve pendingOperation with promise.
  pending_operation->Resolve(resolver->Promise());
}

void OutgoingStream::OnOutgoingStreamClosed() {
  DVLOG(1) << "OutgoingStream::OnOutgoingStreamClosed() this=" << this;

  DCHECK(close_promise_resolver_);
  pending_operation_ = nullptr;
  close_promise_resolver_->Resolve();
  close_promise_resolver_ = nullptr;
}

void OutgoingStream::Error(ScriptValue reason) {
  DVLOG(1) << "OutgoingStream::Error() this=" << this;

  ErrorStreamAbortAndReset(reason);
}

void OutgoingStream::ContextDestroyed() {
  DVLOG(1) << "OutgoingStream::ContextDestroyed() this=" << this;

  ResetPipe();
}

void OutgoingStream::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(client_);
  visitor->Trace(writable_);
  visitor->Trace(send_stream_abort_handle_);
  visitor->Trace(controller_);
  visitor->Trace(write_promise_resolver_);
  visitor->Trace(close_promise_resolver_);
  visitor->Trace(pending_operation_);
}

void OutgoingStream::OnHandleReady(MojoResult result,
                                   const mojo::HandleSignalsState&) {
  DVLOG(1) << "OutgoingStream::OnHandleReady() this=" << this
           << " result=" << result;

  switch (result) {
    case MOJO_RESULT_OK:
      WriteCachedData();
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      HandlePipeClosed();
      break;
    default:
      NOTREACHED();
  }
}

void OutgoingStream::OnPeerClosed(MojoResult result,
                                  const mojo::HandleSignalsState&) {
  DVLOG(1) << "OutgoingStream::OnPeerClosed() this=" << this
           << " result=" << result;

  switch (result) {
    case MOJO_RESULT_OK:
      HandlePipeClosed();
      break;
    default:
      NOTREACHED();
  }
}

void OutgoingStream::HandlePipeClosed() {
  DVLOG(1) << "OutgoingStream::HandlePipeClosed() this=" << this;

  ScriptState::Scope scope(script_state_);
  ErrorStreamAbortAndReset(CreateAbortException(IsLocalAbort(false)));
}

ScriptPromise OutgoingStream::SinkWrite(ScriptState* script_state,
                                        ScriptValue chunk,
                                        ExceptionState& exception_state) {
  DVLOG(1) << "OutgoingStream::SinkWrite() this=" << this;

  // There can only be one call to write() in progress at a time.
  DCHECK(!write_promise_resolver_);
  DCHECK_EQ(0u, offset_);

  auto* buffer_source = V8BufferSource::Create(
      script_state_->GetIsolate(), chunk.V8Value(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  DCHECK(buffer_source);

  if (!data_pipe_) {
    return ScriptPromise::Reject(script_state,
                                 CreateAbortException(IsLocalAbort(false)));
  }

  DOMArrayPiece array_piece(buffer_source);
  return WriteOrCacheData(script_state,
                          {array_piece.Bytes(), array_piece.ByteLength()});
}

// Attempt to write |data|. Cache anything that could not be written
// synchronously. Arrange for the cached data to be written asynchronously.
ScriptPromise OutgoingStream::WriteOrCacheData(ScriptState* script_state,
                                               base::span<const uint8_t> data) {
  DVLOG(1) << "OutgoingStream::WriteOrCacheData() this=" << this << " data=("
           << data.data() << ", " << data.size() << ")";
  size_t written = WriteDataSynchronously(data);

  if (written == data.size())
    return ScriptPromise::CastUndefined(script_state);

  DCHECK_LT(written, data.size());

  if (!data_pipe_) {
    return ScriptPromise::Reject(script_state,
                                 CreateAbortException(IsLocalAbort(false)));
  }

  DCHECK(!cached_data_);
  cached_data_ = std::make_unique<CachedDataBuffer>(
      script_state->GetIsolate(), data.data() + written, data.size() - written);
  DCHECK_EQ(offset_, 0u);
  write_watcher_.ArmOrNotify();
  write_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  pending_operation_ = write_promise_resolver_;
  return write_promise_resolver_->Promise();
}

// Write data previously cached. Arrange for any remaining data to be sent
// asynchronously. Fulfill |write_promise_resolver_| once all data has been
// written.
void OutgoingStream::WriteCachedData() {
  DVLOG(1) << "OutgoingStream::WriteCachedData() this=" << this;

  auto data = base::make_span(static_cast<uint8_t*>(cached_data_->data()),
                              cached_data_->length())
                  .subspan(offset_);
  size_t written = WriteDataSynchronously(data);

  if (written == data.size()) {
    ScriptState::Scope scope(script_state_);

    cached_data_.reset();
    offset_ = 0;
    pending_operation_ = nullptr;
    write_promise_resolver_->Resolve();
    write_promise_resolver_ = nullptr;
    return;
  }

  if (!data_pipe_) {
    cached_data_.reset();
    offset_ = 0;

    return;
  }

  offset_ += written;

  write_watcher_.ArmOrNotify();
}

// Write as much of |data| as can be written synchronously. Return the number of
// bytes written. May close |data_pipe_| as a side-effect on error.
size_t OutgoingStream::WriteDataSynchronously(base::span<const uint8_t> data) {
  DVLOG(1) << "OutgoingStream::WriteDataSynchronously() this=" << this
           << " data=(" << data.data() << ", " << data.size() << ")";
  DCHECK(data_pipe_);

  // This use of saturated cast means that we will fallback to asynchronous
  // sending if |data| is larger than 4GB. In practice we'd never be able to
  // send 4GB synchronously anyway.
  uint32_t num_bytes = base::saturated_cast<uint32_t>(data.size());
  MojoResult result =
      data_pipe_->WriteData(data.data(), &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
    case MOJO_RESULT_SHOULD_WAIT:
      return num_bytes;

    case MOJO_RESULT_FAILED_PRECONDITION:
      HandlePipeClosed();
      return 0;

    default:
      NOTREACHED();
      return 0;
  }
}

ScriptValue OutgoingStream::CreateAbortException(IsLocalAbort is_local_abort) {
  DVLOG(1) << "OutgoingStream::CreateAbortException() this=" << this
           << " is_local_abort=" << static_cast<bool>(is_local_abort);

  DOMExceptionCode code = is_local_abort ? DOMExceptionCode::kAbortError
                                         : DOMExceptionCode::kNetworkError;
  String message =
      String::Format("The stream was aborted %s",
                     is_local_abort ? "locally" : "by the remote server");

  return ScriptValue(script_state_->GetIsolate(),
                     V8ThrowDOMException::CreateOrEmpty(
                         script_state_->GetIsolate(), code, message));
}

void OutgoingStream::ErrorStreamAbortAndReset(ScriptValue reason) {
  DVLOG(1) << "OutgoingStream::ErrorStreamAbortAndReset() this=" << this;

  if (write_promise_resolver_) {
    write_promise_resolver_->Reject(reason);
    write_promise_resolver_ = nullptr;
    controller_ = nullptr;
  } else if (controller_) {
    controller_->error(script_state_, reason);
    controller_ = nullptr;
  }
  if (close_promise_resolver_) {
    pending_operation_ = nullptr;
    close_promise_resolver_->Reject(reason);
    close_promise_resolver_ = nullptr;
  }

  AbortAndReset();
}

void OutgoingStream::AbortAndReset() {
  DVLOG(1) << "OutgoingStream::AbortAndReset() this=" << this;

  DCHECK(state_ == State::kOpen || state_ == State::kSentFin);
  state_ = State::kAborted;
  client_->ForgetStream();

  ResetPipe();
}

void OutgoingStream::ResetPipe() {
  DVLOG(1) << "OutgoingStream::ResetPipe() this=" << this;

  write_watcher_.Cancel();
  close_watcher_.Cancel();
  data_pipe_.reset();
  if (cached_data_)
    cached_data_.reset();
}

void OutgoingStream::Dispose() {
  DVLOG(1) << "OutgoingStream::Dispose() this=" << this;

  ResetPipe();
}

}  // namespace blink
