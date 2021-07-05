// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/tcp_readable_stream_wrapper.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// An implementation of UnderlyingSourceBase that forwards all operations to the
// TCPReadableStreamWrapper object that created it.
class TCPReadableStreamWrapper::UnderlyingSource final
    : public UnderlyingSourceBase {
 public:
  UnderlyingSource(ScriptState* script_state, TCPReadableStreamWrapper* stream)
      : UnderlyingSourceBase(script_state),
        tcp_readable_stream_wrapper_(stream) {}

  ScriptPromise Start(ScriptState* script_state) override {
    DVLOG(1) << "TCPReadableStreamWrapper::UnderlyingSource::start() "
                "tcp_readable_stream_wrapper_="
             << tcp_readable_stream_wrapper_;

    tcp_readable_stream_wrapper_->controller_ = Controller();
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise pull(ScriptState* script_state) override {
    DVLOG(1) << "TCPReadableStreamWrapper::UnderlyingSource::pull() "
                "tcp_readable_stream_wrapper_="
             << tcp_readable_stream_wrapper_;

    tcp_readable_stream_wrapper_->ReadFromPipeAndEnqueue();
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise Cancel(ScriptState* script_state, ScriptValue reason) override {
    DVLOG(1) << "TCPReadableStreamWrapper::UnderlyingSource::Cancel() "
                "tcp_readable_stream_wrapper_="
             << tcp_readable_stream_wrapper_;

    tcp_readable_stream_wrapper_->AbortAndReset();
    return ScriptPromise::CastUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(tcp_readable_stream_wrapper_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  const Member<TCPReadableStreamWrapper> tcp_readable_stream_wrapper_;
};

TCPReadableStreamWrapper::TCPReadableStreamWrapper(
    ScriptState* script_state,
    base::OnceClosure on_abort,
    mojo::ScopedDataPipeConsumerHandle handle)
    : script_state_(script_state),
      on_abort_(std::move(on_abort)),
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

  ScriptState::Scope scope(script_state_);
  // Set queuing strategy of default behavior with a high water mark of 1.
  readable_ = ReadableStream::CreateWithCountQueueingStrategy(
      script_state_,
      MakeGarbageCollected<UnderlyingSource>(script_state_, this), 1);
}

TCPReadableStreamWrapper::~TCPReadableStreamWrapper() = default;

void TCPReadableStreamWrapper::Reset() {
  DVLOG(1) << "TCPReadableStreamWrapper::Reset() this=" << this;
  // We no longer need to call |on_abort_|.
  on_abort_.Reset();

  ErrorStreamAbortAndReset();
}

void TCPReadableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(readable_);
  visitor->Trace(controller_);
}

void TCPReadableStreamWrapper::OnHandleReady(MojoResult result,
                                             const mojo::HandleSignalsState&) {
  DVLOG(1) << "TCPReadableStreamWrapper::OnHandleReady() this=" << this
           << " result=" << result;

  switch (result) {
    case MOJO_RESULT_OK:
      ReadFromPipeAndEnqueue();
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

  DCHECK_EQ(state_, State::kOpen);
  state_ = State::kClosed;

  ErrorStreamAbortAndReset();
}

void TCPReadableStreamWrapper::ReadFromPipeAndEnqueue() {
  if (!script_state_->ContextIsValid())
    return;

  DVLOG(1) << "TCPReadableStreamWrapper::ReadFromPipeAndEnqueue() this=" << this
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
      // EnqueueBytes() may re-enter this method via pull().
      EnqueueBytes(buffer, buffer_num_bytes);
      data_pipe_->EndReadData(buffer_num_bytes);
      in_two_phase_read_ = false;
      if (read_pending_) {
        read_pending_ = false;
        // pull() will not be called when another pull() is in progress, so the
        // maximum recursion depth is 1.
        ReadFromPipeAndEnqueue();
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

void TCPReadableStreamWrapper::EnqueueBytes(const void* source,
                                            uint32_t byte_length) {
  DVLOG(1) << "TCPReadableStreamWrapper::EnqueueBytes() this=" << this;

  auto* buffer =
      DOMUint8Array::Create(static_cast<const uint8_t*>(source), byte_length);
  controller_->Enqueue(buffer);
}

ScriptValue TCPReadableStreamWrapper::CreateAbortException() {
  DVLOG(1) << "TCPReadableStreamWrapper::CreateAbortException() this=" << this;

  DOMExceptionCode code = DOMExceptionCode::kNetworkError;
  String message = "The stream was aborted by the remote";

  return ScriptValue(script_state_->GetIsolate(),
                     V8ThrowDOMException::CreateOrEmpty(
                         script_state_->GetIsolate(), code, message));
}

void TCPReadableStreamWrapper::ErrorStreamAbortAndReset() {
  DVLOG(1) << "TCPReadableStreamWrapper::ErrorStreamAbortAndReset() this="
           << this;

  if (script_state_->ContextIsValid()) {
    ScriptState::Scope scope(script_state_);
    if (controller_) {
      controller_->Error(CreateAbortException());
    }
  }

  controller_ = nullptr;
  AbortAndReset();
}

void TCPReadableStreamWrapper::AbortAndReset() {
  DVLOG(1) << "TCPReadableStreamWrapper::AbortAndReset() this=" << this;
  state_ = State::kAborted;

  if (on_abort_) {
    std::move(on_abort_).Run();
  }

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
