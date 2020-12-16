// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/bluetooth_socket/bluetooth_api_socket.h"

#include "base/bind.h"
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
  if (socket_.get())
    socket_->Close();
}

void BluetoothApiSocket::AdoptConnectedSocket(
    scoped_refptr<device::BluetoothSocket> socket,
    const std::string& device_address,
    const device::BluetoothUUID& uuid) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (socket_.get())
    socket_->Close();

  socket_ = socket;
  device_address_ = device_address;
  uuid_ = uuid;
  connected_ = true;
}

void BluetoothApiSocket::AdoptListeningSocket(
    scoped_refptr<device::BluetoothSocket> socket,
    const device::BluetoothUUID& uuid) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (socket_.get())
    socket_->Close();

  socket_ = socket;
  device_address_ = "";
  uuid_ = uuid;
  connected_ = false;
}

void BluetoothApiSocket::Disconnect(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (!socket_.get()) {
    callback.Run();
    return;
  }

  connected_ = false;
  socket_->Disconnect(callback);
}

bool BluetoothApiSocket::IsPersistent() const {
  DCHECK_CURRENTLY_ON(kThreadId);
  return persistent_;
}

void BluetoothApiSocket::Receive(
    int count,
    const ReceiveCompletionCallback& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (!socket_.get() || !IsConnected()) {
    error_callback.Run(BluetoothApiSocket::kNotConnected,
                       kSocketNotConnectedError);
    return;
  }

  socket_->Receive(count, success_callback,
                   base::BindOnce(&OnSocketReceiveError, error_callback));
}

// static
void BluetoothApiSocket::OnSocketReceiveError(
    const ErrorCompletionCallback& error_callback,
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
  error_callback.Run(error_reason, message);
}

void BluetoothApiSocket::Send(scoped_refptr<net::IOBuffer> buffer,
                              int buffer_size,
                              const SendCompletionCallback& success_callback,
                              const ErrorCompletionCallback& error_callback) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (!socket_.get() || !IsConnected()) {
    error_callback.Run(BluetoothApiSocket::kNotConnected,
                       kSocketNotConnectedError);
    return;
  }

  socket_->Send(buffer, buffer_size, success_callback,
                base::BindOnce(&OnSocketSendError, error_callback));
}

// static
void BluetoothApiSocket::OnSocketSendError(
    const ErrorCompletionCallback& error_callback,
    const std::string& message) {
  DCHECK_CURRENTLY_ON(kThreadId);
  error_callback.Run(BluetoothApiSocket::kSystemError, message);
}

void BluetoothApiSocket::Accept(
    const AcceptCompletionCallback& success_callback,
    const ErrorCompletionCallback& error_callback) {
  DCHECK_CURRENTLY_ON(kThreadId);

  if (!socket_.get() || IsConnected()) {
    error_callback.Run(BluetoothApiSocket::kNotListening,
                       kSocketNotListeningError);
    return;
  }

  socket_->Accept(success_callback,
                  base::BindOnce(&OnSocketAcceptError, error_callback));
}

// static
void BluetoothApiSocket::OnSocketAcceptError(
    const ErrorCompletionCallback& error_callback,
    const std::string& message) {
  DCHECK_CURRENTLY_ON(kThreadId);
  error_callback.Run(BluetoothApiSocket::kSystemError, message);
}

}  // namespace extensions
