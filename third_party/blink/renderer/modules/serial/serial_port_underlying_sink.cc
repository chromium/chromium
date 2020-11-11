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
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise SerialPortUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  // There can only be one call to write() in progress at a time.
  DCHECK(buffer_source_.IsNull());
  DCHECK_EQ(0u, offset_);
  DCHECK(!pending_operation_);

  if (pending_exception_) {
    DOMException* exception = pending_exception_;
    pending_exception_ = nullptr;
    serial_port_->UnderlyingSinkClosed();
    exception_state.RethrowV8Exception(ToV8(exception, script_state));
    return ScriptPromise();
  }

  V8ArrayBufferOrArrayBufferView::ToImpl(
      script_state->GetIsolate(), chunk.V8Value(), buffer_source_,
      UnionTypeConversionMode::kNotNullable, exception_state);
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
    DOMException* exception = pending_exception_;
    pending_exception_ = nullptr;
    exception_state.RethrowV8Exception(ToV8(exception, script_state));
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
    DOMException* exception = pending_exception_;
    pending_exception_ = nullptr;
    exception_state.RethrowV8Exception(ToV8(exception, script_state));
    serial_port_->UnderlyingSinkClosed();
    return ScriptPromise();
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
  visitor->Trace(pending_exception_);
  visitor->Trace(buffer_source_);
  visitor->Trace(pending_operation_);
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

void SerialPortUnderlyingSink::OnFlushOrDrain() {
  ScriptPromiseResolver* resolver = pending_operation_;
  pending_operation_ = nullptr;

  DOMException* exception = pending_exception_;
  pending_exception_ = nullptr;

  serial_port_->UnderlyingSinkClosed();

  if (exception) {
    resolver->Reject(exception);
  } else {
    resolver->Resolve();
  }
}

void SerialPortUnderlyingSink::WriteData() {
  DCHECK(data_pipe_);
  DCHECK(pending_operation_);
  DCHECK(!buffer_source_.IsNull());

  const uint8_t* data = nullptr;
  uint32_t length = 0;
  size_t byte_size = 0;
  if (buffer_source_.IsArrayBuffer()) {
    DOMArrayBuffer* array = buffer_source_.GetAsArrayBuffer();
    byte_size = array->ByteLength();
    data = static_cast<const uint8_t*>(array->Data());
  } else {
    DOMArrayBufferView* view = buffer_source_.GetAsArrayBufferView().View();
    byte_size = view->byteLength();
    data = static_cast<const uint8_t*>(view->BaseAddress());
  }
  if (byte_size > std::numeric_limits<uint32_t>::max()) {
    pending_exception_ = DOMException::Create(
        "Buffer size exceeds maximum heap object size.", "DataError");
    PipeClosed();
    return;
  }
  length = static_cast<uint32_t>(byte_size);

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
        pending_operation_->Resolve();
        pending_operation_ = nullptr;
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
  DCHECK(pending_operation_);

  watcher_.Cancel();
  data_pipe_.reset();

  if (pending_exception_) {
    DOMException* exception = pending_exception_;
    pending_exception_ = nullptr;
    serial_port_->UnderlyingSinkClosed();
    pending_operation_->Reject(exception);
    pending_operation_ = nullptr;
  }
}

}  // namespace blink
