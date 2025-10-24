// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_request.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
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
    mojo::ScopedDataPipeConsumerHandle handle,
    uint64_t inspector_id)
    : ReadableByteStreamWrapper(script_state),
      on_close_(std::move(on_close)),
      data_pipe_(std::move(handle)),
      read_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      close_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC),
      inspector_id_(inspector_id) {
  read_watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                      BindRepeating(&TCPReadableStreamWrapper::OnHandleReady,
                                    WrapWeakPersistent(this)));

  close_watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                       MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                       BindRepeating(&TCPReadableStreamWrapper::OnHandleReset,
                                     WrapWeakPersistent(this)));

  ScriptState::Scope scope(script_state);

  auto* source =
      ReadableByteStreamWrapper::MakeForwardingUnderlyingByteSource(this);
  SetSource(source);

  auto* readable = ReadableStream::CreateByteStream(script_state, source);
  SetReadable(readable);

  // UnderlyingByteSourceBase doesn't expose Controller() until the first call
  // to Pull(); this becomes problematic if the socket is errored beforehand -
  // calls to close() / error() will be invoked on a nullptr. Hence we obtain
  // the controller directly.
  auto* controller =
      To<ReadableByteStreamController>(readable->GetController());
  DCHECK(controller);
  SetController(controller);
}

void TCPReadableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(pending_exception_);
  ReadableByteStreamWrapper::Trace(visitor);
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

  base::span<const uint8_t> data_buffer;
  auto result =
      data_pipe_->BeginReadData(MOJO_BEGIN_READ_DATA_FLAG_NONE, data_buffer);
  switch (result) {
    case MOJO_RESULT_OK: {
      // respond() or enqueue() will only throw if their arguments are invalid
      // or the stream is errored. The code below guarantees that the length is
      // in range and the chunk is a valid view. If the stream becomes errored
      // then this method cannot be called because the watcher is disarmed.
      NonThrowableExceptionState exception_state;

      auto* script_state = GetScriptState();
      ScriptState::Scope scope(script_state);

      if (ReadableStreamBYOBRequest* request = Controller()->byobRequest()) {
        DOMArrayPiece view(request->view().Get());
        data_buffer =
            data_buffer.first(std::min(data_buffer.size(), view.ByteLength()));
        view.ByteSpan().copy_prefix_from(data_buffer);
        request->respond(script_state, data_buffer.size(), exception_state);
      } else {
        auto buffer = NotShared(DOMUint8Array::Create(data_buffer));
        Controller()->enqueue(script_state, buffer, exception_state);
      }

      // It is necessary to check |data_pipe_| as |enqueue()| may run
      // JavaScript, leading to invalidation of |data_pipe_|.
      if (!data_pipe_.is_valid()) {
        return;
      }
      result = data_pipe_->EndReadData(data_buffer.size());
      DCHECK_EQ(result, MOJO_RESULT_OK);

      // Send data to DevTools protocol.
      probe::DirectTCPSocketChunkReceived(*script_state, inspector_id_,
                                          data_buffer);
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
  }
}

void TCPReadableStreamWrapper::CloseStream() {
  // Even if we're in the process of graceful close, readable.cancel() has
  // priority.
  if (GetState() != State::kOpen && GetState() != State::kGracefullyClosing) {
    return;
  }
  SetState(State::kClosed);

  ResetPipe();
  std::move(on_close_).Run(v8::Local<v8::Value>(), net::OK);
  return;
}

void TCPReadableStreamWrapper::ErrorStream(int32_t error_code) {
  if (GetState() != State::kOpen) {
    return;
  }
  graceful_peer_shutdown_ = (error_code == net::OK);

  // Error codes are negative.
  base::UmaHistogramSparse("DirectSockets.TCPReadableStreamError", -error_code);

  auto* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  if (graceful_peer_shutdown_) {
    if (data_pipe_) {
      // This is the case where OnReadError() arrived before pipe break.
      // Set |state| to kGracefullyClosing and handle the rest in
      // OnHandleReset().
      SetState(State::kGracefullyClosing);
    } else {
      // This is the case where OnReadError() arrived after pipe break.
      // Since all data has already been read, we can simply close the
      // controller, set |state| to kClosed and invoke the closing callback.
      SetState(State::kClosed);
      DCHECK(ReadableStream::IsReadable(Readable()));
      NonThrowableExceptionState exception_state;
      Controller()->close(script_state, exception_state);
      std::move(on_close_).Run(v8::Local<v8::Value>(), error_code);
    }
    return;
  }

  SetState(State::kAborted);

  auto exception = V8ThrowDOMException::CreateOrDie(
      script_state->GetIsolate(), DOMExceptionCode::kNetworkError,
      String{"Stream aborted by the remote: " +
             net::ErrorToString(error_code)});

  if (data_pipe_) {
    pending_exception_.Reset(script_state->GetIsolate(), exception);
    pending_net_error_ = error_code;
    return;
  }

  Controller()->error(script_state,
                      ScriptValue(script_state->GetIsolate(), exception));
  std::move(on_close_).Run(exception, error_code);
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

  auto* script_state = GetScriptState();
  // Happens in unit tests if V8TestingScope goes out before OnHandleReset
  // propagates.
  if (!script_state->ContextIsValid()) {
    return;
  }

  ScriptState::Scope scope(script_state);
  if (!pending_exception_.IsEmpty()) {
    auto* isolate = script_state->GetIsolate();
    auto exception = pending_exception_.Get(isolate);
    Controller()->error(script_state,
                        ScriptValue(script_state->GetIsolate(), exception));

    SetState(State::kAborted);
    std::move(on_close_).Run(exception, pending_net_error_);

    pending_exception_.Reset();
  } else if (graceful_peer_shutdown_) {
    DCHECK(ReadableStream::IsReadable(Readable()));
    NonThrowableExceptionState exception_state;
    Controller()->close(script_state, exception_state);

    SetState(State::kClosed);
    std::move(on_close_).Run(/*exception=*/v8::Local<v8::Value>(),
                             /*net_error=*/net::OK);
  }
}

}  // namespace blink
