// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_writable_stream_wrapper.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
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

// An implementation of UnderlyingSinkBase that forwards all operations to the
// TCPWritableStreamWrapper object that created it.
class TCPWritableStreamWrapper::TCPUnderlyingSink final
    : public WritableStreamWrapper::UnderlyingSink {
 public:
  explicit TCPUnderlyingSink(TCPWritableStreamWrapper* writable_stream_wrapper)
      : WritableStreamWrapper::UnderlyingSink(writable_stream_wrapper) {}

  ScriptPromise close(ScriptState* script_state, ExceptionState&) override {
    GetWritableStreamWrapper()->CloseSocket(/*error=*/false);
    return ScriptPromise::CastUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    WritableStreamWrapper::UnderlyingSink::Trace(visitor);
  }
};

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
    base::OnceCallback<void(bool)> on_close,
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
      WTF::BindRepeating(&TCPWritableStreamWrapper::OnPeerClosed,
                         WrapWeakPersistent(this)));

  // Set the CountQueueingStrategy's high water mark as 1 to make the logic of
  // |WriteOrCacheData| much simpler
  InitSinkAndWritable(/*sink=*/MakeGarbageCollected<TCPUnderlyingSink>(this),
                      /*high_water_mark=*/1);
}

void TCPWritableStreamWrapper::CloseSocket(bool error) {
  if (on_close_) {
    DCHECK_EQ(GetState(), State::kOpen);
    std::move(on_close_).Run(error);
  }
  DCHECK_NE(GetState(), State::kOpen);
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
  DVLOG(1) << "TCPWritableStreamWrapper::OnHandleReady() this=" << this
           << " result=" << result;

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

void TCPWritableStreamWrapper::OnPeerClosed(MojoResult result,
                                            const mojo::HandleSignalsState&) {
  DVLOG(1) << "TCPWritableStreamWrapper::OnPeerClosed() this=" << this
           << " result=" << result;

  DCHECK_EQ(result, MOJO_RESULT_OK);
  DCHECK_EQ(GetState(), State::kOpen);

  CloseSocket(/*error=*/true);
}

ScriptPromise TCPWritableStreamWrapper::Write(ScriptValue chunk,
                                              ExceptionState& exception_state) {
  DVLOG(1) << "TCPWritableStreamWrapper::Write() this=" << this;

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
  return WriteOrCacheData({array_piece.Bytes(), array_piece.ByteLength()});
}

// Attempt to write |data|. Cache anything that could not be written
// synchronously. Arrange for the cached data to be written asynchronously.
ScriptPromise TCPWritableStreamWrapper::WriteOrCacheData(
    base::span<const uint8_t> data) {
  DVLOG(1) << "TCPWritableStreamWrapper::WriteOrCacheData() this=" << this
           << " data=(" << data.data() << ", " << data.size() << ")";
  size_t written = WriteDataSynchronously(data);

  if (written == data.size())
    return ScriptPromise::CastUndefined(GetScriptState());

  DCHECK_LT(written, data.size());

  if (!data_pipe_) {
    ScriptState::Scope scope(GetScriptState());
    return ScriptPromise::Reject(
        GetScriptState(),
        CreateException(GetScriptState(), DOMExceptionCode::kInvalidStateError,
                        "Pipe is disconnected."));
  }

  DCHECK(!cached_data_);
  cached_data_ = std::make_unique<CachedDataBuffer>(
      GetScriptState()->GetIsolate(), data.data() + written,
      data.size() - written);
  DCHECK_EQ(offset_, 0u);
  write_watcher_.ArmOrNotify();
  write_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver>(GetScriptState());
  return write_promise_resolver_->Promise();
}

// Write data previously cached. Arrange for any remaining data to be sent
// asynchronously. Fulfill |write_promise_resolver_| once all data has been
// written.
void TCPWritableStreamWrapper::WriteCachedData() {
  DVLOG(1) << "TCPWritableStreamWrapper::WriteCachedData() this=" << this;

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
  DVLOG(1) << "TCPWritableStreamWrapper::WriteDataSynchronously() this=" << this
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
      // Will be handled by |close_watcher_|.
      return 0;

    default:
      NOTREACHED();
      return 0;
  }
}

void TCPWritableStreamWrapper::CloseStream(bool error) {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(error ? State::kAborted : State::kClosed);

  {
    ScriptState::Scope scope(GetScriptState());
    ScriptValue exception =
        error
            ? CreateException(GetScriptState(), DOMExceptionCode::kNetworkError,
                              "Connection aborted by remote")
            : CreateException(GetScriptState(),
                              DOMExceptionCode::kInvalidStateError,
                              "Stream closed.");
    if (write_promise_resolver_) {
      write_promise_resolver_->Reject(exception);
      write_promise_resolver_ = nullptr;
    }

    Controller()->error(GetScriptState(), exception);
  }

  on_close_.Reset();

  ResetPipe();
}

void TCPWritableStreamWrapper::ResetPipe() {
  DVLOG(1) << "TCPWritableStreamWrapper::ResetPipe() this=" << this;

  write_watcher_.Cancel();
  close_watcher_.Cancel();
  data_pipe_.reset();
  if (cached_data_) {
    cached_data_.reset();
  }
}

void TCPWritableStreamWrapper::Dispose() {
  DVLOG(1) << "TCPWritableStreamWrapper::Dispose() this=" << this;

  ResetPipe();
}

}  // namespace blink
