// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_socket/bluetooth_api_socket.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "net/base/io_buffer.h"

namespace {

const char kSocketNotConnectedError[] = "Socket not connected";
const char kSocketNotListeningError[] = "Socket not listening";

}  // namespace

namespace extensions {

// static
static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<BluetoothApiSocket>>>::DestructorAtExit
    g_server_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<BluetoothApiSocket> >*
ApiResourceManager<BluetoothApiSocket>::GetFactoryInstance() {
  return g_server_factory.Pointer();
}

BluetoothApiSocket::BluetoothApiSocket(const std::string& owner_extension_id)
    : ApiResource(owner_extension_id),
      persistent_(false),
      buffer_size_(0),
      paused_(false),
      connected_(false) {
  DCHECK_CURRENTLY_ON(kThreadId);
}

BluetoothApiSocket::BluetoothApiSocket(
    const std::string& owner_extension_id,
    scoped_refptr<device::BluetoothSocket> socket,
    const std::string& device_address,
    const device::BluetoothUUID& uuid)
    : ApiResource(owner_extension_id),
      socket_(socket),
      device_address_(device_address),
      uuid_(uuid),
      persistent_(false),
      buffer_size_(0),
      paused_(true),
      connected_(true) {
  DCHECK_CURRENTLY_ON(kThreadId);
}

BluetoothApiSocket::~BluetoothApiSocket() {
  DCHECK_CURRENTLY_ON(kThreadId);
  if (socket_.get()) {
    socket_->Disconnect(base::DoNothing());
  }
}

void BluetoothApiSocket::AdoptConnectedSocket(
    scoped_refptr<device::BluetoothSocket> socket,
    const std::string& device_address,
    const device::BluetoothUUID& uuid) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (socket_.get()) {
    socket_->Disconnect(base::DoNothing());
  }

  socket_ = socket;
  device_address_ = device_address;
  uuid_ = uuid;
  connected_ = true;
}

void BluetoothApiSocket::AdoptListeningSocket(
    scoped_refptr<device::BluetoothSocket> socket,
    const device::BluetoothUUID& uuid) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (socket_.get()) {
    socket_->Disconnect(base::DoNothing());
  }

  socket_ = socket;
  device_address_ = "";
  uuid_ = uuid;
  connected_ = false;
}

void BluetoothApiSocket::Disconnect(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (!socket_.get()) {
    std::move(callback).Run();
    return;
  }

  connected_ = false;
  socket_->Disconnect(std::move(callback));
  socket_.reset();
}

bool BluetoothApiSocket::IsPersistent() const {
  DCHECK_CURRENTLY_ON(kThreadId);
  return persistent_;
}

void BluetoothApiSocket::Receive(int count,
                                 ReceiveCompletionCallback success_callback,
                                 ErrorCompletionCallback error_callback) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (!socket_.get() || !IsConnected()) {
    std::move(error_callback)
        .Run(BluetoothApiSocket::kNotConnected, kSocketNotConnectedError);
    return;
  }

  socket_->Receive(
      count, std::move(success_callback),
      base::BindOnce(&OnSocketReceiveError, std::move(error_callback)));
}

// static
void BluetoothApiSocket::OnSocketReceiveError(
    ErrorCompletionCallback error_callback,
    device::BluetoothSocket::ErrorReason reason,
    const std::string& message) {
  DCHECK_CURRENTLY_ON(kThreadId);
  BluetoothApiSocket::ErrorReason error_reason =
      BluetoothApiSocket::kSystemError;
  switch (reason) {
    case device::BluetoothSocket::kIOPending:
      error_reason = BluetoothApiSocket::kIOPending;
      break;
    case device::BluetoothSocket::kDisconnected:
      error_reason = BluetoothApiSocket::kDisconnected;
      break;
    case device::BluetoothSocket::kSystemError:
      error_reason = BluetoothApiSocket::kSystemError;
      break;
  }
  std::move(error_callback).Run(error_reason, message);
}

void BluetoothApiSocket::Send(scoped_refptr<net::IOBuffer> buffer,
                              int buffer_size,
                              SendCompletionCallback success_callback,
                              ErrorCompletionCallback error_callback) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (!socket_.get() || !IsConnected()) {
    std::move(error_callback)
        .Run(BluetoothApiSocket::kNotConnected, kSocketNotConnectedError);
    return;
  }

  socket_->Send(buffer, buffer_size, std::move(success_callback),
                base::BindOnce(&OnSocketSendError, std::move(error_callback)));
}

// static
void BluetoothApiSocket::OnSocketSendError(
    ErrorCompletionCallback error_callback,
    const std::string& message) {
  DCHECK_CURRENTLY_ON(kThreadId);
  std::move(error_callback).Run(BluetoothApiSocket::kSystemError, message);
}

void BluetoothApiSocket::Accept(AcceptCompletionCallback success_callback,
                                ErrorCompletionCallback error_callback) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (!socket_.get() || IsConnected()) {
    std::move(error_callback)
        .Run(BluetoothApiSocket::kNotListening, kSocketNotListeningError);
    return;
  }

  socket_->Accept(
      std::move(success_callback),
      base::BindOnce(&OnSocketAcceptError, std::move(error_callback)));
}

// static
void BluetoothApiSocket::OnSocketAcceptError(
    ErrorCompletionCallback error_callback,
    const std::string& message) {
  DCHECK_CURRENTLY_ON(kThreadId);
  std::move(error_callback).Run(BluetoothApiSocket::kSystemError, message);
}

}  // namespace extensions
