// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/serial/serial_port_underlying_source.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/serial/serial_port.h"

namespace blink {

SerialPortUnderlyingSource::SerialPortUnderlyingSource(
    ScriptState* script_state,
    SerialPort* serial_port,
    mojo::ScopedDataPipeConsumerHandle handle)
    : UnderlyingSourceBase(script_state),
      data_pipe_(std::move(handle)),
      watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      serial_port_(serial_port) {
  watcher_.Watch(data_pipe_.get(),
                 MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
                 MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                 WTF::BindRepeating(&SerialPortUnderlyingSource::OnHandleReady,
                                    WrapWeakPersistent(this)));
}

ScriptPromise SerialPortUnderlyingSource::pull(ScriptState* script_state) {
  // pull() signals that the stream wants more data. By resolving immediately
  // we allow the stream to be canceled before that data is received. pull()
  // will not be called again until a chunk is enqueued or if an error has been
  // signaled to the controller.
  DCHECK(data_pipe_);

  if (!ReadData())
    ArmWatcher();

  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise SerialPortUnderlyingSource::Cancel(ScriptState* script_state,
                                                 ScriptValue reason) {
  // TODO(crbug.com/989653): Rather than calling Close(), cancel() should
  // trigger a purge of the serial read buffer and wait for the pipe to close to
  // indicate the purge has been completed.
  Close();
  ExpectPipeClose();
  return ScriptPromise::CastUndefined(script_state);
}

void SerialPortUnderlyingSource::ContextDestroyed(
    ExecutionContext* execution_context) {
  Close();
  UnderlyingSourceBase::ContextDestroyed(execution_context);
}

void SerialPortUnderlyingSource::SignalErrorImmediately(
    DOMException* exception) {
  SignalErrorOnClose(exception);
  PipeClosed();
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

void SerialPortUnderlyingSource::Trace(Visitor* visitor) {
  visitor->Trace(pending_exception_);
  visitor->Trace(serial_port_);
  UnderlyingSourceBase::Trace(visitor);
}

bool SerialPortUnderlyingSource::ReadData() {
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
      return true;
    }
    case MOJO_RESULT_FAILED_PRECONDITION:
      PipeClosed();
      return true;
    case MOJO_RESULT_SHOULD_WAIT:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

void SerialPortUnderlyingSource::ArmWatcher() {
  MojoResult ready_result;
  mojo::HandleSignalsState ready_state;
  MojoResult result = watcher_.Arm(&ready_result, &ready_state);
  if (result == MOJO_RESULT_OK)
    return;

  DCHECK_EQ(ready_result, MOJO_RESULT_OK);
  if (ready_state.readable()) {
    bool read_result = ReadData();
    DCHECK(read_result);
  } else if (ready_state.peer_closed()) {
    PipeClosed();
  }
}

void SerialPortUnderlyingSource::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  switch (result) {
    case MOJO_RESULT_OK: {
      bool read_result = ReadData();
      DCHECK(read_result);
      break;
    }
    case MOJO_RESULT_SHOULD_WAIT:
      watcher_.ArmOrNotify();
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      PipeClosed();
      break;
  }
}

void SerialPortUnderlyingSource::ExpectPipeClose() {
  if (data_pipe_) {
    // The pipe is still open. Wait for PipeClosed() to be called.
    expect_close_ = true;
    return;
  }

  Controller()->Close();
  serial_port_->UnderlyingSourceClosed();
}

void SerialPortUnderlyingSource::PipeClosed() {
  if (pending_exception_) {
    Controller()->Error(pending_exception_);
    serial_port_->UnderlyingSourceClosed();
  }
  if (expect_close_) {
    Controller()->Close();
    serial_port_->UnderlyingSourceClosed();
  }
  Close();
}

void SerialPortUnderlyingSource::Close() {
  watcher_.Cancel();
  data_pipe_.reset();
}

}  // namespace blink
