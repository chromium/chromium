// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BLE_ADAPTER_MANAGER_H_
#define DEVICE_FIDO_BLE_ADAPTER_MANAGER_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/fido/ble/fido_ble_pairing_delegate.h"
#include "device/fido/fido_request_handler_base.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) BleAdapterManager
    : public BluetoothAdapter::Observer {
 public:
  // Handles notifying events from/exposing API's in BluetoothAdapter to
  // FidoRequestHandler. Namely, handles the following logic:
  //   a) Exposing API to initiate Bluetooth pairing event.
  //   b) Exposing API to trigger power Bluetooth adapter on/off.
  //   c) Notifying FidoRequestHandler when Bluetooth adapter power changes.
  BleAdapterManager(FidoRequestHandlerBase* request_handler);
  ~BleAdapterManager() override;

  void SetAdapterPower(bool set_power_on);
  void InitiatePairing(std::string fido_authenticator_id,
                       base::Optional<std::string> pin_code,
                       base::OnceClosure success_callback,
                       base::OnceClosure error_callback);

 private:
  friend class FidoBleAdapterManagerTest;

  // BluetoothAdapter::Observer:
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;
  void DeviceAddressChanged(BluetoothAdapter* adapter,
                            BluetoothDevice* device,
                            const std::string& old_address) override;

  void Start(scoped_refptr<BluetoothAdapter> adapter);

  FidoRequestHandlerBase* const request_handler_;
  scoped_refptr<BluetoothAdapter> adapter_;
  FidoBlePairingDelegate pairing_delegate_;
  bool adapter_powered_on_programmatically_ = false;

  base::WeakPtrFactory<BleAdapterManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BleAdapterManager);
};

}  // namespace device

#endif  // DEVICE_FIDO_BLE_ADAPTER_MANAGER_H_
