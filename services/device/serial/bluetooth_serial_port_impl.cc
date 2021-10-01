// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/bluetooth_serial_port_impl.h"

#include <limits.h>
#include <algorithm>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "net/base/io_buffer.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/cpp/serial/serial_switches.h"

namespace device {

// static
void BluetoothSerialPortImpl::Open(
    scoped_refptr<BluetoothAdapter> adapter,
    const std::string& address,
    mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    OpenCallback callback) {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableBluetoothSerialPortProfileInSerialApi));

  // This BluetoothSerialPortImpl is owned by its |receiver_| and |watcher_| and
  // will self-destruct on connection failure.
  auto* port = new BluetoothSerialPortImpl(
      std::move(adapter), address, std::move(options), std::move(client),
      std::move(watcher));
  port->OpenSocket(std::move(callback));
}

BluetoothSerialPortImpl::BluetoothSerialPortImpl(
    scoped_refptr<BluetoothAdapter> adapter,
    const std::string& address,
    mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher)
    : watcher_(std::move(watcher)),
      client_(std::move(client)),
      in_stream_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      out_stream_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      bluetooth_adapter_(std::move(adapter)),
      address_(address),
      options_(std::move(options)) {
  if (watcher_.is_bound()) {
    watcher_.set_disconnect_handler(
        base::BindOnce([](BluetoothSerialPortImpl* self) { delete self; },
                       base::Unretained(this)));
  }
}

BluetoothSerialPortImpl::~BluetoothSerialPortImpl() {
  if (bluetooth_socket_)
    bluetooth_socket_->Disconnect(base::DoNothing());
}

void BluetoothSerialPortImpl::OpenSocket(OpenCallback callback) {
  BluetoothDevice* device = bluetooth_adapter_->GetDevice(address_);
  if (!device) {
    std::move(callback).Run(mojo::NullRemote());
    delete this;
    return;
  }

  BluetoothDevice::UUIDSet device_uuids = device->GetUUIDs();
  if (base::Contains(device_uuids, GetSerialPortProfileUUID())) {
    auto split_callback = base::SplitOnceCallback(std::move(callback));
    device->ConnectToService(
        GetSerialPortProfileUUID(),
        base::BindOnce(&BluetoothSerialPortImpl::OnSocketConnected,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(split_callback.first)),
        base::BindOnce(&BluetoothSerialPortImpl::OnSocketConnectedError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(split_callback.second)));
    return;
  }

  std::move(callback).Run(mojo::NullRemote());
  delete this;
}

void BluetoothSerialPortImpl::OnSocketConnected(
    OpenCallback callback,
    scoped_refptr<BluetoothSocket> socket) {
  DCHECK(socket);
  bluetooth_socket_ = std::move(socket);
  mojo::PendingRemote<mojom::SerialPort> port =
      receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(
      base::BindOnce([](BluetoothSerialPortImpl* self) { delete self; },
                     base::Unretained(this)));
  std::move(callback).Run(std::move(port));
}

void BluetoothSerialPortImpl::OnSocketConnectedError(
    OpenCallback callback,
    const std::string& message) {
  std::move(callback).Run(mojo::NullRemote());
  delete this;
}

void BluetoothSerialPortImpl::StartWriting(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  DCHECK(!write_pending_);

  if (in_stream_) {
    receiver_.ReportBadMessage("Data pipe consumer still open.");
    return;
  }

  if (!bluetooth_socket_) {
    receiver_.ReportBadMessage("No Bluetooth socket.");
    return;
  }

  in_stream_watcher_.Cancel();
  in_stream_ = std::move(consumer);
  in_stream_watcher_.Watch(
      in_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&BluetoothSerialPortImpl::WriteToSocket,
                          weak_ptr_factory_.GetWeakPtr()));
  in_stream_watcher_.ArmOrNotify();
}

void BluetoothSerialPortImpl::StartReading(
    mojo::ScopedDataPipeProducerHandle producer) {
  if (out_stream_) {
    mojo::ReportBadMessage("Data pipe producer still open.");
    return;
  }

  if (!bluetooth_socket_) {
    mojo::ReportBadMessage("No Bluetooth socket.");
    return;
  }

  out_stream_watcher_.Cancel();
  out_stream_ = std::move(producer);
  out_stream_watcher_.Watch(
      out_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&BluetoothSerialPortImpl::ReadFromSocketAndWriteOut,
                          weak_ptr_factory_.GetWeakPtr()));
  out_stream_watcher_.ArmOrNotify();
}

