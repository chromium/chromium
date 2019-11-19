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
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace bluez {
class BluetoothAdapterClient;
class BluetoothDeviceClient;
}

namespace device {

class BluetoothSystem : public mojom::BluetoothSystem,
                        public bluez::BluetoothAdapterClient::Observer {
 public:
  static void Create(mojo::PendingReceiver<mojom::BluetoothSystem> receiver,
                     mojo::PendingRemote<mojom::BluetoothSystemClient> client);

  explicit BluetoothSystem(
      mojo::PendingRemote<mojom::BluetoothSystemClient> client);
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
  void StartScan(StartScanCallback callback) override;
  void StopScan(StopScanCallback callback) override;
  void GetAvailableDevices(GetAvailableDevicesCallback callback) override;

 private:
  bluez::BluetoothAdapterClient* GetBluetoothAdapterClient();
  bluez::BluetoothDeviceClient* GetBluetoothDeviceClient();

  void UpdateStateAndNotifyIfNecessary();

  ScanState GetScanStateFromActiveAdapter();

  void OnSetPoweredFinished(SetPoweredCallback callback, bool succeeded);

  void OnStartDiscovery(
      StartScanCallback callback,
      const base::Optional<bluez::BluetoothAdapterClient::Error>& error);
  void OnStopDiscovery(
      StopScanCallback callback,
      const base::Optional<bluez::BluetoothAdapterClient::Error>& error);

  mojo::Remote<mojom::BluetoothSystemClient> client_;

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
