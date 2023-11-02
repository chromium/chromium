// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_SOCKET_H_
#define DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_SOCKET_H_

#include <string>

#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "net/base/io_buffer.h"

namespace device {

class FakeBluetoothSocket : public BluetoothSocket {
 public:
  using ReceiveArgs = std::
      tuple<int, ReceiveCompletionCallback, ReceiveErrorCompletionCallback>;
  using SendArgs = std::tuple<scoped_refptr<net::IOBuffer>,
                              int,
                              SendCompletionCallback,
                              ErrorCompletionCallback>;
  using AcceptArgs =
      std::tuple<AcceptCompletionCallback, ErrorCompletionCallback>;

  FakeBluetoothSocket();

  FakeBluetoothSocket(const FakeBluetoothSocket&) = delete;
  FakeBluetoothSocket& operator=(const FakeBluetoothSocket&) = delete;

  // BluetoothSocket:
  void Disconnect(base::OnceClosure success_callback) override;
  void Receive(int buffer_size,
               ReceiveCompletionCallback success_callback,
               ReceiveErrorCompletionCallback error_callback) override;
  void Send(scoped_refptr<net::IOBuffer> buffer,
            int buffer_size,
            SendCompletionCallback success_callback,
            ErrorCompletionCallback error_callback) override;
  void Accept(AcceptCompletionCallback success_callback,
              ErrorCompletionCallback error_callback) override;

  bool called_disconnect() { return called_disconnect_; }

  bool HasReceiveArgs() { return receive_args_.get(); }
  bool HasSendArgs() { return send_args_.get(); }
  bool HasAcceptArgs() { return accept_args_.get(); }

  std::unique_ptr<ReceiveArgs> TakeReceiveArgs() {
    return std::move(receive_args_);
  }

  std::unique_ptr<SendArgs> TakeSendArgs() { return std::move(send_args_); }

  std::unique_ptr<AcceptArgs> TakeAcceptArgs() {
    return std::move(accept_args_);
  }

 protected:
  ~FakeBluetoothSocket() override;

 private:
  bool called_disconnect_ = false;
  std::unique_ptr<ReceiveArgs> receive_args_;
  std::unique_ptr<SendArgs> send_args_;
  std::unique_ptr<AcceptArgs> accept_args_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_FAKE_BLUETOOTH_SOCKET_H_
