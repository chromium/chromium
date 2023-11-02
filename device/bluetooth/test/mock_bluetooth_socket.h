// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_SOCKET_H_
#define DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_SOCKET_H_

#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {

class MockBluetoothSocket : public BluetoothSocket {
 public:
  MockBluetoothSocket();
  MOCK_METHOD1(Disconnect, void(base::OnceClosure success_callback));
  MOCK_METHOD3(Receive,
               void(int count,
                    ReceiveCompletionCallback success_callback,
                    ReceiveErrorCompletionCallback error_callback));
  MOCK_METHOD4(Send,
               void(scoped_refptr<net::IOBuffer> buffer,
                    int buffer_size,
                    SendCompletionCallback success_callback,
                    ErrorCompletionCallback error_callback));
  MOCK_METHOD2(Accept,
               void(AcceptCompletionCallback success_callback,
                    ErrorCompletionCallback error_callback));

 protected:
  ~MockBluetoothSocket() override;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_MOCK_BLUETOOTH_SOCKET_H_
