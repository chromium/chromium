// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_PORT_IMPL_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_PORT_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class SerialIoHandler;

// TODO(leonhsl): Merge this class with SerialIoHandler if/once
// SerialIoHandler is exposed only via the Device Service.
// crbug.com/748505
// This class must be constructed and run on IO thread.
class SerialPortImpl : public mojom::SerialPort {
 public:
  using OpenCallback =
      base::OnceCallback<void(mojo::PendingRemote<mojom::SerialPort>)>;

  static void Open(
      const base::FilePath& path,
      mojom::SerialConnectionOptionsPtr options,
      mojo::PendingRemote<mojom::SerialPortClient> client,
      mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      OpenCallback callback);

  static void OpenForTesting(
      scoped_refptr<SerialIoHandler> io_handler,
      mojom::SerialConnectionOptionsPtr options,
      mojo::PendingRemote<mojom::SerialPortClient> client,
      mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
      OpenCallback callback);

  SerialPortImpl(const SerialPortImpl&) = delete;
  SerialPortImpl& operator=(const SerialPortImpl&) = delete;

 private:
  SerialPortImpl(
      scoped_refptr<SerialIoHandler> io_handler,
      mojo::PendingRemote<mojom::SerialPortClient> client,
      mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher);
  ~SerialPortImpl() override;

  // mojom::SerialPort methods:
  void StartWriting(mojo::ScopedDataPipeConsumerHandle consumer) override;
  void StartReading(mojo::ScopedDataPipeProducerHandle producer) override;
  void Flush(mojom::SerialPortFlushMode mode, FlushCallback callback) override;
  void Drain(DrainCallback callback) override;
  void GetControlSignals(GetControlSignalsCallback callback) override;
  void SetControlSignals(mojom::SerialHostControlSignalsPtr signals,
                         SetControlSignalsCallback callback) override;
  void ConfigurePort(mojom::SerialConnectionOptionsPtr options,
                     ConfigurePortCallback callback) override;
  void GetPortInfo(GetPortInfoCallback callback) override;
  void Close(bool flush, CloseCallback callback) override;

  void OpenPort(const mojom::SerialConnectionOptions& options,
                OpenCallback callback);
  void PortOpened(OpenCallback callback, bool success);
  void WriteToPort(MojoResult result, const mojo::HandleSignalsState& state);
  void OnWriteToPortCompleted(uint32_t bytes_sent,
                              mojom::SerialSendError error);
  void ReadFromPortAndWriteOut(MojoResult result,
                               const mojo::HandleSignalsState& state);
  void WriteToOutStream(uint32_t bytes_read, mojom::SerialReceiveError error);
  void PortClosed(CloseCallback callback);

  mojo::Receiver<mojom::SerialPort> receiver_{this};

  // Underlying connection to the serial port.
  scoped_refptr<SerialIoHandler> io_handler_;

  // Client interfaces.
  mojo::Remote<mojom::SerialPortClient> client_;
  mojo::Remote<mojom::SerialPortConnectionWatcher> watcher_;

  // Data pipes for input and output.
  mojo::ScopedDataPipeConsumerHandle in_stream_;
  mojo::SimpleWatcher in_stream_watcher_;
  mojo::ScopedDataPipeProducerHandle out_stream_;
  mojo::SimpleWatcher out_stream_watcher_;

  // Holds the callback for a flush or drain until pending operations have been
  // completed.
  FlushCallback read_flush_callback_;
  FlushCallback write_flush_callback_;
  DrainCallback drain_callback_;

  base::WeakPtrFactory<SerialPortImpl> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_PORT_IMPL_H_
