// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_BLUETOOTH_BLUETOOTH_SYSTEM_H_
#define SERVICES_DEVICE_BLUETOOTH_BLUETOOTH_SYSTEM_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "dbus/object_path.h"
#include "device/bluetooth/dbus/bluetooth_adapter_client.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace bluez {
class BluetoothAdapterClient;
}

namespace device {

class BluetoothSystem : public mojom::BluetoothSystem,
                        public bluez::BluetoothAdapterClient::Observer {
 public:
  static void Create(mojom::BluetoothSystemRequest request,
                     mojom::BluetoothSystemClientPtr client);

  explicit BluetoothSystem(mojom::BluetoothSystemClientPtr client);
  ~BluetoothSystem() override;

  // bluez::BluetoothAdapterClient::Observer
  void AdapterAdded(const dbus::ObjectPath& object_path) override;
  void AdapterRemoved(const dbus::ObjectPath& object_path) override;
  void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                              const std::string& property_name) override;

  // mojom::BluetoothSystem
  void GetState(GetStateCallback callback) override;
  void SetPowered(bool powered, SetPoweredCallback callback) override;
  void GetScanState(GetScanStateCallback callback) override;

 private:
  bluez::BluetoothAdapterClient* GetBluetoothAdapterClient();

  void UpdateStateAndNotifyIfNecessary();

  ScanState GetScanStateFromActiveAdapter();

  void OnSetPoweredFinished(SetPoweredCallback callback, bool succeeded);

  mojom::BluetoothSystemClientPtr client_ptr_;

  // The ObjectPath of the adapter being used. Updated as BT adapters are
  // added and removed. nullopt if there is no adapter.
  base::Optional<dbus::ObjectPath> active_adapter_;

  // State of |active_adapter_| or kUnavailable if there is no
  // |active_adapter_|.
  State state_ = State::kUnavailable;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothSystem> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothSystem);
};

}  // namespace device

#endif  // SERVICES_DEVICE_BLUETOOTH_BLUETOOTH_SYSTEM_H_
