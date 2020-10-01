// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/outgoing_stream.h"

#include <string.h>
#include <utility>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_stream_abort_info.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

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

    // The specification guarantees that this will only be called after all
    // pending writes have been completed.
    DCHECK(!outgoing_stream_->write_promise_resolver_);

    if (outgoing_stream_->client_) {
      outgoing_stream_->state_ = State::kSentFin;
      outgoing_stream_->client_->SendFin();
      outgoing_stream_->client_ = nullptr;
    }

    outgoing_stream_->AbortAndReset();

    // TODO(ricea): close() should wait for data to be flushed before resolving.
    // Since DataPipeProducerHandle doesn't have an API to observe the remote
    // side closing, this will have to be done out-of-band.
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise abort(ScriptState* script_state,
                      ScriptValue reason,
                      ExceptionState& exception_state) override {
    DVLOG(1) << "OutgoingStream::UnderlyingSink::abort() outgoing_stream_="
             << outgoing_stream_;

    return close(script_state, exception_state);
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

void OutgoingStream::Init() {
  DVLOG(1) << "OutgoingStream::Init() this=" << this;

  write_watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                       MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                       WTF::BindRepeating(&OutgoingStream::OnHandleReady,
                                          WrapWeakPersistent(this)));
  close_watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                       MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                       WTF::BindRepeating(&OutgoingStream::OnPeerClosed,
                                          WrapWeakPersistent(this)));

  // TODO(ricea): How do we make sure this promise is settled or detached before
  // it is destroyed?
  writing_aborted_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  writing_aborted_ = writing_aborted_resolver_->Promise();
  writable_ = WritableStream::CreateWithCountQueueingStrategy(
      script_state_, MakeGarbageCollected<UnderlyingSink>(this), 1);
}

void OutgoingStream::AbortWriting(StreamAbortInfo* stream_abort_info) {
  DVLOG(1) << "OutgoingStream::abortWriting() this=" << this;

  ErrorStreamAbortAndReset(IsLocalAbort(true));
}

void OutgoingStream::Reset() {
  DVLOG(1) << "OutgoingStream::Reset() this=" << this;

  ErrorStreamAbortAndReset(IsLocalAbort(false));
}

void OutgoingStream::ContextDestroyed() {
  DVLOG(1) << "OutgoingStream::ContextDestroyed() this=" << this;

  ResetPipe();
}

void OutgoingStream::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(client_);
  visitor->Trace(writable_);
  visitor->Trace(controller_);
  visitor->Trace(writing_aborted_);
  visitor->Trace(writing_aborted_resolver_);
  visitor->Trace(write_promise_resolver_);
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
  ErrorStreamAbortAndReset(IsLocalAbort(false));
}

ScriptPromise OutgoingStream::SinkWrite(ScriptState* script_state,
                                        ScriptValue chunk,
                                        ExceptionState& exception_state) {
  DVLOG(1) << "OutgoingStream::SinkWrite() this=" << this;

  // There can only be one call to write() in progress at a time.
  DCHECK(!write_promise_resolver_);
  DCHECK_EQ(0u, offset_);

  auto* isolate = script_state->GetIsolate();

  ArrayBufferOrArrayBufferView buffer_source;
  V8ArrayBufferOrArrayBufferView::ToImpl(
      isolate, chunk.V8Value(), buffer_source,
      UnionTypeConversionMode::kNotNullable, exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  DCHECK(!buffer_source.IsNull());

  // |data| will not be valid when this function returns.
  base::span<const uint8_t> data;
  if (buffer_source.IsArrayBuffer()) {
    const auto* array_buffer = buffer_source.GetAsArrayBuffer();
    data = base::span<const uint8_t>(
        static_cast<const uint8_t*>(array_buffer->Data()),
        array_buffer->ByteLengthAsSizeT());
  } else {
    DCHECK(buffer_source.IsArrayBufferView());
    const auto* array_buffer_view = buffer_source.GetAsArrayBufferView().View();
    data = base::span<const uint8_t>(
        static_cast<const uint8_t*>(array_buffer_view->BaseAddress()),
        array_buffer_view->byteLengthAsSizeT());
  }

  if (!data_pipe_) {
    return ScriptPromise::Reject(script_state,
                                 CreateAbortException(IsLocalAbort(false)));
  }

  return WriteOrCacheData(script_state, data);
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

void OutgoingStream::ErrorStreamAbortAndReset(IsLocalAbort is_local_abort) {
  DVLOG(1) << "OutgoingStream::ErrorStreamAbortAndReset() this=" << this;

  ScriptValue exception = CreateAbortException(is_local_abort);

  if (write_promise_resolver_) {
    write_promise_resolver_->Reject(exception);
    write_promise_resolver_ = nullptr;
    controller_ = nullptr;
  } else if (controller_) {
    controller_->error(script_state_, exception);
    controller_ = nullptr;
  }

  AbortAndReset();
}

void OutgoingStream::AbortAndReset() {
  DVLOG(1) << "OutgoingStream::AbortAndReset() this=" << this;

  if (writing_aborted_resolver_) {
    // TODO(ricea): Set errorCode on the StreamAbortInfo.
    writing_aborted_resolver_->Resolve(StreamAbortInfo::Create());
    writing_aborted_resolver_ = nullptr;
  }

  if (client_) {
    DCHECK_EQ(state_, State::kOpen);
    state_ = State::kAborted;
    client_->OnOutgoingStreamAbort();
    client_ = nullptr;
  }

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
