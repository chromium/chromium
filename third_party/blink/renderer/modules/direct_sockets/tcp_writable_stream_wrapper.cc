// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

  ScriptState::Scope scope(script_state);

  auto* sink = WritableStreamWrapper::MakeForwardingUnderlyingSink(this);
  SetSink(sink);

  // Set the CountQueueingStrategy's high water mark as 1 to make the logic of
  // |WriteOrCacheData| much simpler.
  auto* writable = WritableStream::CreateWithCountQueueingStrategy(
      script_state, sink, /*high_water_mark=*/1);
  SetWritable(writable);
}

bool TCPWritableStreamWrapper::HasPendingWrite() const {
  return !!write_promise_resolver_;
}

void TCPWritableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(buffer_source_);
  visitor->Trace(write_promise_resolver_);
  WritableStreamWrapper::Trace(visitor);
}

void TCPWritableStreamWrapper::OnHandleReady(MojoResult result,
                                             const mojo::HandleSignalsState&) {
  switch (result) {
    case MOJO_RESULT_OK:
      WriteDataAsynchronously();
      break;

    case MOJO_RESULT_FAILED_PRECONDITION:
      // Will be handled by |close_watcher_|.
      break;

    default:
      NOTREACHED_IN_MIGRATION();
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

ScriptPromise<IDLUndefined> TCPWritableStreamWrapper::Write(
    ScriptValue chunk,
    ExceptionState& exception_state) {
  // There can only be one call to write() in progress at a time.
  DCHECK(!write_promise_resolver_);
  DCHECK(!buffer_source_);
  DCHECK_EQ(0u, offset_);

  if (!data_pipe_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNetworkError,
        "The underlying data pipe was disconnected.");
    return EmptyPromise();
  }

  buffer_source_ = V8BufferSource::Create(GetScriptState()->GetIsolate(),
                                          chunk.V8Value(), exception_state);
  if (exception_state.HadException()) {
    return EmptyPromise();
  }
  DCHECK(buffer_source_);

  write_promise_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          GetScriptState(), exception_state.GetContext());
  auto promise = write_promise_resolver_->Promise();

  WriteDataAsynchronously();

  return promise;
}

void TCPWritableStreamWrapper::WriteDataAsynchronously() {
  DCHECK(data_pipe_);
  DCHECK(buffer_source_);

  DOMArrayPiece array_piece(buffer_source_);
  // From https://webidl.spec.whatwg.org/#dfn-get-buffer-source-copy, if the
  // buffer source is detached then an empty byte sequence is returned, which
  // means the write is complete.
  if (array_piece.IsDetached()) {
    FinalizeWrite();
    return;
  }
  auto data = base::make_span(array_piece.Bytes(), array_piece.ByteLength())
                  .subspan(offset_);
  size_t written = WriteDataSynchronously(data);

  DCHECK_LE(offset_ + written, array_piece.ByteLength());
  if (offset_ + written == array_piece.ByteLength()) {
    FinalizeWrite();
    return;
  }
  offset_ += written;

  write_watcher_.ArmOrNotify();
}

// Write as much of |data| as can be written synchronously. Return the number of
// bytes written. May close |data_pipe_| as a side-effect on error.
size_t TCPWritableStreamWrapper::WriteDataSynchronously(
    base::span<const uint8_t> data) {
  size_t actually_written_bytes = 0;
  MojoResult result = data_pipe_->WriteData(data, MOJO_WRITE_DATA_FLAG_NONE,
                                            actually_written_bytes);

  switch (result) {
    case MOJO_RESULT_OK:
    case MOJO_RESULT_SHOULD_WAIT:
      return actually_written_bytes;

    case MOJO_RESULT_FAILED_PRECONDITION:
      // Will be handled by |close_watcher_|.
      return 0;

    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

void TCPWritableStreamWrapper::FinalizeWrite() {
  buffer_source_ = nullptr;
  offset_ = 0;
  write_promise_resolver_->Resolve();
  write_promise_resolver_ = nullptr;
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
  std::move(on_close_).Run(/*exception=*/ScriptValue());
}

void TCPWritableStreamWrapper::ErrorStream(int32_t error_code) {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kAborted);

  auto message =
      String{"Stream aborted by the remote: " + net::ErrorToString(error_code)};

  auto* script_state = write_promise_resolver_
                           ? write_promise_resolver_->GetScriptState()
                           : GetScriptState();
  // Scope is needed because there's no ScriptState* on the call stack for
  // ScriptValue.
  ScriptState::Scope scope{script_state};

  auto exception = ScriptValue(script_state->GetIsolate(),
                               V8ThrowDOMException::CreateOrDie(
                                   script_state->GetIsolate(),
                                   DOMExceptionCode::kNetworkError, message));

  // Can be already reset due to HandlePipeClosed() called previously.
  if (data_pipe_) {
    ResetPipe();
  }

  if (write_promise_resolver_) {
    write_promise_resolver_->Reject(exception);
    write_promise_resolver_ = nullptr;
  } else {
    Controller()->error(script_state, exception);
  }

  std::move(on_close_).Run(exception);
}

void TCPWritableStreamWrapper::ResetPipe() {
  write_watcher_.Cancel();
  close_watcher_.Cancel();
  data_pipe_.reset();
  buffer_source_ = nullptr;
  offset_ = 0;
}

void TCPWritableStreamWrapper::Dispose() {
  ResetPipe();
}

}  // namespace blink