void BluetoothSerialPortImpl::ReadFromSocketAndWriteOut(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  switch (result) {
    case MOJO_RESULT_OK:
      ReadMore();
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      // If there is no space to write, wait for more space.
      out_stream_watcher_.ArmOrNotify();
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
    case MOJO_RESULT_CANCELLED:
      // The |out_stream_| has been closed.
      out_stream_watcher_.Cancel();
      out_stream_.reset();
      break;
    default:
      NOTREACHED() << "Unexpected Mojo result: " << result;
  }
}

void BluetoothSerialPortImpl::ReadMore() {
  DCHECK(out_stream_.is_valid());

  void* buffer = nullptr;

  uint32_t buffer_num_bytes = 0;
  // The |buffer| is owned by |out_stream_|.
  MojoResult result = out_stream_->BeginWriteData(&buffer, &buffer_num_bytes,
                                                  MOJO_WRITE_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    out_stream_watcher_.ArmOrNotify();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    out_stream_watcher_.Cancel();
    out_stream_.reset();
    return;
  }

  if (!bluetooth_socket_) {
    mojo::ReportBadMessage("No Bluetooth socket.");
    return;
  }

  if (receive_buffer_) {
    const size_t num_remaining_bytes =
        receive_buffer_size_ - receive_buffer_next_byte_pos_;
    const size_t bytes_to_copy =
        std::min(num_remaining_bytes, size_t{buffer_num_bytes});
    std::copy(
        receive_buffer_->data() + receive_buffer_next_byte_pos_,
        receive_buffer_->data() + receive_buffer_next_byte_pos_ + bytes_to_copy,
        reinterpret_cast<char*>(buffer));
    out_stream_->EndWriteData(bytes_to_copy);
    if (bytes_to_copy == num_remaining_bytes) {  // If copied the last byte.
      receive_buffer_size_ = 0;
      receive_buffer_next_byte_pos_ = 0;
      receive_buffer_.reset();
    } else {
      receive_buffer_next_byte_pos_ += buffer_num_bytes;
    }
    out_stream_watcher_.ArmOrNotify();
    return;
  }
  if (read_pending_)
    return;
  read_pending_ = true;
  bluetooth_socket_->Receive(
      buffer_num_bytes,
      base::BindOnce(
          &BluetoothSerialPortImpl::OnBluetoothSocketReceive,
          weak_ptr_factory_.GetWeakPtr(),
          base::make_span(reinterpret_cast<char*>(buffer), buffer_num_bytes)),
      base::BindOnce(&BluetoothSerialPortImpl::OnBluetoothSocketReceiveError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothSerialPortImpl::OnBluetoothSocketReceive(
    base::span<char> pending_write_buffer,
    int num_bytes_received,
    scoped_refptr<net::IOBuffer> io_buffer) {
  DCHECK_GT(num_bytes_received, 0);
  DCHECK(io_buffer->data());
  DCHECK(out_stream_.is_valid());

  read_pending_ = false;
  // Note: Some platform implementations of BluetoothSocket::Receive ignore the
  // buffer size parameter. This means that |num_bytes_received| could be
  // larger than |pending_write_buffer|. Check to avoid buffer overflow.
  size_t bytes_to_copy = std::min(pending_write_buffer.size(),
                                  static_cast<size_t>(num_bytes_received));
  std::copy(io_buffer->data(), io_buffer->data() + bytes_to_copy,
            pending_write_buffer.data());
  out_stream_->EndWriteData(bytes_to_copy);
  if (pending_write_buffer.size() < static_cast<size_t>(num_bytes_received)) {
    receive_buffer_size_ = num_bytes_received;
    receive_buffer_next_byte_pos_ = pending_write_buffer.size();
    receive_buffer_ = std::move(io_buffer);
  }

  if (read_flush_callback_) {
    std::move(read_flush_callback_).Run();
    out_stream_->EndWriteData(0);
    out_stream_watcher_.Cancel();
    out_stream_.reset();
    return;
  }
  ReadMore();
}

void BluetoothSerialPortImpl::OnBluetoothSocketReceiveError(
    BluetoothSocket::ErrorReason error_reason,
    const std::string& error_message) {
  DCHECK(out_stream_.is_valid());
  read_pending_ = false;
  if (client_) {
    DCHECK(error_reason != BluetoothSocket::ErrorReason::kIOPending);
    switch (error_reason) {
      case BluetoothSocket::ErrorReason::kDisconnected:
        client_->OnReadError(mojom::SerialReceiveError::DISCONNECTED);
        break;
      case BluetoothSocket::ErrorReason::kIOPending:
        NOTREACHED();
        break;
      case BluetoothSocket::ErrorReason::kSystemError:
        client_->OnReadError(mojom::SerialReceiveError::SYSTEM_ERROR);
        break;
    }
  }
  if (read_flush_callback_)
    std::move(read_flush_callback_).Run();
  out_stream_->EndWriteData(0);
  out_stream_watcher_.Cancel();
  out_stream_.reset();
}

void BluetoothSerialPortImpl::WriteToSocket(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  switch (result) {
    case MOJO_RESULT_OK:
      WriteMore();
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      // If there is no space to write, wait for more space.
      in_stream_watcher_.ArmOrNotify();
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
    case MOJO_RESULT_CANCELLED:
      // The |in_stream_| has been closed.
      in_stream_watcher_.Cancel();
      in_stream_.reset();

      if (drain_callback_)
        std::move(drain_callback_).Run();
      break;
    default:
      NOTREACHED() << "Unexpected Mojo result: " << result;
  }
}

void BluetoothSerialPortImpl::WriteMore() {
  DCHECK(in_stream_.is_valid());

  const void* buffer = nullptr;
  uint32_t buffer_size = 0;
  // |buffer| is owned by |in_stream_|.
  MojoResult result = in_stream_->BeginReadData(&buffer, &buffer_size,
                                                MOJO_WRITE_DATA_FLAG_NONE);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    in_stream_watcher_.ArmOrNotify();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    in_stream_watcher_.Cancel();
    in_stream_.reset();
    return;
  }

  if (!bluetooth_socket_) {
    mojo::ReportBadMessage("No Bluetooth socket.");
    return;
  }

  write_pending_ = true;
  bluetooth_socket_->Send(
      base::MakeRefCounted<net::WrappedIOBuffer>(
          reinterpret_cast<const char*>(buffer)),
      buffer_size,
      base::BindOnce(&BluetoothSerialPortImpl::OnBluetoothSocketSend,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothSerialPortImpl::OnBluetoothSocketSendError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothSerialPortImpl::OnBluetoothSocketSend(int num_bytes_sent) {
  DCHECK_GE(num_bytes_sent, 0);
  DCHECK(in_stream_.is_valid());

  write_pending_ = false;

  in_stream_->EndReadData(static_cast<uint32_t>(num_bytes_sent));

  if (write_flush_callback_) {
    std::move(write_flush_callback_).Run();
    in_stream_->EndReadData(0);
    in_stream_watcher_.Cancel();
    in_stream_.reset();
    return;
  }
  WriteMore();
}

void BluetoothSerialPortImpl::OnBluetoothSocketSendError(
    const std::string& error_message) {
  DCHECK(in_stream_.is_valid());
  write_pending_ = false;
  if (client_)
    client_->OnSendError(mojom::SerialSendError::SYSTEM_ERROR);
  if (write_flush_callback_)
    std::move(write_flush_callback_).Run();
  in_stream_->EndReadData(0);
  in_stream_watcher_.Cancel();
  in_stream_.reset();
}

void BluetoothSerialPortImpl::OnSocketDisconnected(CloseCallback callback) {
  std::move(callback).Run();
  bluetooth_socket_.reset();  // Avoid calling Disconnect() twice.
  delete this;
}

void BluetoothSerialPortImpl::Flush(mojom::SerialPortFlushMode mode,
                                    FlushCallback callback) {
  NOTIMPLEMENTED();
}

void BluetoothSerialPortImpl::Drain(DrainCallback callback) {
  if (!in_stream_) {
    std::move(callback).Run();
    return;
  }

  drain_callback_ = std::move(callback);
}

void BluetoothSerialPortImpl::GetControlSignals(
    GetControlSignalsCallback callback) {
  auto signals = mojom::SerialPortControlSignals::New();
  std::move(callback).Run(std::move(signals));
}

void BluetoothSerialPortImpl::SetControlSignals(
    mojom::SerialHostControlSignalsPtr signals,
    SetControlSignalsCallback callback) {
  std::move(callback).Run(true);
}

void BluetoothSerialPortImpl::ConfigurePort(
    mojom::SerialConnectionOptionsPtr options,
    ConfigurePortCallback callback) {
  options_ = std::move(options);
  std::move(callback).Run(true);
}

void BluetoothSerialPortImpl::GetPortInfo(GetPortInfoCallback callback) {
  auto info = mojom::SerialConnectionInfo::New(
      /*bitrate=*/options_->bitrate, /*data_bits=*/options_->data_bits,
      /*parity_bit=*/options_->parity_bit, /*stop_bits=*/options_->stop_bits,
      /*cts_flow_control=*/options_->cts_flow_control);
  std::move(callback).Run(std::move(info));
}

void BluetoothSerialPortImpl::Close(CloseCallback callback) {
  bluetooth_socket_->Disconnect(
      base::BindOnce(&BluetoothSerialPortImpl::OnSocketDisconnected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

}  // namespace device
