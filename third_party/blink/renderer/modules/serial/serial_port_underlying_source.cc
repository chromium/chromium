// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port_underlying_source.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/serial/serial_port.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

SerialPortUnderlyingSource::SerialPortUnderlyingSource(
    ScriptState* script_state,
    SerialPort* serial_port,
    mojo::ScopedDataPipeConsumerHandle handle)
    : UnderlyingSourceBase(script_state),
      data_pipe_(std::move(handle)),
      watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      serial_port_(serial_port) {
  watcher_.Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                 MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                 WTF::BindRepeating(&SerialPortUnderlyingSource::OnHandleReady,
                                    WrapWeakPersistent(this)));
}

ScriptPromise SerialPortUnderlyingSource::pull(ScriptState* script_state) {
  DCHECK(data_pipe_);
  ReadDataOrArmWatcher();

  // pull() signals that the stream wants more data. By resolving immediately
  // we allow the stream to be canceled before that data is received. pull()
  // will not be called again until a chunk is enqueued or if an error has been
  // signaled to the controller.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise SerialPortUnderlyingSource::Cancel(ScriptState* script_state,
                                                 ScriptValue reason) {
  DCHECK(data_pipe_);

  Close();

  // If the port is closing the flush will be performed when it closes so we
  // don't need to do it here.
  if (serial_port_->IsClosing()) {
    serial_port_->UnderlyingSourceClosed();
    return ScriptPromise::CastUndefined(script_state);
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  serial_port_->Flush(
      device::mojom::blink::SerialPortFlushMode::kReceive,
      WTF::Bind(&SerialPortUnderlyingSource::OnFlush, WrapPersistent(this),
                WrapPersistent(resolver)));
  return resolver->Promise();
}

void SerialPortUnderlyingSource::ContextDestroyed() {
  Close();
  UnderlyingSourceBase::ContextDestroyed();
}

void SerialPortUnderlyingSource::SignalErrorOnClose(DOMException* exception) {
  if (data_pipe_) {
    // Pipe is still open. Wait for PipeClosed() to be called.
    pending_exception_ = exception;
    return;
  }

  Controller()->Error(exception);
  serial_port_->UnderlyingSourceClosed();
}

void SerialPortUnderlyingSource::Trace(Visitor* visitor) const {
  visitor->Trace(pending_exception_);
  visitor->Trace(serial_port_);
  UnderlyingSourceBase::Trace(visitor);
}

void SerialPortUnderlyingSource::ReadDataOrArmWatcher() {
  const void* buffer = nullptr;
  uint32_t available = 0;
  MojoResult result =
      data_pipe_->BeginReadData(&buffer, &available, MOJO_READ_DATA_FLAG_NONE);
  switch (result) {
    case MOJO_RESULT_OK: {
      auto* array = DOMUint8Array::Create(
          static_cast<const unsigned char*>(buffer), available);
      result = data_pipe_->EndReadData(available);
      DCHECK_EQ(result, MOJO_RESULT_OK);
      Controller()->Enqueue(array);
      break;
    }
    case MOJO_RESULT_FAILED_PRECONDITION:
      PipeClosed();
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      watcher_.ArmOrNotify();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void SerialPortUnderlyingSource::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
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
  if (pending_exception_) {
    Controller()->Error(pending_exception_);
    serial_port_->UnderlyingSourceClosed();
  }
  Close();
}

void SerialPortUnderlyingSource::Close() {
  watcher_.Cancel();
  data_pipe_.reset();
}

}  // namespace blink
