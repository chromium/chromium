// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_BLUETOOTH_SERIAL_PORT_IMPL_H_
#define SERVICES_DEVICE_SERIAL_BLUETOOTH_SERIAL_PORT_IMPL_H_

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/serial_io_handler.h"
#include "services/device/serial/serial_port_impl.h"

namespace device {

class BluetoothUUID;

// This class is intended to allow serial communication using a Bluetooth
// SPP device. The Bluetooth device is used to create a Bluetooth socket
// which is closed upon error in any of the interface functions.
class BluetoothSerialPortImpl : public mojom::SerialPort {
 public:
  using OpenCallback =
      base::OnceCallback<void(mojo::PendingRemote<mojom::SerialPort>)>;

  // Creates of instance of BluetoothSerialPortImpl using a Bluetooth
  // adapter, a Bluetooth device address and a receiver/watcher to
  // create a pipe. The receiver and watcher will own this object.
  static void Open(
      scoped_refptr<BluetoothAdapter> adapter,
      const std::string& address,
      const BluetoothUUID& service_class_id,
      mojom::SerialConnectionOptionsPtr options,
      mojo::PendingRemote<mojom::SerialPortClient> client,
      mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
      OpenCallback callback);

  BluetoothSerialPortImpl(
      scoped_refptr<BluetoothAdapter> adapter,
      const std::string& address,
      mojom::SerialConnectionOptionsPtr options,
      mojo::PendingRemote<mojom::SerialPortClient> client,
      mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher);
  BluetoothSerialPortImpl(const BluetoothSerialPortImpl&) = delete;
  BluetoothSerialPortImpl& operator=(const BluetoothSerialPortImpl&) = delete;
  ~BluetoothSerialPortImpl() override;

 private:
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

  void OpenSocket(const BluetoothUUID& service_class_id, OpenCallback callback);
  void WriteToSocket(MojoResult result, const mojo::HandleSignalsState& state);
  void ReadFromSocketAndWriteOut(MojoResult result,
                                 const mojo::HandleSignalsState& state);

  void ResetPendingWriteBuffer();
  void ResetReceiveBuffer();

  void ReadMore();
  void WriteMore();

  void OnSocketConnected(scoped_refptr<BluetoothSocket> socket);
  void OnSocketConnectedError(const std::string& message);

  void OnBluetoothSocketReceive(int num_bytes_received,
                                scoped_refptr<net::IOBuffer> receive_buffer);
  void OnBluetoothSocketReceiveError(
      device::BluetoothSocket::ErrorReason error_reason,
      const std::string& error_message);
  void OnBluetoothSocketSend(int num_bytes_sent);
  void OnBluetoothSocketSendError(const std::string& error_message);
  void OnSocketDisconnected(CloseCallback callback);

  mojo::Receiver<mojom::SerialPort> receiver_{this};
  mojo::Remote<mojom::SerialPortConnectionWatcher> watcher_;
  mojo::Remote<mojom::SerialPortClient> client_;

  // Data pipes for input and output.
  mojo::ScopedDataPipeConsumerHandle in_stream_;
  mojo::SimpleWatcher in_stream_watcher_;
  mojo::ScopedDataPipeProducerHandle out_stream_;
  mojo::SimpleWatcher out_stream_watcher_;

  // Used for pending writes to |out_stream_|. When empty this indicates that
  // |out_stream_| has been closed (and possibly replaced).
  base::raw_span<char> pending_write_buffer_;

  // Holds the callback for a drain until pending operations have been
  // completed.
  DrainCallback drain_callback_;

  scoped_refptr<BluetoothSocket> bluetooth_socket_;
  const scoped_refptr<BluetoothAdapter> bluetooth_adapter_;
  const std::string address_;

  bool read_pending_ = false;
  bool write_pending_ = false;

  // Field to track whether the a write had a Flush call.
  bool flush_next_write_ = false;

  // |receive_buffer_| is used to temporarily hold larger than expected
  // BluetoothSocket::Receive() responses, or responses received on a replaced
  // |out_stream_|.
  size_t receive_buffer_size_ = 0;
  size_t receive_buffer_next_byte_pos_ = 0;
  scoped_refptr<net::IOBuffer> receive_buffer_;

  mojom::SerialConnectionOptionsPtr options_;

  // Callback for the ongoing connect socket request.
  OpenCallback open_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BluetoothSerialPortImpl> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_BLUETOOTH_SERIAL_PORT_IMPL_H_
