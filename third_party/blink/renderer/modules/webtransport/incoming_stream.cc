// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/incoming_stream.h"

#include <string.h>

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_error.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/readable_stream_generic_reader.h"
#include "third_party/blink/renderer/core/streams/readable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_error.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

namespace blink {

// An implementation of UnderlyingSourceBase that forwards all operations to the
// IncomingStream object that created it.
class IncomingStream::UnderlyingSource final : public UnderlyingSourceBase {
 public:
  explicit UnderlyingSource(ScriptState* script_state, IncomingStream* stream)
      : UnderlyingSourceBase(script_state), incoming_stream_(stream) {}

  ScriptPromise Start(ScriptState* script_state) override {
    incoming_stream_->controller_ = Controller();
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise pull(ScriptState* script_state) override {
    incoming_stream_->ReadFromPipeAndEnqueue();
    return ScriptPromise::CastUndefined(script_state);
  }

  ScriptPromise Cancel(ScriptState* script_state, ScriptValue reason) override {
    uint8_t code = 0;
    WebTransportError* exception = V8WebTransportError::ToImplWithTypeCheck(
        script_state->GetIsolate(), reason.V8Value());
    if (exception) {
      code = exception->streamErrorCode().value_or(0);
    }
    incoming_stream_->AbortAndReset(code);
    return ScriptPromise::CastUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(incoming_stream_);
    UnderlyingSourceBase::Trace(visitor);
  }

 private:
  const Member<IncomingStream> incoming_stream_;
};

IncomingStream::IncomingStream(
    ScriptState* script_state,
    base::OnceCallback<void(absl::optional<uint8_t>)> on_abort,
    mojo::ScopedDataPipeConsumerHandle handle)
    : script_state_(script_state),
      on_abort_(std::move(on_abort)),
      data_pipe_(std::move(handle)),
      read_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

IncomingStream::~IncomingStream() = default;

void IncomingStream::Init(ExceptionState& exception_state) {
  DVLOG(1) << "IncomingStream::Init() this=" << this;
  auto* stream = MakeGarbageCollected<ReadableStream>();
  InitWithExistingReadableStream(stream, exception_state);
}

void IncomingStream::InitWithExistingReadableStream(
    ReadableStream* stream,
    ExceptionState& exception_state) {
  read_watcher_.Watch(
      data_pipe_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      WTF::BindRepeating(&IncomingStream::OnHandleReady,
                         WrapWeakPersistent(this)));

  stream->InitWithCountQueueingStrategy(
      script_state_,
      MakeGarbageCollected<UnderlyingSource>(script_state_, this), 1,
      AllowPerChunkTransferring(false),
      /*optimizer=*/nullptr, exception_state);
  readable_ = stream;
}

void IncomingStream::OnIncomingStreamClosed(bool fin_received) {
  DVLOG(1) << "IncomingStream::OnIncomingStreamClosed(" << fin_received
           << ") this=" << this;

  DCHECK_NE(state_, State::kClosed);
  state_ = State::kClosed;

  DCHECK(!fin_received_.has_value());

  fin_received_ = fin_received;

  // Wait until HandlePipeClosed() has also been called before processing the
  // close.
  if (is_pipe_closed_) {
    ProcessClose();
  }
}

void IncomingStream::Error(ScriptValue reason) {
  DVLOG(1) << "IncomingStream::Error() this=" << this;

  // We no longer need to call |on_abort_|.
  on_abort_.Reset();

  ErrorStreamAbortAndReset(reason);
}

void IncomingStream::ContextDestroyed() {
  DVLOG(1) << "IncomingStream::ContextDestroyed() this=" << this;

  ResetPipe();
}

void IncomingStream::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(readable_);
  visitor->Trace(controller_);
}

void IncomingStream::OnHandleReady(MojoResult result,
                                   const mojo::HandleSignalsState&) {
  DVLOG(1) << "IncomingStream::OnHandleReady() this=" << this
           << " result=" << result;

  ReadFromPipeAndEnqueue();
}

void IncomingStream::HandlePipeClosed() {
  DVLOG(1) << "IncomingStream::HandlePipeClosed() this=" << this;

  DCHECK(!is_pipe_closed_);

  is_pipe_closed_ = true;

  // Reset the pipe immediately to prevent being called in a loop.
  ResetPipe();

  // Wait until OnIncomingStreamClosed() has also been called before processing
  // the close.
  if (fin_received_.has_value()) {
    // We need a JavaScript scope to be entered in order to resolve the
    // |reading_aborted_| promise.
    ScriptState::Scope scope(script_state_);
    ProcessClose();
  }
}

void IncomingStream::ProcessClose() {
  DVLOG(1) << "IncomingStream::ProcessClose() this=" << this;

  DCHECK(fin_received_.has_value());

  if (fin_received_.value()) {
    CloseAbortAndReset();
  }

  ScriptValue error;
  {
    ScriptState::Scope scope(script_state_);
    DOMExceptionCode code = DOMExceptionCode::kNetworkError;
    String message =
        String::Format("The stream was aborted by the remote server");

    error = ScriptValue(script_state_->GetIsolate(),
                        V8ThrowDOMException::CreateOrEmpty(
                            script_state_->GetIsolate(), code, message));
  }
  ErrorStreamAbortAndReset(error);
}

void IncomingStream::ReadFromPipeAndEnqueue() {
  DVLOG(1) << "IncomingStream::ReadFromPipeAndEnqueue() this=" << this
           << " in_two_phase_read_=" << in_two_phase_read_
           << " read_pending_=" << read_pending_;
  if (is_pipe_closed_) {
    return;
  }

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
      HandlePipeClosed();
      return;

    default:
      NOTREACHED() << "Unexpected result: " << result;
      return;
  }
}

void IncomingStream::EnqueueBytes(const void* source, uint32_t byte_length) {
  DVLOG(1) << "IncomingStream::EnqueueBytes() this=" << this;

  auto* buffer =
      DOMUint8Array::Create(static_cast<const uint8_t*>(source), byte_length);
  controller_->Enqueue(buffer);
}

void IncomingStream::CloseAbortAndReset() {
  DVLOG(1) << "IncomingStream::CloseAbortAndReset() this=" << this;

  if (controller_) {
    controller_->Close();
    controller_ = nullptr;
  }

  AbortAndReset(absl::nullopt);
}

void IncomingStream::ErrorStreamAbortAndReset(ScriptValue exception) {
  DVLOG(1) << "IncomingStream::ErrorStreamAbortAndReset() this=" << this;

  if (controller_) {
    controller_->Error(exception);
    controller_ = nullptr;
  }

  AbortAndReset(absl::nullopt);
}

void IncomingStream::AbortAndReset(absl::optional<uint8_t> code) {
  DVLOG(1) << "IncomingStream::AbortAndReset() this=" << this;

  state_ = State::kAborted;

  if (on_abort_) {
    // Cause WebTransport to drop its reference to us.
    std::move(on_abort_).Run(code);
  }

  ResetPipe();
}

void IncomingStream::ResetPipe() {
  DVLOG(1) << "IncomingStream::ResetPipe() this=" << this;

  read_watcher_.Cancel();
  data_pipe_.reset();
}

void IncomingStream::Dispose() {
  DVLOG(1) << "IncomingStream::Dispose() this=" << this;

  ResetPipe();
}

}  // namespace blink
