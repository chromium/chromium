// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port_underlying_source.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_byte_stream_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_request.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/serial/serial_port.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {
using ::device::mojom::blink::SerialReceiveError;
}

SerialPortUnderlyingSource::SerialPortUnderlyingSource(
    ScriptState* script_state,
    SerialPort* serial_port,
    mojo::ScopedDataPipeConsumerHandle handle)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      data_pipe_(std::move(handle)),
      watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      script_state_(script_state),
      serial_port_(serial_port) {
  watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                 MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                 WTF::BindRepeating(&SerialPortUnderlyingSource::OnHandleReady,
                                    WrapWeakPersistent(this)));
}

ScriptPromise SerialPortUnderlyingSource::Pull(
    ReadableByteStreamController* controller,
    ExceptionState&) {
  DCHECK(controller_ == nullptr || controller_ == controller);
  controller_ = controller;

  DCHECK(data_pipe_);
  ReadDataOrArmWatcher();

  // pull() signals that the stream wants more data. By resolving immediately
  // we allow the stream to be canceled before that data is received. pull()
  // will not be called again until a chunk is enqueued or if an error has been
  // signaled to the controller.
  return ScriptPromise::CastUndefined(script_state_);
}

ScriptPromise SerialPortUnderlyingSource::Cancel(
    ExceptionState& exception_state) {
  DCHECK(data_pipe_);

  Close();

  // If the port is closing the flush will be performed when it closes so we
  // don't need to do it here.
  if (serial_port_->IsClosing()) {
    serial_port_->UnderlyingSourceClosed();
    return ScriptPromise::CastUndefined(script_state_);
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  serial_port_->Flush(
      device::mojom::blink::SerialPortFlushMode::kReceive,
      WTF::BindOnce(&SerialPortUnderlyingSource::OnFlush, WrapPersistent(this),
                    WrapPersistent(resolver)));
  return resolver->Promise();
}

ScriptPromise SerialPortUnderlyingSource::Cancel(
    v8::Local<v8::Value> reason,
    ExceptionState& exception_state) {
  return Cancel(exception_state);
}

ScriptState* SerialPortUnderlyingSource::GetScriptState() {
  return script_state_;
}

void SerialPortUnderlyingSource::ContextDestroyed() {
  Close();
}

void SerialPortUnderlyingSource::SignalErrorOnClose(SerialReceiveError error) {
  ScriptState::Scope script_state_scope(script_state_);

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Value> exception;
  switch (error) {
    case SerialReceiveError::NONE:
      NOTREACHED();
      break;
    case SerialReceiveError::DISCONNECTED:
      [[fallthrough]];
    case SerialReceiveError::DEVICE_LOST:
      exception = V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kNetworkError,
          "The device has been lost.");
      break;
    case SerialReceiveError::BREAK:
      exception = V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kBreakError, "Break received");
      break;
    case SerialReceiveError::FRAME_ERROR:
      exception = V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kFramingError, "Framing error");
      break;
    case SerialReceiveError::OVERRUN:
      [[fallthrough]];
    case SerialReceiveError::BUFFER_OVERFLOW:
      exception = V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kBufferOverrunError, "Buffer overrun");
      break;
    case SerialReceiveError::PARITY_ERROR:
      exception = V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kParityError, "Parity error");
      break;
    case SerialReceiveError::SYSTEM_ERROR:
      exception = V8ThrowDOMException::CreateOrDie(
          isolate, DOMExceptionCode::kUnknownError,
          "An unknown system error has occurred.");
      break;
  }

  if (data_pipe_) {
    // Pipe is still open. Wait for PipeClosed() to be called.
    pending_exception_ = ScriptValue(isolate, exception);
    return;
  }

  controller_->error(script_state_, ScriptValue(isolate, exception));
  serial_port_->UnderlyingSourceClosed();
}

void SerialPortUnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(pending_exception_);
  visitor->Trace(script_state_);
  visitor->Trace(serial_port_);
  visitor->Trace(controller_);
  UnderlyingByteSourceBase::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

void SerialPortUnderlyingSource::ReadDataOrArmWatcher() {
  const void* buffer = nullptr;
  uint32_t length = 0;
  MojoResult result =
      data_pipe_->BeginReadData(&buffer, &length, MOJO_READ_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK: {
      // respond() or enqueue() will only throw if their arguments are invalid
      // or the stream is errored. The code below guarantees that the length is
      // in range and the chunk is a valid view. If the stream becomes errored
      // then this method cannot be called because the watcher is disarmed.
      NonThrowableExceptionState exception_state;

      if (ReadableStreamBYOBRequest* request = controller_->byobRequest()) {
        DOMArrayPiece view(request->view().Get());
        length =
            std::min(base::saturated_cast<uint32_t>(view.ByteLength()), length);
        memcpy(view.Data(), buffer, length);
        request->respond(script_state_, length, exception_state);
      } else {
        auto chunk = NotShared(DOMUint8Array::Create(
            static_cast<const unsigned char*>(buffer), length));
        controller_->enqueue(script_state_, chunk, exception_state);
      }
      result = data_pipe_->EndReadData(length);
      DCHECK_EQ(result, MOJO_RESULT_OK);
      break;
    }
    case MOJO_RESULT_FAILED_PRECONDITION:
      PipeClosed();
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      watcher_.ArmOrNotify();
      break;
    default:
      invalid_data_pipe_read_result_ = result;
      NOTREACHED() << "Invalid data pipe read result: " << result;
      break;
  }
}

void SerialPortUnderlyingSource::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  ScriptState::Scope script_state_scope(script_state_);

  switch (result) {
    case MOJO_RESULT_OK:
      ReadDataOrArmWatcher();
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      watcher_.ArmOrNotify();
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      PipeClosed();
      break;
  }
}

void SerialPortUnderlyingSource::OnFlush(ScriptPromiseResolver* resolver) {
  serial_port_->UnderlyingSourceClosed();
  resolver->Resolve();
}

void SerialPortUnderlyingSource::PipeClosed() {
  if (!pending_exception_.IsEmpty()) {
    controller_->error(script_state_, pending_exception_);
    pending_exception_.Clear();
    serial_port_->UnderlyingSourceClosed();
  }
  Close();
}

void SerialPortUnderlyingSource::Close() {
  watcher_.Cancel();
  data_pipe_.reset();
}

}  // namespace blink
