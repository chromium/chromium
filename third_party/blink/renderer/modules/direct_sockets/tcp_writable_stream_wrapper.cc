// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_writable_stream_wrapper.h"

#include "base/notreached.h"
#include "mojo/public/cpp/system/handle_signals_state.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event_target_impl.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

TCPWritableStreamWrapper::CachedDataBuffer::CachedDataBuffer(
    v8::Isolate* isolate,
    const uint8_t* data,
    size_t length)
    : isolate_(isolate), length_(length) {
  // We use the BufferPartition() allocator here to allow big enough
  // allocations, and to do proper accounting of the used memory. If
  // BufferPartition() will ever not be able to provide big enough allocations,
  // e.g. because bigger ArrayBuffers get supported, then we have to switch to
  // another allocator, e.g. the ArrayBuffer allocator.
  buffer_ = std::unique_ptr<uint8_t[], OnFree>(
      reinterpret_cast<uint8_t*>(WTF::Partitions::BufferPartition()->Alloc(
          length, "TCPWritableStreamWrapper")));
  memcpy(buffer_.get(), data, length);
  isolate_->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(length));
}

TCPWritableStreamWrapper::CachedDataBuffer::~CachedDataBuffer() {
  isolate_->AdjustAmountOfExternalAllocatedMemory(
      -static_cast<int64_t>(length_));
}

TCPWritableStreamWrapper::TCPWritableStreamWrapper(
    ScriptState* script_state,
    CloseOnceCallback on_close,
    mojo::ScopedDataPipeProducerHandle handle)
    : WritableStreamWrapper(script_state),
      on_close_(std::move(on_close)),
      data_pipe_(std::move(handle)),
      write_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      close_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC) {
  write_watcher_.Watch(
      data_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      WTF::BindRepeating(&TCPWritableStreamWrapper::OnHandleReady,
                         WrapWeakPersistent(this)));

  close_watcher_.Watch(
      data_pipe_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      WTF::BindRepeating(&TCPWritableStreamWrapper::OnHandleReset,
                         WrapWeakPersistent(this)));

  // Set the CountQueueingStrategy's high water mark as 1 to make the logic of
  // |WriteOrCacheData| much simpler
  InitSinkAndWritable(/*sink=*/MakeGarbageCollected<UnderlyingSink>(this),
                      /*high_water_mark=*/1);
}

bool TCPWritableStreamWrapper::HasPendingWrite() const {
  return !!write_promise_resolver_;
}

void TCPWritableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(write_promise_resolver_);
  WritableStreamWrapper::Trace(visitor);
}

void TCPWritableStreamWrapper::OnHandleReady(MojoResult result,
                                             const mojo::HandleSignalsState&) {
  switch (result) {
    case MOJO_RESULT_OK:
      WriteCachedData();
      break;

    case MOJO_RESULT_FAILED_PRECONDITION:
      // Will be handled by |close_watcher_|.
      break;

    default:
      NOTREACHED();
  }
}

void TCPWritableStreamWrapper::OnHandleReset(MojoResult result,
                                             const mojo::HandleSignalsState&) {
  DCHECK_EQ(result, MOJO_RESULT_OK);
  ResetPipe();
}

void TCPWritableStreamWrapper::OnAbortSignal() {
  if (write_promise_resolver_) {
    write_promise_resolver_->Reject(
        Controller()->signal()->reason(GetScriptState()));
    write_promise_resolver_ = nullptr;
  }
}

ScriptPromise TCPWritableStreamWrapper::Write(ScriptValue chunk,
                                              ExceptionState& exception_state) {
  // There can only be one call to write() in progress at a time.
  DCHECK(!write_promise_resolver_);
  DCHECK_EQ(0u, offset_);

  if (!data_pipe_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        "The underlying data pipe was disconnected.");
    return ScriptPromise();
  }

  auto* buffer_source = V8BufferSource::Create(
      GetScriptState()->GetIsolate(), chunk.V8Value(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();
  DCHECK(buffer_source);

  DOMArrayPiece array_piece(buffer_source);
  return WriteOrCacheData({array_piece.Bytes(), array_piece.ByteLength()},
                          exception_state);
}

// Attempt to write |data|. Cache anything that could not be written
// synchronously. Arrange for the cached data to be written asynchronously.
ScriptPromise TCPWritableStreamWrapper::WriteOrCacheData(
    base::span<const uint8_t> data,
    ExceptionState& exception_state) {
  DCHECK(data_pipe_);
  size_t written = WriteDataSynchronously(data);

  if (written == data.size())
    return ScriptPromise::CastUndefined(GetScriptState());

  DCHECK_LT(written, data.size());

  DCHECK(!cached_data_);
  cached_data_ = std::make_unique<CachedDataBuffer>(
      GetScriptState()->GetIsolate(), data.data() + written,
      data.size() - written);
  DCHECK_EQ(offset_, 0u);
  write_watcher_.ArmOrNotify();
  write_promise_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(
      GetScriptState(), exception_state.GetContext());
  return write_promise_resolver_->Promise();
}

// Write data previously cached. Arrange for any remaining data to be sent
// asynchronously. Fulfill |write_promise_resolver_| once all data has been
// written.
void TCPWritableStreamWrapper::WriteCachedData() {
  auto data = base::make_span(static_cast<uint8_t*>(cached_data_->data()),
                              cached_data_->length())
                  .subspan(offset_);
  size_t written = WriteDataSynchronously(data);

  if (written == data.size()) {
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
size_t TCPWritableStreamWrapper::WriteDataSynchronously(
    base::span<const uint8_t> data) {
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
      // Will be handled by |close_watcher_|.
      return 0;

    default:
      NOTREACHED();
      return 0;
  }
}

void TCPWritableStreamWrapper::CloseStream() {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kClosed);
  DCHECK(!write_promise_resolver_);

  // If close request came from writer.close() or writer.abort(), the internal
  // state of the stream is already set to closed.  Therefore we don't have to
  // do anything with the controller.
  if (!data_pipe_) {
    // This is a rare case indicating that writer.close/abort() interrupted
    // the OnWriteError() call where the pipe already got reset, but the
    // corresponding IPC hasn't yet arrived. The simplest way is to abort
    // CloseStream by setting state to Open and allow the IPC to finish the
    // job.
    SetState(State::kOpen);
    return;
  }

  ResetPipe();
  std::move(on_close_).Run(/*error=*/false);
}

void TCPWritableStreamWrapper::ErrorStream(int32_t error_code) {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kAborted);

  auto message =
      String{"Stream aborted by the remote: " + net::ErrorToString(error_code)};
  auto* exception = MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNetworkError, message);

  // Can be already reset due to HandlePipeClosed() called previously.
  if (data_pipe_) {
    ResetPipe();
  }

  auto* script_state = GetScriptState();
  DCHECK(script_state->ContextIsValid());

  ScriptState::Scope scope{script_state};
  if (write_promise_resolver_) {
    write_promise_resolver_->RejectWithDOMException(
        DOMExceptionCode::kNetworkError, message);
    write_promise_resolver_ = nullptr;
  } else {
    auto* script_state = GetScriptState();
    Controller()->error(script_state,
                        ScriptValue::From(script_state, exception));
  }

  std::move(on_close_).Run(/*error=*/true);
}

void TCPWritableStreamWrapper::ResetPipe() {
  write_watcher_.Cancel();
  close_watcher_.Cancel();
  data_pipe_.reset();
  if (cached_data_) {
    cached_data_.reset();
  }
}

void TCPWritableStreamWrapper::Dispose() {
  ResetPipe();
}

}  // namespace blink
