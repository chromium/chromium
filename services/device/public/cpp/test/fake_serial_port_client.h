// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SERIAL_PORT_CLIENT_H_
#define SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SERIAL_PORT_CLIENT_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/serial.mojom.h"

namespace device {

class FakeSerialPortClient : public mojom::SerialPortClient {
 public:
  FakeSerialPortClient();
  FakeSerialPortClient(FakeSerialPortClient&) = delete;
  FakeSerialPortClient& operator=(FakeSerialPortClient&) = delete;
  ~FakeSerialPortClient() override;

  static mojo::PendingRemote<mojom::SerialPortClient> Create();

  // device::mojom::SerialPortClient
  void OnReadError(mojom::SerialReceiveError error) override;
  void OnSendError(mojom::SerialSendError error) override;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_TEST_FAKE_SERIAL_PORT_CLIENT_H_
