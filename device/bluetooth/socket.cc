// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/socket.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/io_buffer.h"

namespace bluetooth {
namespace {
// TODO(b/269348144) - BluetoothSocket is constructed in UI thread and must also
// be destructed in UI thread. We must keep this reference until disconnect
// completes so that the destructor does not run in the socket thread.
void HoldReferenceUntilDisconnected(
    scoped_refptr<device::BluetoothSocket> socket,
    mojom::Socket::DisconnectCallback callback) {
  std::move(callback).Run();
}
}  // namespace

Socket::Socket(scoped_refptr<device::BluetoothSocket> bluetooth_socket,
               mojo::ScopedDataPipeProducerHandle receive_stream,
               mojo::ScopedDataPipeConsumerHandle send_stream)
    : bluetooth_socket_(std::move(bluetooth_socket)),
      receive_stream_(std::move(receive_stream)),
      send_stream_(std::move(send_stream)),
      receive_stream_watcher_(FROM_HERE,
                              mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      send_stream_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
  receive_stream_watcher_.Watch(
      receive_stream_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&Socket::OnReceiveStreamWritable,
                          base::Unretained(this)));
  send_stream_watcher_.Watch(
      send_stream_.get(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&Socket::OnSendStreamReadable,
                          base::Unretained(this)));
  ReceiveMore();
  SendMore();
}

Socket::~Socket() {
  ShutdownReceive();
  ShutdownSend();
  bluetooth_socket_->Disconnect(base::BindOnce(
      &HoldReferenceUntilDisconnected, bluetooth_socket_, base::DoNothing()));
}

void Socket::Disconnect(DisconnectCallback callback) {
  bluetooth_socket_->Disconnect(base::BindOnce(
      &HoldReferenceUntilDisconnected, bluetooth_socket_, std::move(callback)));
}

void Socket::OnReceiveStreamWritable(MojoResult result) {
  DCHECK(receive_stream_.is_valid());
  if (result == MOJO_RESULT_OK) {
    ReceiveMore();
    return;
  }
  ShutdownReceive();
}

void Socket::ShutdownReceive() {
  receive_stream_watcher_.Cancel();
  receive_stream_.reset();
}

void Socket::ReceiveMore() {
  DCHECK(receive_stream_.is_valid());

  // The destination to which we will write incoming bytes from
  // |bluetooth_socket_|. The allocated buffer and its size will be fetched by
  // calling BeginWriteData() below. This already-allocated buffer is a buffer
  // shared between the 2 sides of |receive_stream_|.
  base::span<uint8_t> pending_write_buffer;

  MojoResult result = receive_stream_->BeginWriteData(
      mojo::DataPipeProducerHandle::kNoSizeHint, MOJO_WRITE_DATA_FLAG_NONE,
      pending_write_buffer);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    receive_stream_watcher_.ArmOrNotify();
    return;
  } else if (result != MOJO_RESULT_OK) {
    ShutdownReceive();
    return;
  }

  bluetooth_socket_->Receive(
      base::checked_cast<int>(pending_write_buffer.size()),
      base::BindOnce(&Socket::OnBluetoothSocketReceive,
                     weak_ptr_factory_.GetWeakPtr(),
                     pending_write_buffer.data()),
      base::BindOnce(&Socket::OnBluetoothSocketReceiveError,
                     weak_ptr_factory_.GetWeakPtr()));
}

void Socket::OnBluetoothSocketReceive(void* pending_write_buffer,
                                      int num_bytes_received,
                                      scoped_refptr<net::IOBuffer> io_buffer) {
  DCHECK_GT(num_bytes_received, 0);
  DCHECK(io_buffer->data());

  if (!receive_stream_.is_valid())
    return;

  memcpy(pending_write_buffer, io_buffer->data(), num_bytes_received);
  receive_stream_->EndWriteData(static_cast<uint32_t>(num_bytes_received));

  ReceiveMore();
}

void Socket::OnBluetoothSocketReceiveError(
    device::BluetoothSocket::ErrorReason error_reason,
    const std::string& error_message) {
  DLOG(ERROR) << "Failed to receive data for reason '" << error_reason << "': '"
              << error_message << "'";
  if (receive_stream_.is_valid()) {
    receive_stream_->EndWriteData(0);
    ShutdownReceive();
  }
}

void Socket::OnSendStreamReadable(MojoResult result) {
  DCHECK(send_stream_.is_valid());
  if (result == MOJO_RESULT_OK)
    SendMore();
  else
    ShutdownSend();
}

void Socket::ShutdownSend() {
  send_stream_watcher_.Cancel();
  send_stream_.reset();
}

void Socket::SendMore() {
  DCHECK(send_stream_.is_valid());

  // The source from which we will write outgoing bytes to |bluetooth_socket_|.
  // The allocated buffer and the number of bytes already written by the other
  // side of |send_stream_| will be fetched by calling BeginReadData() below.
  // This already-allocated buffer is a buffer shared between the 2 sides of
  // |send_stream_|.
  base::span<const uint8_t> pending_read_buffer;
  MojoResult result = send_stream_->BeginReadData(MOJO_WRITE_DATA_FLAG_NONE,
                                                  pending_read_buffer);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    send_stream_watcher_.ArmOrNotify();
    return;
  } else if (result != MOJO_RESULT_OK) {
    ShutdownSend();
    return;
  }

  std::string_view chars = base::as_string_view(pending_read_buffer);
  bluetooth_socket_->Send(base::MakeRefCounted<net::WrappedIOBuffer>(chars),
                          chars.size(),
                          base::BindOnce(&Socket::OnBluetoothSocketSend,
                                         weak_ptr_factory_.GetWeakPtr()),
                          base::BindOnce(&Socket::OnBluetoothSocketSendError,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void Socket::OnBluetoothSocketSend(int num_bytes_sent) {
  DCHECK_GE(num_bytes_sent, 0);

  if (!send_stream_.is_valid())
    return;

  send_stream_->EndReadData(static_cast<uint32_t>(num_bytes_sent));
  SendMore();
}

void Socket::OnBluetoothSocketSendError(const std::string& error_message) {
  DLOG(ERROR) << "Failed to send data: '" << error_message << "'";
  if (send_stream_.is_valid()) {
    send_stream_->EndReadData(0);
    ShutdownSend();
  }
}

}  // namespace bluetooth
