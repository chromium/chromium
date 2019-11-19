// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_UNDERLYING_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_UNDERLYING_SINK_H_

#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"

namespace blink {

class ScriptPromiseResolver;
class SerialPort;

class SerialPortUnderlyingSink final : public UnderlyingSinkBase {
 public:
  SerialPortUnderlyingSink(SerialPort*, mojo::ScopedDataPipeProducerHandle);

  // UnderlyingSinkBase
  ScriptPromise start(ScriptState*,
                      WritableStreamDefaultControllerInterface*) override;
  ScriptPromise write(ScriptState*,
                      ScriptValue chunk,
                      WritableStreamDefaultControllerInterface*) override;
  ScriptPromise close(ScriptState*) override;
  ScriptPromise abort(ScriptState*, ScriptValue reason) override;

  // After |data_pipe_| has closed calls to write() will return a Promise
  // rejected with this DOMException.
  void SignalErrorOnClose(DOMException*);

  void Trace(Visitor*) override;

 private:
  void OnHandleReady(MojoResult, const mojo::HandleSignalsState&);
  void WriteData();
  void PipeClosed();

  mojo::ScopedDataPipeProducerHandle data_pipe_;
  mojo::SimpleWatcher watcher_;
  Member<SerialPort> serial_port_;
  Member<DOMException> pending_exception_;

  ArrayBufferOrArrayBufferView buffer_source_;
  uint32_t offset_ = 0;
  Member<ScriptPromiseResolver> pending_write_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_UNDERLYING_SINK_H_
