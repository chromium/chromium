// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BLE_ADAPTER_MANAGER_H_
#define DEVICE_FIDO_BLE_ADAPTER_MANAGER_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/fido/fido_request_handler_base.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) BleAdapterManager
    : public BluetoothAdapter::Observer {
 public:
  // Handles notifying events from/exposing API's in BluetoothAdapter to
  // FidoRequestHandler. Namely, handles the following logic:
  //   a) Exposing API to trigger power Bluetooth adapter on/off.
  //   b) Notifying FidoRequestHandler when Bluetooth adapter power changes.
  explicit BleAdapterManager(FidoRequestHandlerBase* request_handler);

  BleAdapterManager(const BleAdapterManager&) = delete;
  BleAdapterManager& operator=(const BleAdapterManager&) = delete;

  ~BleAdapterManager() override;

  void SetAdapterPower(bool set_power_on);

  // Queries the OS for the status of the Bluetooth adapter. On macOS, this will
  // trigger a bluetooth permission prompt if Chrome has never asked before.
  void RequestBluetoothPermission(
      FidoRequestHandlerBase::BlePermissionCallback callback);

 private:
  friend class FidoBleAdapterManagerTest;

  void OnHaveBluetoothPermission(
      FidoRequestHandlerBase::BlePermissionCallback callback);

  // BluetoothAdapter::Observer:
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;

  void Start(scoped_refptr<BluetoothAdapter> adapter);

  const raw_ptr<FidoRequestHandlerBase> request_handler_;
  scoped_refptr<BluetoothAdapter> adapter_;

  base::WeakPtrFactory<BleAdapterManager> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_BLE_ADAPTER_MANAGER_H_
