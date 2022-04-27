// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port_underlying_sink.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/serial/serial_port.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

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

ScriptPromise SerialPortUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
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

  controller->signal()->AddAlgorithm(
      MakeGarbageCollected<AbortAlgorithm>(this));

  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise SerialPortUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  // There can only be one call to write() in progress at a time.
  DCHECK(!buffer_source_);
  DCHECK_EQ(0u, offset_);
  DCHECK(!pending_operation_);

  if (pending_exception_) {
    exception_state.RethrowV8Exception(
        ToV8Traits<DOMException>::ToV8(script_state, pending_exception_)
            .ToLocalChecked());
    pending_exception_ = nullptr;
    serial_port_->UnderlyingSinkClosed();
    return ScriptPromise();
  }

  buffer_source_ = V8BufferSource::Create(script_state->GetIsolate(),
                                          chunk.V8Value(), exception_state);
  if (exception_state.HadException())
    return ScriptPromise();

  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = pending_operation_->Promise();

  WriteData();
  return promise;
}

ScriptPromise SerialPortUnderlyingSink::close(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  // The specification guarantees that this will only be called after all
  // pending writes have been completed.
  DCHECK(!pending_operation_);

  watcher_.Cancel();
  data_pipe_.reset();

  if (pending_exception_) {
    exception_state.RethrowV8Exception(
        ToV8Traits<DOMException>::ToV8(script_state, pending_exception_)
            .ToLocalChecked());
    pending_exception_ = nullptr;
    serial_port_->UnderlyingSinkClosed();
    return ScriptPromise();
  }

  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  serial_port_->Drain(WTF::Bind(&SerialPortUnderlyingSink::OnFlushOrDrain,
                                WrapPersistent(this)));
  return pending_operation_->Promise();
}

ScriptPromise SerialPortUnderlyingSink::abort(ScriptState* script_state,
                                              ScriptValue reason,
                                              ExceptionState& exception_state) {
  // The specification guarantees that this will only be called after all
  // pending writes have been completed.
  DCHECK(!pending_operation_);

  watcher_.Cancel();
  data_pipe_.reset();

  if (pending_exception_) {
    exception_state.RethrowV8Exception(
        ToV8Traits<DOMException>::ToV8(script_state, pending_exception_)
            .ToLocalChecked());
    pending_exception_ = nullptr;
    serial_port_->UnderlyingSinkClosed();
    return ScriptPromise();
  }

  // If the port is closing the flush will be performed when it closes so we
  // don't need to do it here.
  if (serial_port_->IsClosing()) {
    serial_port_->UnderlyingSinkClosed();
    return ScriptPromise::CastUndefined(script_state);
  }

  pending_operation_ =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  serial_port_->Flush(device::mojom::blink::SerialPortFlushMode::kTransmit,
                      WTF::Bind(&SerialPortUnderlyingSink::OnFlushOrDrain,
                                WrapPersistent(this)));
  return pending_operation_->Promise();
}

void SerialPortUnderlyingSink::SignalErrorOnClose(DOMException* exception) {
  if (data_pipe_ || !pending_operation_) {
    // Pipe is still open or we don't have a write operation that can be failed.
    // Wait for PipeClosed() to be called.
    pending_exception_ = exception;
    return;
  }

  if (pending_operation_) {
    pending_operation_->Reject(exception);
    pending_operation_ = nullptr;
    serial_port_->UnderlyingSinkClosed();
  }
}

void SerialPortUnderlyingSink::Trace(Visitor* visitor) const {
  visitor->Trace(serial_port_);
  visitor->Trace(controller_);
  visitor->Trace(pending_exception_);
  visitor->Trace(buffer_source_);
  visitor->Trace(pending_operation_);
  UnderlyingSinkBase::Trace(visitor);
}

void SerialPortUnderlyingSink::OnAborted() {
  watcher_.Cancel();

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
      NOTREACHED();
  }
}

void SerialPortUnderlyingSink::OnFlushOrDrain() {
  DCHECK(pending_operation_);

  if (pending_exception_) {
    pending_operation_->Reject(pending_exception_);
    pending_exception_ = nullptr;
  } else {
    pending_operation_->Resolve();
  }
  pending_operation_ = nullptr;

  serial_port_->UnderlyingSinkClosed();
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

  const uint8_t* data = array_piece.Bytes();
  const size_t length = array_piece.ByteLength();

  DCHECK_LT(offset_, length);
  data += offset_;
  uint32_t num_bytes = base::saturated_cast<uint32_t>(length - offset_);

  MojoResult result =
      data_pipe_->WriteData(data, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      offset_ += num_bytes;
      if (offset_ == length) {
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
      NOTREACHED();
  }
}

void SerialPortUnderlyingSink::PipeClosed() {
  DCHECK(pending_operation_);

  watcher_.Cancel();
  data_pipe_.reset();

  if (pending_exception_) {
    pending_operation_->Reject(pending_exception_);
    pending_operation_ = nullptr;
    pending_exception_ = nullptr;

    serial_port_->UnderlyingSinkClosed();
  }
}

}  // namespace blink
