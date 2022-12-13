// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/direct_sockets/stream_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

TCPReadableStreamWrapper::TCPReadableStreamWrapper(
    ScriptState* script_state,
    CloseOnceCallback on_close,
    mojo::ScopedDataPipeConsumerHandle handle)
    : ReadableStreamWrapper(script_state),
      on_close_(std::move(on_close)),
      data_pipe_(std::move(handle)),
      read_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      close_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC) {
  read_watcher_.Watch(
      data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      WTF::BindRepeating(&TCPReadableStreamWrapper::OnHandleReady,
                         WrapWeakPersistent(this)));

  close_watcher_.Watch(
      data_pipe_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      WTF::BindRepeating(&TCPReadableStreamWrapper::OnHandleReset,
                         WrapWeakPersistent(this)));

  // Set queuing strategy of default behavior with a high water mark of 0.
  InitSourceAndReadable(
      /*source=*/MakeGarbageCollected<UnderlyingSource>(script_state, this),
      /*high_water_mark=*/0);
}

void TCPReadableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(pending_exception_);
  ReadableStreamWrapper::Trace(visitor);
}

void TCPReadableStreamWrapper::OnHandleReady(MojoResult result,
                                             const mojo::HandleSignalsState&) {
  switch (result) {
    case MOJO_RESULT_OK:
      Pull();
      break;

    case MOJO_RESULT_FAILED_PRECONDITION:
      // Will be handled by |close_watcher_|.
      break;

    default:
      NOTREACHED();
  }
}

void TCPReadableStreamWrapper::Pull() {
  if (!GetScriptState()->ContextIsValid())
    return;

  DCHECK(data_pipe_);

  const void* buffer = nullptr;
  uint32_t buffer_num_bytes = 0;
  auto result = data_pipe_->BeginReadData(&buffer, &buffer_num_bytes,
                                          MOJO_BEGIN_READ_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK: {
      Push(base::make_span(static_cast<const uint8_t*>(buffer),
                           buffer_num_bytes),
           {});
      result = data_pipe_->EndReadData(buffer_num_bytes);
      DCHECK_EQ(result, MOJO_RESULT_OK);

      break;
    }

    case MOJO_RESULT_SHOULD_WAIT:
      read_watcher_.ArmOrNotify();
      return;

    case MOJO_RESULT_FAILED_PRECONDITION:
      // Will be handled by |close_watcher_|.
      return;

    default:
      NOTREACHED() << "Unexpected result: " << result;
      return;
  }
}

bool TCPReadableStreamWrapper::Push(base::span<const uint8_t> data,
                                    const absl::optional<net::IPEndPoint>&) {
  auto* buffer = DOMUint8Array::Create(data.data(), data.size_bytes());
  Controller()->Enqueue(buffer);

  return true;
}

void TCPReadableStreamWrapper::CloseStream() {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(State::kClosed);

  // If close request came from reader.cancel(), the internal state of the
  // stream is already set to closed. Therefore we don't have to do anything
  // with the controller.
  if (!data_pipe_) {
    // This is a rare case indicating that reader.cancel() interrupted the
    // OnReadError() call where the pipe already got reset, but the
    // corresponding IPC hasn't yet arrived. The simplest way is to abort
    // CloseStream by setting state to Open and allow the IPC to finish the
    // job.
    SetState(State::kOpen);
    return;
  }

  ResetPipe();
  std::move(on_close_).Run(ScriptValue());
  return;
}

void TCPReadableStreamWrapper::ErrorStream(int32_t error_code) {
  if (GetState() != State::kOpen) {
    return;
  }
  graceful_peer_shutdown_ = (error_code == net::OK);

  if (graceful_peer_shutdown_) {
    SetState(State::kClosed);
    if (!data_pipe_) {
      Controller()->Close();
      std::move(on_close_).Run(ScriptValue());
    }
    return;
  }

  SetState(State::kAborted);

  auto* script_state = GetScriptState();
  // Scope is needed because there's no ScriptState* on the call stack for
  // ScriptValue::From.
  ScriptState::Scope scope{script_state};

  auto exception = ScriptValue::From(
      script_state,
      V8ThrowDOMException::CreateOrDie(script_state->GetIsolate(),
                                       DOMExceptionCode::kNetworkError,
                                       String{"Stream aborted by the remote: " +
                                              net::ErrorToString(error_code)}));

  if (data_pipe_) {
    pending_exception_ = exception;
    return;
  }

  Controller()->Error(exception);
  std::move(on_close_).Run(exception);
}

void TCPReadableStreamWrapper::ResetPipe() {
  read_watcher_.Cancel();
  close_watcher_.Cancel();
  data_pipe_.reset();
}

void TCPReadableStreamWrapper::Dispose() {
  ResetPipe();
}

void TCPReadableStreamWrapper::OnHandleReset(MojoResult result,
                                             const mojo::HandleSignalsState&) {
#if DCHECK_IS_ON()
  DCHECK_EQ(result, MOJO_RESULT_OK);
  DCHECK(data_pipe_);
  DCHECK(on_close_);
  DCHECK(!(!pending_exception_.IsEmpty() && graceful_peer_shutdown_));
  if (!pending_exception_.IsEmpty() || graceful_peer_shutdown_) {
    DCHECK_NE(GetState(), State::kOpen);
  } else {
    DCHECK_EQ(GetState(), State::kOpen);
  }
#endif

  ResetPipe();

  if (!pending_exception_.IsEmpty()) {
    Controller()->Error(pending_exception_);

    SetState(State::kAborted);
    std::move(on_close_).Run(pending_exception_);

    pending_exception_.Clear();
  } else if (graceful_peer_shutdown_) {
    Controller()->Close();

    SetState(State::kClosed);
    std::move(on_close_).Run(ScriptValue());
  }
}

}  // namespace blink
