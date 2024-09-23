// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port_underlying_sink.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/serial/serial_port.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {
using ::device::mojom::blink::SerialSendError;
}

SerialPortUnderlyingSink::SerialPortUnderlyingSink(
    SerialPort* serial_port,
    mojo::ScopedDataPipeProducerHandle handle)
    : data_pipe_(std::move(handle)),
      watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      serial_port_(serial_port) {
  watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                 MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                 WTF::BindRepeating(&SerialPortUnderlyingSink::OnHandleReady,
                                    WrapWeakPersistent(this)));
}

ScriptPromise<IDLUndefined> SerialPortUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  script_state_ = script_state;
  controller_ = controller;

  class AbortAlgorithm final : public AbortSignal::Algorithm {
   public:
    explicit AbortAlgorithm(SerialPortUnderlyingSink* sink) : sink_(sink) {}

    void Run() override { sink_->OnAborted(); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(sink_);
      Algorithm::Trace(visitor);
    }

   private:
    Member<SerialPortUnderlyingSink> sink_;
  };

  DCHECK(!abort_handle_);
  abort_handle_ = controller->signal()->AddAlgorithm(
      MakeGarbageCollected<AbortAlgorithm>(this));

  return ToResolvedUndefinedPromise(script_state);
}

ScriptPromise<IDLUndefined> SerialPortUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  // There can only be one call to write() in progress at a time.
  DCHECK(!buffer_source_);
  DCHECK_EQ(0u, offset_);
  DCHECK(!pending_operation_);

  buffer_source_ = V8BufferSource::Create(script_state->GetIsolate(),
                                          chunk.V8Value(), exception_state);
  if (exception_state.HadException())
    return EmptyPromise();

  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state, exception_state.GetContext());
  auto promise = pending_operation_->Promise();

  WriteData();
  return promise;
}

ScriptPromise<IDLUndefined> SerialPortUnderlyingSink::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // The specification guarantees that this will only be called after all
  // pending writes have been completed.
  DCHECK(!pending_operation_);

  watcher_.Cancel();
  data_pipe_.reset();
  abort_handle_.Clear();

  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state, exception_state.GetContext());
  serial_port_->Drain(WTF::BindOnce(&SerialPortUnderlyingSink::OnFlushOrDrain,
                                    WrapPersistent(this)));
  return pending_operation_->Promise();
}

ScriptPromise<IDLUndefined> SerialPortUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  // The specification guarantees that this will only be called after all
  // pending writes have been completed.
  DCHECK(!pending_operation_);

  watcher_.Cancel();
  data_pipe_.reset();
  abort_handle_.Clear();

  // If the port is closing the flush will be performed when it closes so we
  // don't need to do it here.
  if (serial_port_->IsClosing()) {
    serial_port_->UnderlyingSinkClosed();
    return ToResolvedUndefinedPromise(script_state);
  }

  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state, exception_state.GetContext());
  serial_port_->Flush(device::mojom::blink::SerialPortFlushMode::kTransmit,
                      WTF::BindOnce(&SerialPortUnderlyingSink::OnFlushOrDrain,
                                    WrapPersistent(this)));
  return pending_operation_->Promise();
}

void SerialPortUnderlyingSink::SignalError(SerialSendError error) {
  watcher_.Cancel();
  data_pipe_.reset();
  abort_handle_.Clear();

  ScriptState* script_state = pending_operation_
                                  ? pending_operation_->GetScriptState()
                                  : script_state_.Get();
  ScriptState::Scope script_state_scope(script_state_);

  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> exception;
  switch (error) {
    case SerialSendError::NONE:
      NOTREACHED_IN_MIGRATION();
      break;
    case SerialSendError::DISCONNECTED:
      exception = V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kNetworkError,
          "The device has been lost.");
      break;
    case SerialSendError::SYSTEM_ERROR:
      exception = V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kUnknownError,
          "An unknown system error has occurred.");
      break;
  }

  if (pending_operation_) {
    pending_operation_->Reject(exception);
    pending_operation_ = nullptr;
  } else {
    controller_->error(script_state_, ScriptValue(isolate, exception));
  }

  serial_port_->UnderlyingSinkClosed();
}

void SerialPortUnderlyingSink::Trace(Visitor* visitor) const {
  visitor->Trace(serial_port_);
  visitor->Trace(script_state_);
  visitor->Trace(controller_);
  visitor->Trace(abort_handle_);
  visitor->Trace(buffer_source_);
  visitor->Trace(pending_operation_);
  UnderlyingSinkBase::Trace(visitor);
}

void SerialPortUnderlyingSink::OnAborted() {
  watcher_.Cancel();
  abort_handle_.Clear();

  // Rejecting |pending_operation_| allows the rest of the process of aborting
  // the stream to be handled by abort().
  if (pending_operation_) {
    ScriptState* script_state = pending_operation_->GetScriptState();
    pending_operation_->Reject(controller_->signal()->reason(script_state));
    pending_operation_ = nullptr;
  }
}

void SerialPortUnderlyingSink::OnHandleReady(MojoResult result,
                                             const mojo::HandleSignalsState&) {
  switch (result) {
    case MOJO_RESULT_OK:
      WriteData();
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      PipeClosed();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void SerialPortUnderlyingSink::OnFlushOrDrain() {
  // If pending_operation_ is nullptr, that means SignalError happened before
  // flush finished and SerialPort::UnderlyingSinkClosed has been called.
  if (pending_operation_) {
    pending_operation_->Resolve();
    pending_operation_ = nullptr;
    serial_port_->UnderlyingSinkClosed();
  }
}

void SerialPortUnderlyingSink::WriteData() {
  DCHECK(data_pipe_);
  DCHECK(pending_operation_);
  DCHECK(buffer_source_);

  DOMArrayPiece array_piece(buffer_source_);
  // From https://webidl.spec.whatwg.org/#dfn-get-buffer-source-copy, if the
  // buffer source is detached then an empty byte sequence is returned, which
  // means the write is complete.
  if (array_piece.IsDetached()) {
    buffer_source_ = nullptr;
    offset_ = 0;
    pending_operation_->Resolve();
    pending_operation_ = nullptr;
    return;
  }

  size_t actually_written_bytes = 0;
  MojoResult result =
      data_pipe_->WriteData(array_piece.ByteSpan().subspan(offset_),
                            MOJO_WRITE_DATA_FLAG_NONE, actually_written_bytes);
  switch (result) {
    case MOJO_RESULT_OK:
      offset_ += actually_written_bytes;
      if (offset_ == array_piece.ByteLength()) {
        buffer_source_ = nullptr;
        offset_ = 0;
        pending_operation_->Resolve();
        pending_operation_ = nullptr;
        break;
      }
      [[fallthrough]];
    case MOJO_RESULT_SHOULD_WAIT:
      watcher_.ArmOrNotify();
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      PipeClosed();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void SerialPortUnderlyingSink::PipeClosed() {
  watcher_.Cancel();
  data_pipe_.reset();
  abort_handle_.Clear();
}

}  // namespace blink
