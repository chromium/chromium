// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/test/fake_serial_port_client.h"

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace device {

FakeSerialPortClient::FakeSerialPortClient() = default;

FakeSerialPortClient::~FakeSerialPortClient() = default;

// static
mojo::PendingRemote<mojom::SerialPortClient> FakeSerialPortClient::Create() {
  mojo::PendingRemote<mojom::SerialPortClient> remote;
  mojo::MakeSelfOwnedReceiver(std::make_unique<FakeSerialPortClient>(),
                              remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeSerialPortClient::OnReadError(mojom::SerialReceiveError error) {}

void FakeSerialPortClient::OnSendError(mojom::SerialSendError error) {}

}  // namespace device
