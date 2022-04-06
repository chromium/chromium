// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"

#include "base/containers/span.h"
#include "net/base/ip_endpoint.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
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

// An implementation of UnderlyingSourceBase that forwards all operations to the
// TCPReadableStreamWrapper object that created it.
class TCPReadableStreamWrapper::TCPUnderlyingSource final
    : public ReadableStreamWrapper::UnderlyingSource {
 public:
  TCPUnderlyingSource(ScriptState* script_state,
                      TCPReadableStreamWrapper* readable_stream_wrapper)
      : ReadableStreamWrapper::UnderlyingSource(script_state,
                                                readable_stream_wrapper) {}

  ScriptPromise Cancel(ScriptState* script_state, ScriptValue reason) override {
    GetReadableStreamWrapper()->CloseSocket(/*error=*/false);
    return ScriptPromise::CastUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    ReadableStreamWrapper::UnderlyingSource::Trace(visitor);
  }
};

TCPReadableStreamWrapper::TCPReadableStreamWrapper(
    ScriptState* script_state,
    base::OnceCallback<void(bool)> on_close,
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
      WTF::BindRepeating(&TCPReadableStreamWrapper::OnPeerClosed,
                         WrapWeakPersistent(this)));

  // Set queuing strategy of default behavior with a high water mark of 1.
  InitSourceAndReadable(
      /*source=*/MakeGarbageCollected<TCPUnderlyingSource>(script_state, this),
      /*high_water_mark=*/1);
}

void TCPReadableStreamWrapper::CloseSocket(bool error) {
  if (on_close_) {
    std::move(on_close_).Run(error);
  }
  DCHECK_NE(GetState(), State::kOpen);
}

void TCPReadableStreamWrapper::Trace(Visitor* visitor) const {
  ReadableStreamWrapper::Trace(visitor);
}

void TCPReadableStreamWrapper::OnHandleReady(MojoResult result,
                                             const mojo::HandleSignalsState&) {
  DVLOG(1) << "TCPReadableStreamWrapper::OnHandleReady() this=" << this
           << " result=" << result;

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

void TCPReadableStreamWrapper::OnPeerClosed(MojoResult result,
                                            const mojo::HandleSignalsState&) {
  DVLOG(1) << "TCPReadableStreamWrapper::OnPeerClosed() this=" << this
           << " result=" << result;

  DCHECK_EQ(result, MOJO_RESULT_OK);
  DCHECK_EQ(GetState(), State::kOpen);

  CloseSocket(/*error=*/true);
}

void TCPReadableStreamWrapper::Pull() {
  if (!GetScriptState()->ContextIsValid())
    return;

  DVLOG(1) << "TCPReadableStreamWrapper::Pull() this=" << this
           << " in_two_phase_read_=" << in_two_phase_read_
           << " read_pending_=" << read_pending_;

  // Protect against re-entrancy.
  if (in_two_phase_read_) {
    read_pending_ = true;
    return;
  }
  DCHECK(!read_pending_);

  const void* buffer = nullptr;
  uint32_t buffer_num_bytes = 0;
  auto result = data_pipe_->BeginReadData(&buffer, &buffer_num_bytes,
                                          MOJO_BEGIN_READ_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK: {
      in_two_phase_read_ = true;
      // Push() may re-enter this method via TCPUnderlyingSource::pull().
      Push(base::make_span(static_cast<const uint8_t*>(buffer),
                           buffer_num_bytes),
           {});
      data_pipe_->EndReadData(buffer_num_bytes);
      in_two_phase_read_ = false;
      if (read_pending_) {
        read_pending_ = false;
        // pull() will not be called when another pull() is in progress, so the
        // maximum recursion depth is 1.
        Pull();
      }
      break;
    }

    case MOJO_RESULT_SHOULD_WAIT:
      read_watcher_.ArmOrNotify();
      return;

    case MOJO_RESULT_FAILED_PRECONDITION:
      // This will be handled by close_watcher_.
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

void TCPReadableStreamWrapper::CloseStream(bool error) {
  if (GetState() != State::kOpen) {
    return;
  }
  SetState(error ? State::kAborted : State::kClosed);

  if (error) {
    ScriptState::Scope scope(GetScriptState());
    Controller()->Error(CreateException(
        GetScriptState(), DOMExceptionCode::kNetworkError, "Error."));
  } else {
    Controller()->Close();
  }

  on_close_.Reset();

  ResetPipe();
}

void TCPReadableStreamWrapper::ResetPipe() {
  DVLOG(1) << "TCPReadableStreamWrapper::ResetPipe() this=" << this;

  read_watcher_.Cancel();
  close_watcher_.Cancel();
  data_pipe_.reset();
}

void TCPReadableStreamWrapper::Dispose() {
  DVLOG(1) << "TCPReadableStreamWrapper::Dispose() this=" << this;

  ResetPipe();
}

}  // namespace blink
