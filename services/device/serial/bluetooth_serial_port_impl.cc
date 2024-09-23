// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/serial/bluetooth_serial_port_impl.h"

#include <limits.h>

#include <algorithm>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/cpp/device_features.h"

namespace device {

// static
void BluetoothSerialPortImpl::Open(
    scoped_refptr<BluetoothAdapter> adapter,
    const std::string& address,
    const BluetoothUUID& service_class_id,
    mojom::SerialConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::SerialPortClient> client,
    mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
    OpenCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kEnableBluetoothSerialPortProfileInSerialApi));

  // This BluetoothSerialPortImpl is owned by its |receiver_| and |watcher_| and
  // will self-destruct on connection failure.
  auto* port = new BluetoothSerialPortImpl(
      std::move(adapter), address, std::move(options), std::move(client),
      std::move(watcher));
  port->OpenSocket(service_class_id, std::move(callback));
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
  if (open_callback_) {
    BLUETOOTH_LOG(ERROR) << "Callback pending at destruction: address: "
                         << address_;
    std::move(open_callback_).Run(mojo::NullRemote());
  }
  if (bluetooth_socket_)
    bluetooth_socket_->Disconnect(base::DoNothing());
}

void BluetoothSerialPortImpl::OpenSocket(const BluetoothUUID& service_class_id,
                                         OpenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BluetoothDevice* device = bluetooth_adapter_->GetDevice(address_);
  if (!device) {
    std::move(callback).Run(mojo::NullRemote());
    delete this;
    return;
  }

  open_callback_ = std::move(callback);
  device->ConnectToService(
      service_class_id,
      base::BindOnce(&BluetoothSerialPortImpl::OnSocketConnected,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothSerialPortImpl::OnSocketConnectedError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothSerialPortImpl::OnSocketConnected(
    scoped_refptr<BluetoothSocket> socket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(socket);
  bluetooth_socket_ = std::move(socket);
  mojo::PendingRemote<mojom::SerialPort> port =
      receiver_.BindNewPipeAndPassRemote();
  receiver_.set_disconnect_handler(
      base::BindOnce([](BluetoothSerialPortImpl* self) { delete self; },
                     base::Unretained(this)));
  std::move(open_callback_).Run(std::move(port));
}

void BluetoothSerialPortImpl::OnSocketConnectedError(
    const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BLUETOOTH_LOG(ERROR) << "Failed to connect socket: address: " << address_
                       << ", message: " << message;
  std::move(open_callback_).Run(mojo::NullRemote());
  delete this;
}

void BluetoothSerialPortImpl::StartWriting(
    mojo::ScopedDataPipeConsumerHandle consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  if (!write_pending_)
    in_stream_watcher_.ArmOrNotify();
}

void BluetoothSerialPortImpl::StartReading(
    mojo::ScopedDataPipeProducerHandle producer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (out_stream_) {
    receiver_.ReportBadMessage("Data pipe producer still open.");
    return;
  }

  if (!bluetooth_socket_) {
    receiver_.ReportBadMessage("No Bluetooth socket.");
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
  if (!read_pending_)
    out_stream_watcher_.ArmOrNotify();
}

void BluetoothSerialPortImpl::ReadFromSocketAndWriteOut(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (result) {
    case MOJO_RESULT_OK:
      DCHECK(!read_pending_);
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
      NOTREACHED_IN_MIGRATION() << "Unexpected Mojo result: " << result;
  }
}

void BluetoothSerialPortImpl::ResetPendingWriteBuffer() {
  pending_write_buffer_ = base::span<char>();
}

void BluetoothSerialPortImpl::ResetReceiveBuffer() {
  receive_buffer_size_ = 0;
  receive_buffer_next_byte_pos_ = 0;
  receive_buffer_.reset();
}

void BluetoothSerialPortImpl::ReadMore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(out_stream_.is_valid());
  DCHECK(!read_pending_);

  base::span<uint8_t> buffer;
  // The |buffer| is owned by |out_stream_|.
  MojoResult result =
      out_stream_->BeginWriteData(mojo::DataPipeProducerHandle::kNoSizeHint,
                                  MOJO_WRITE_DATA_FLAG_NONE, buffer);
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
    const size_t bytes_to_copy = std::min(num_remaining_bytes, buffer.size());
    buffer.copy_prefix_from(receive_buffer_->span().subspan(
        receive_buffer_next_byte_pos_, bytes_to_copy));
    out_stream_->EndWriteData(bytes_to_copy);
    if (bytes_to_copy == num_remaining_bytes) {  // If copied the last byte.
      ResetReceiveBuffer();
    } else {
      receive_buffer_next_byte_pos_ += bytes_to_copy;
    }
    out_stream_watcher_.ArmOrNotify();
    return;
  }
  read_pending_ = true;
  pending_write_buffer_ = base::as_writable_chars(buffer);
  bluetooth_socket_->Receive(
      base::checked_cast<int>(buffer.size()),
      base::BindOnce(&BluetoothSerialPortImpl::OnBluetoothSocketReceive,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothSerialPortImpl::OnBluetoothSocketReceiveError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothSerialPortImpl::OnBluetoothSocketReceive(
    int num_bytes_received,
    scoped_refptr<net::IOBuffer> receive_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(num_bytes_received, 0);
  DCHECK(receive_buffer->data());

  const bool receive_completed_after_flush = pending_write_buffer_.empty();
  read_pending_ = false;

  if (receive_completed_after_flush) {
    if (out_stream_) {
      // The prior pipe was flushed while a read was in-flight. Now that
      // StartReading() has been called, put this received data in the receive
      // buffer which will prepend to the next read output and call ReadMore()
      // to start consuming receive_buffer_.
      receive_buffer_size_ = num_bytes_received;
      receive_buffer_next_byte_pos_ = 0;
      receive_buffer_ = std::move(receive_buffer);
      ReadMore();
    }
    return;
  }

  // Note: Some platform implementations of BluetoothSocket::Receive ignore the
  // buffer size parameter. This means that |num_bytes_received| could be
  // larger than |pending_write_buffer_|. Check to avoid buffer overflow.
  DCHECK(out_stream_);
  size_t bytes_to_copy = std::min(pending_write_buffer_.size(),
                                  static_cast<size_t>(num_bytes_received));
  std::copy(receive_buffer->data(), receive_buffer->data() + bytes_to_copy,
            pending_write_buffer_.data());
  out_stream_->EndWriteData(bytes_to_copy);
  if (pending_write_buffer_.size() < static_cast<size_t>(num_bytes_received)) {
    receive_buffer_size_ = num_bytes_received;
    receive_buffer_next_byte_pos_ = pending_write_buffer_.size();
    receive_buffer_ = std::move(receive_buffer);
  }
  ResetPendingWriteBuffer();

  ReadMore();
}

void BluetoothSerialPortImpl::OnBluetoothSocketReceiveError(
    BluetoothSocket::ErrorReason error_reason,
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  read_pending_ = false;
  ResetPendingWriteBuffer();

  if (out_stream_) {
    if (client_) {
      DCHECK(error_reason != BluetoothSocket::ErrorReason::kIOPending);
      switch (error_reason) {
        case BluetoothSocket::ErrorReason::kDisconnected:
          client_->OnReadError(mojom::SerialReceiveError::DISCONNECTED);
          break;
        case BluetoothSocket::ErrorReason::kIOPending:
          NOTREACHED_IN_MIGRATION();
          break;
        case BluetoothSocket::ErrorReason::kSystemError:
          client_->OnReadError(mojom::SerialReceiveError::SYSTEM_ERROR);
          break;
      }
    }

    out_stream_->EndWriteData(0);
    out_stream_watcher_.Cancel();
    out_stream_.reset();
    ResetReceiveBuffer();
  }
}

void BluetoothSerialPortImpl::WriteToSocket(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (result) {
    case MOJO_RESULT_OK:
      DCHECK(!write_pending_);
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
      NOTREACHED_IN_MIGRATION() << "Unexpected Mojo result: " << result;
  }
}

void BluetoothSerialPortImpl::WriteMore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(in_stream_.is_valid());
  DCHECK(!write_pending_);

  base::span<const uint8_t> buffer;
  // |buffer| is owned by |in_stream_|.
  MojoResult result =
      in_stream_->BeginReadData(MOJO_WRITE_DATA_FLAG_NONE, buffer);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    in_stream_watcher_.ArmOrNotify();
    return;
  }
  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    in_stream_watcher_.Cancel();
    in_stream_.reset();
    if (drain_callback_)
      std::move(drain_callback_).Run();
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
  // Copying the buffer because we might want to close in_stream_, thus
  // invalidating |buffer|, which is passed to Send().
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(buffer.size());
  io_buffer->span().copy_from(buffer);

  // The call to EndReadData() will be delayed until after Send() completes.
  bluetooth_socket_->Send(
      io_buffer, base::checked_cast<int>(buffer.size()),
      base::BindOnce(&BluetoothSerialPortImpl::OnBluetoothSocketSend,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&BluetoothSerialPortImpl::OnBluetoothSocketSendError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BluetoothSerialPortImpl::OnBluetoothSocketSend(int num_bytes_sent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(num_bytes_sent, 0);

  write_pending_ = false;

  if (!flush_next_write_) {
    DCHECK(in_stream_);
    in_stream_->EndReadData(static_cast<uint32_t>(num_bytes_sent));
  }
  flush_next_write_ = false;

  // If |in_stream_| is valid then StartWriting() has been called.
  if (in_stream_) {
    WriteMore();
  }
}

void BluetoothSerialPortImpl::OnBluetoothSocketSendError(
    const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  write_pending_ = false;
  flush_next_write_ = false;

  if (in_stream_) {
    if (client_)
      client_->OnSendError(mojom::SerialSendError::SYSTEM_ERROR);

    in_stream_->EndReadData(0);
    in_stream_watcher_.Cancel();
    in_stream_.reset();
  }
}

void BluetoothSerialPortImpl::OnSocketDisconnected(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run();
  bluetooth_socket_.reset();  // Avoid calling Disconnect() twice.
  delete this;
}

void BluetoothSerialPortImpl::Flush(mojom::SerialPortFlushMode mode,
                                    FlushCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (mode) {
    case mojom::SerialPortFlushMode::kReceiveAndTransmit:
      // Do nothing. This case exists to support the chrome.serial.flush()
      // method and is not used by the Web Serial API.
      break;
    case mojom::SerialPortFlushMode::kReceive:
      if (read_pending_)
        out_stream_->EndWriteData(0);
      out_stream_watcher_.Cancel();
      out_stream_.reset();
      ResetReceiveBuffer();
      ResetPendingWriteBuffer();
      break;
    case mojom::SerialPortFlushMode::kTransmit:
      if (write_pending_) {
        flush_next_write_ = true;
        in_stream_->EndReadData(0);
      }
      in_stream_watcher_.Cancel();
      in_stream_.reset();
      break;
  }

  std::move(callback).Run();
}

void BluetoothSerialPortImpl::Drain(DrainCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!in_stream_) {
    std::move(callback).Run();
    return;
  }

  drain_callback_ = std::move(callback);
}

void BluetoothSerialPortImpl::GetControlSignals(
    GetControlSignalsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto signals = mojom::SerialPortControlSignals::New();
  std::move(callback).Run(std::move(signals));
}

void BluetoothSerialPortImpl::SetControlSignals(
    mojom::SerialHostControlSignalsPtr signals,
    SetControlSignalsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(true);
}

void BluetoothSerialPortImpl::ConfigurePort(
    mojom::SerialConnectionOptionsPtr options,
    ConfigurePortCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  options_ = std::move(options);
  std::move(callback).Run(true);
}

void BluetoothSerialPortImpl::GetPortInfo(GetPortInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto info = mojom::SerialConnectionInfo::New(
      /*bitrate=*/options_->bitrate, /*data_bits=*/options_->data_bits,
      /*parity_bit=*/options_->parity_bit, /*stop_bits=*/options_->stop_bits,
      /*cts_flow_control=*/options_->cts_flow_control);
  std::move(callback).Run(std::move(info));
}

void BluetoothSerialPortImpl::Close(bool flush, CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It is safe to ignore |flush| because we get the semantics of a flush simply
  // by disconnecting the socket. There are no hardware buffers to clear.
  bluetooth_socket_->Disconnect(
      base::BindOnce(&BluetoothSerialPortImpl::OnSocketDisconnected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

}  // namespace device
