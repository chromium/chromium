// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port_underlying_sink.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
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
    WritableStreamDefaultControllerInterface* controller) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise SerialPortUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultControllerInterface* controller) {
  // There can only be one call to write() in progress at a time.
  DCHECK(buffer_source_.IsNull());
  DCHECK_EQ(0u, offset_);
  DCHECK(!pending_write_);

  if (pending_exception_) {
    DOMException* exception = pending_exception_;
    pending_exception_ = nullptr;
    serial_port_->UnderlyingSinkClosed();
    return ScriptPromise::RejectWithDOMException(script_state, exception);
  }

  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "SerialPortUnderlyingSink", "write");

  V8ArrayBufferOrArrayBufferView::ToImpl(
      script_state->GetIsolate(), chunk.V8Value(), buffer_source_,
      UnionTypeConversionMode::kNotNullable, exception_state);
  if (exception_state.HadException())
    return ScriptPromise::Reject(script_state, exception_state);

  pending_write_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = pending_write_->Promise();

  WriteData();
  return promise;
}

ScriptPromise SerialPortUnderlyingSink::close(ScriptState* script_state) {
  // The specification guarantees that this will only be called after all
  // pending writes have been completed.
  DCHECK(!pending_write_);

  watcher_.Cancel();
  data_pipe_.reset();
  serial_port_->UnderlyingSinkClosed();

  if (pending_exception_) {
    DOMException* exception = pending_exception_;
    pending_exception_ = nullptr;
    return ScriptPromise::RejectWithDOMException(script_state, exception);
  }

  // TODO(crbug.com/989656): close() should wait for data to be flushed before
  // resolving. This will require waiting for |data_pipe_| to close.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise SerialPortUnderlyingSink::abort(ScriptState* script_state,
                                              ScriptValue reason) {
  // The specification guarantees that this will only be called after all
  // pending writes have been completed.
  // TODO(crbug.com/969653): abort() should trigger a purge of the serial write
  // buffers.
  return close(script_state);
}

void SerialPortUnderlyingSink::SignalErrorOnClose(DOMException* exception) {
  if (data_pipe_ || !pending_write_) {
    // Pipe is still open or we don't have a write operation that can be failed.
    // Wait for PipeClosed() to be called.
    pending_exception_ = exception;
    return;
  }

  if (pending_write_) {
    pending_write_->Reject(exception);
    pending_write_ = nullptr;
    serial_port_->UnderlyingSinkClosed();
  }
}

void SerialPortUnderlyingSink::Trace(Visitor* visitor) {
  visitor->Trace(serial_port_);
  visitor->Trace(pending_exception_);
  visitor->Trace(buffer_source_);
  visitor->Trace(pending_write_);
  UnderlyingSinkBase::Trace(visitor);
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

void SerialPortUnderlyingSink::WriteData() {
  DCHECK(data_pipe_);
  DCHECK(pending_write_);
  DCHECK(!buffer_source_.IsNull());

  const uint8_t* data = nullptr;
  uint32_t length = 0;
  if (buffer_source_.IsArrayBuffer()) {
    DOMArrayBuffer* array = buffer_source_.GetAsArrayBuffer();
    data = static_cast<const uint8_t*>(array->Data());
    length = array->DeprecatedByteLengthAsUnsigned();
  } else {
    DOMArrayBufferView* view = buffer_source_.GetAsArrayBufferView().View();
    data = static_cast<const uint8_t*>(view->BaseAddress());
    length = view->byteLength();
  }

  DCHECK_LT(offset_, length);
  data += offset_;
  uint32_t num_bytes = length - offset_;

  MojoResult result =
      data_pipe_->WriteData(data, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK:
      offset_ += num_bytes;
      if (offset_ == length) {
        buffer_source_ = ArrayBufferOrArrayBufferView();
        offset_ = 0;
        pending_write_->Resolve();
        pending_write_ = nullptr;
        break;
      }
      FALLTHROUGH;
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
  DCHECK(pending_write_);

  watcher_.Cancel();
  data_pipe_.reset();

  if (pending_exception_) {
    DOMException* exception = pending_exception_;
    pending_exception_ = nullptr;
    serial_port_->UnderlyingSinkClosed();
    pending_write_->Reject(exception);
    pending_write_ = nullptr;
  }
}

}  // namespace blink
