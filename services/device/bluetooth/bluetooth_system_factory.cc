// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/bluetooth/bluetooth_system_factory.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/bluetooth/bluetooth_system.h"

namespace device {

void BluetoothSystemFactory::CreateFactory(
    mojo::PendingReceiver<mojom::BluetoothSystemFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<BluetoothSystemFactory>(),
                              std::move(receiver));
}

BluetoothSystemFactory::BluetoothSystemFactory() = default;

BluetoothSystemFactory::~BluetoothSystemFactory() = default;

void BluetoothSystemFactory::Create(
    mojo::PendingReceiver<mojom::BluetoothSystem> system_receiver,
    mojo::PendingRemote<mojom::BluetoothSystemClient> system_client) {
  BluetoothSystem::Create(std::move(system_receiver), std::move(system_client));
}

}  // namespace device
