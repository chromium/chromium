// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BLUETOOTH_BLUETOOTH_SYSTEM_FACTORY_H_
#define SERVICES_DEVICE_BLUETOOTH_BLUETOOTH_SYSTEM_FACTORY_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace device {

class BluetoothSystemFactory : public mojom::BluetoothSystemFactory {
 public:
  static void CreateFactory(
      mojo::PendingReceiver<mojom::BluetoothSystemFactory> receiver);

  BluetoothSystemFactory();
  ~BluetoothSystemFactory() override;

  // mojom::BluetoothSystemFactory
  void Create(
      mojo::PendingReceiver<mojom::BluetoothSystem> system_receiver,
      mojo::PendingRemote<mojom::BluetoothSystemClient> system_client) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothSystemFactory);
};

}  // namespace device

#endif  // SERVICES_DEVICE_BLUETOOTH_BLUETOOTH_SYSTEM_FACTORY_H_
