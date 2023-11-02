// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/test/fake_bluetooth_socket.h"

namespace device {

FakeBluetoothSocket::FakeBluetoothSocket() = default;
FakeBluetoothSocket::~FakeBluetoothSocket() = default;

void FakeBluetoothSocket::Disconnect(base::OnceClosure success_callback) {
  called_disconnect_ = true;
  std::move(success_callback).Run();
}

void FakeBluetoothSocket::Receive(
    int buffer_size,
    ReceiveCompletionCallback success_callback,
    ReceiveErrorCompletionCallback error_callback) {
  DCHECK(!receive_args_);
  receive_args_ = std::make_unique<ReceiveArgs>(
      buffer_size, std::move(success_callback), std::move(error_callback));
}

void FakeBluetoothSocket::Send(scoped_refptr<net::IOBuffer> buffer,
                               int buffer_size,
                               SendCompletionCallback success_callback,
                               ErrorCompletionCallback error_callback) {
  DCHECK(!send_args_);
  send_args_ = std::make_unique<SendArgs>(std::move(buffer), buffer_size,
                                          std::move(success_callback),
                                          std::move(error_callback));
}

void FakeBluetoothSocket::Accept(AcceptCompletionCallback success_callback,
                                 ErrorCompletionCallback error_callback) {
  DCHECK(!accept_args_);
  accept_args_ = std::make_unique<AcceptArgs>(std::move(success_callback),
                                              std::move(error_callback));
}

}  // namespace device
