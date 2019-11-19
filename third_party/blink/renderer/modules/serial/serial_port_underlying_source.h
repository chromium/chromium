// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_UNDERLYING_SOURCE_H_

#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"

namespace blink {

class DOMException;
class SerialPort;

class SerialPortUnderlyingSource : public UnderlyingSourceBase {
 public:
  SerialPortUnderlyingSource(ScriptState*,
                             SerialPort*,
                             mojo::ScopedDataPipeConsumerHandle);

  // UnderlyingSourceBase
  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*, ScriptValue reason) override;
  void ContextDestroyed(ExecutionContext*) override;

  void SignalErrorImmediately(DOMException*);
  void SignalErrorOnClose(DOMException*);

  void Trace(Visitor*) override;

 private:
  // Reads data from |data_pipe_|. Returns true if data was enqueued to
  // |Controller()| or the pipe was closed, and false otherwise.
  bool ReadData();

  void ArmWatcher();
  void OnHandleReady(MojoResult, const mojo::HandleSignalsState&);
  void ExpectPipeClose();
  void PipeClosed();
  void Close();

  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  mojo::SimpleWatcher watcher_;
  Member<SerialPort> serial_port_;
  Member<DOMException> pending_exception_;
  bool expect_close_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_UNDERLYING_SOURCE_H_
