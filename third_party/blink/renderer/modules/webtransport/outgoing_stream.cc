// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/outgoing_stream.h"

#include <cstring>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "partition_alloc/partition_alloc.h"
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
#include "third_party/blink/renderer/platform/bindings/v8_external_memory_accounter.h"
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

struct CachedDataBufferDeleter {
  void operator()(void* buffer) {
    WTF::Partitions::BufferPartition()->Free(buffer);
  }
};

}  // namespace

class OutgoingStream::UnderlyingSink final : public UnderlyingSinkBase {
 public:
  explicit UnderlyingSink(OutgoingStream* outgoing_stream)
      : outgoing_stream_(outgoing_stream) {}

  // Implementation of UnderlyingSinkBase
  ScriptPromise<IDLUndefined> start(ScriptState* script_state,
                                    WritableStreamDefaultController* controller,
                                    ExceptionState&) override {
    DVLOG(1) << "OutgoingStream::UnderlyinkSink::start() outgoing_stream_="
             << outgoing_stream_;

    outgoing_stream_->controller_ = controller;
    return ToResolvedUndefinedPromise(script_state);
  }

  ScriptPromise<IDLUndefined> write(ScriptState* script_state,
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

  ScriptPromise<IDLUndefined> close(ScriptState* script_state,
                                    ExceptionState&) override {
    DVLOG(1) << "OutgoingStream::UnderlingSink::close() outgoing_stream_="
             << outgoing_stream_;

    // The streams specification guarantees that this will only be called after
    // all pending writes have been completed.
    DCHECK(!outgoing_stream_->write_promise_resolver_);

    DCHECK(!outgoing_stream_->close_promise_resolver_);

    outgoing_stream_->close_promise_resolver_ =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
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

  ScriptPromise<IDLUndefined> abort(ScriptState* script_state,
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

    return ToResolvedUndefinedPromise(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(outgoing_stream_);
    UnderlyingSinkBase::Trace(visitor);
  }

 private:
  const Member<OutgoingStream> outgoing_stream_;
};

class OutgoingStream::CachedDataBuffer {
 public:
  using HeapBuffer = base::HeapArray<uint8_t, CachedDataBufferDeleter>;

  CachedDataBuffer(v8::Isolate* isolate, base::span<const uint8_t> data)
      : isolate_(isolate) {
    // We use the BufferPartition() allocator here to allow big enough
    // allocations, and to do proper accounting of the used memory. If
    // BufferPartition() will ever not be able to provide big enough
    // allocations, e.g. because bigger ArrayBuffers get supported, then we
    // have to switch to another allocator, e.g. the ArrayBuffer allocator.
    void* memory_buffer = WTF::Partitions::BufferPartition()->Alloc(
        data.size(), "OutgoingStream");
    // SAFETY: WTF::Partitions::BufferPartition()->Alloc() returns a valid
    // pointer to at least data.size() bytes.
    buffer_ = UNSAFE_BUFFERS(HeapBuffer::FromOwningPointer(
        reinterpret_cast<uint8_t*>(memory_buffer), data.size()));
    buffer_.copy_from(data);
    external_memory_accounter_.Increase(isolate_.get(), buffer_.size());
  }

  ~CachedDataBuffer() {
    external_memory_accounter_.Decrease(isolate_.get(), buffer_.size());
  }

  base::span<const uint8_t> span() const { return buffer_; }

 private:
  // We need the isolate to report memory to
  // |external_memory_accounter_| for the memory stored in |buffer_|.
  raw_ptr<v8::Isolate> isolate_;
  HeapBuffer buffer_;
  NO_UNIQUE_ADDRESS V8ExternalMemoryAccounterBase external_memory_accounter_;
};

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

  // Step 7 of https://w3c.github.io/webtransport/#webtransportsendstream-create
  // 1. Let pendingOperation be stream.[[PendingOperation]].
  // 2. If pendingOperation is null, then abort these steps.
  auto* pending_operation = stream->pending_operation_.Get();
  if (!pending_operation) {
    return;
  }

  // 3. Set stream.[[PendingOperation]] to null.
  stream->pending_operation_ = nullptr;

  // 4. Let reason be abortSignal’s abort reason.
  ScriptValue reason = stream->controller_->signal()->reason(script_state_);

  // 5. Let promise be the result of aborting stream with reason.
  // ASSERT_NO_EXCEPTION is used as OutgoingStream::UnderlyingSink::abort()
  // does not throw an exception, and hence a proper ExceptionState does not
  // have to be passed since it is not used.
  auto* underlying_sink = MakeGarbageCollected<UnderlyingSink>(stream);
  ScriptPromiseUntyped abort_promise =
      underlying_sink->abort(script_state_, reason, ASSERT_NO_EXCEPTION);

  // 6. Upon fulfillment of promise, reject pendingOperation with reason.
  class ResolveFunction final : public PromiseHandler {
   public:
    ResolveFunction(ScriptValue reason,
                    ScriptPromiseResolver<IDLUndefined>* resolver)
        : reason_(reason), resolver_(resolver) {}

    void CallWithLocal(ScriptState*, v8::Local<v8::Value>) override {
      resolver_->Reject(reason_);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(reason_);
      visitor->Trace(resolver_);
      PromiseHandler::Trace(visitor);
    }

   private:
    ScriptValue reason_;
    Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  };
  StreamThenPromise(script_state_->GetContext(), abort_promise.V8Promise(),
                    MakeGarbageCollected<ScriptFunction>(
                        script_state_, MakeGarbageCollected<ResolveFunction>(
                                           reason, pending_operation)));
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
      NOTREACHED_IN_MIGRATION();
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
      NOTREACHED_IN_MIGRATION();
  }
}

void OutgoingStream::HandlePipeClosed() {
  DVLOG(1) << "OutgoingStream::HandlePipeClosed() this=" << this;

  ScriptState::Scope scope(script_state_);
  ErrorStreamAbortAndReset(CreateAbortException(IsLocalAbort(false)));
}

ScriptPromise<IDLUndefined> OutgoingStream::SinkWrite(
    ScriptState* script_state,
    ScriptValue chunk,
    ExceptionState& exception_state) {
  DVLOG(1) << "OutgoingStream::SinkWrite() this=" << this;

  // There can only be one call to write() in progress at a time.
  DCHECK(!write_promise_resolver_);
  DCHECK_EQ(0u, offset_);

  auto* buffer_source = V8BufferSource::Create(
      script_state_->GetIsolate(), chunk.V8Value(), exception_state);
  if (exception_state.HadException())
    return EmptyPromise();
  DCHECK(buffer_source);

  if (!data_pipe_) {
    return ScriptPromise<IDLUndefined>::Reject(
        script_state, CreateAbortException(IsLocalAbort(false)));
  }

  DOMArrayPiece array_piece(buffer_source);
  return WriteOrCacheData(script_state, array_piece.ByteSpan());
}

// Attempt to write |data|. Cache anything that could not be written
// synchronously. Arrange for the cached data to be written asynchronously.
ScriptPromise<IDLUndefined> OutgoingStream::WriteOrCacheData(
    ScriptState* script_state,
    base::span<const uint8_t> data) {
  DVLOG(1) << "OutgoingStream::WriteOrCacheData() this=" << this << " data=("
           << static_cast<const void*>(data.data()) << ", " << data.size()
           << ")";
  size_t written = WriteDataSynchronously(data);

  if (written == data.size())
    return ToResolvedUndefinedPromise(script_state);

  DCHECK_LT(written, data.size());

  if (!data_pipe_) {
    return ScriptPromise<IDLUndefined>::Reject(
        script_state, CreateAbortException(IsLocalAbort(false)));
  }

  DCHECK(!cached_data_);
  cached_data_ = std::make_unique<CachedDataBuffer>(script_state->GetIsolate(),
                                                    data.subspan(written));
  DCHECK_EQ(offset_, 0u);
  write_watcher_.ArmOrNotify();
  write_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  pending_operation_ = write_promise_resolver_;
  return write_promise_resolver_->Promise();
}

// Write data previously cached. Arrange for any remaining data to be sent
// asynchronously. Fulfill |write_promise_resolver_| once all data has been
// written.
void OutgoingStream::WriteCachedData() {
  DVLOG(1) << "OutgoingStream::WriteCachedData() this=" << this;

  auto data = cached_data_->span().subspan(offset_);
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
           << " data=(" << static_cast<const void*>(data.data()) << ", "
           << data.size() << ")";
  DCHECK(data_pipe_);

  size_t actually_written_bytes = 0;
  MojoResult result = data_pipe_->WriteData(data, MOJO_WRITE_DATA_FLAG_NONE,
                                            actually_written_bytes);
  switch (result) {
    case MOJO_RESULT_OK:
      return actually_written_bytes;

    case MOJO_RESULT_SHOULD_WAIT:
      return 0;

    case MOJO_RESULT_FAILED_PRECONDITION:
      HandlePipeClosed();
      return 0;

    default:
      NOTREACHED_IN_MIGRATION();
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
