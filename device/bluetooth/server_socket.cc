// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/server_socket.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/device.h"
#include "device/bluetooth/socket.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/io_buffer.h"

namespace bluetooth {
namespace {
// TODO(b/269348144) - BluetoothSocket is constructed in UI thread and must also
// be destructed in UI thread. We must keep this reference until disconnect
// completes so that the destructor does not run in the socket thread.
void HoldReferenceUntilDisconnected(
    scoped_refptr<device::BluetoothSocket> server_socket,
    mojom::ServerSocket::DisconnectCallback callback) {
  std::move(callback).Run();
}
}  // namespace

ServerSocket::ServerSocket(
    scoped_refptr<device::BluetoothSocket> bluetooth_socket)
    : server_socket_(std::move(bluetooth_socket)) {}

ServerSocket::~ServerSocket() {
  server_socket_->Disconnect(base::BindOnce(&HoldReferenceUntilDisconnected,
                                            server_socket_, base::DoNothing()));
}

void ServerSocket::Accept(AcceptCallback callback) {
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  server_socket_->Accept(
      base::BindOnce(&ServerSocket::OnAccept, weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&ServerSocket::OnAcceptError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.second)));
}

void ServerSocket::Disconnect(DisconnectCallback callback) {
  DCHECK(server_socket_);
  server_socket_->Disconnect(base::BindOnce(
      &HoldReferenceUntilDisconnected, server_socket_, std::move(callback)));
}

void ServerSocket::OnAccept(
    AcceptCallback callback,
    const device::BluetoothDevice* device,
    scoped_refptr<device::BluetoothSocket> bluetooth_socket) {
  mojo::ScopedDataPipeProducerHandle receive_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_pipe_consumer_handle;
  MojoResult result =
      mojo::CreateDataPipe(/*options=*/nullptr, receive_pipe_producer_handle,
                           receive_pipe_consumer_handle);
  if (result != MOJO_RESULT_OK) {
    bluetooth_socket->Disconnect(base::BindOnce(
        &ServerSocket::OnAcceptError, weak_ptr_factory_.GetWeakPtr(),
        std::move(callback), "Failed to create receiving DataPipe."));
    return;
  }

  mojo::ScopedDataPipeProducerHandle send_pipe_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_pipe_consumer_handle;
  result = mojo::CreateDataPipe(/*options=*/nullptr, send_pipe_producer_handle,
                                send_pipe_consumer_handle);
  if (result != MOJO_RESULT_OK) {
    bluetooth_socket->Disconnect(base::BindOnce(
        &ServerSocket::OnAcceptError, weak_ptr_factory_.GetWeakPtr(),
        std::move(callback), "Failed to create sending DataPipe."));
    return;
  }

  mojo::PendingRemote<mojom::Socket> pending_socket;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<Socket>(std::move(bluetooth_socket),
                               std::move(receive_pipe_producer_handle),
                               std::move(send_pipe_consumer_handle)),
      pending_socket.InitWithNewPipeAndPassReceiver());

  mojom::AcceptConnectionResultPtr accept_connection_result =
      mojom::AcceptConnectionResult::New();
  accept_connection_result->device = Device::ConstructDeviceInfoStruct(device);
  accept_connection_result->socket = std::move(pending_socket);
  accept_connection_result->receive_stream =
      std::move(receive_pipe_consumer_handle);
  accept_connection_result->send_stream = std::move(send_pipe_producer_handle);

  std::move(callback).Run(std::move(accept_connection_result));
}

void ServerSocket::OnAcceptError(AcceptCallback callback,
                                 const std::string& error_message) {
  LOG(ERROR) << "Failed to accept incoming connection: " << error_message;
  std::move(callback).Run(/*result=*/nullptr);
}

}  // namespace bluetooth
