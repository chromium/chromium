// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BLE_FIDO_BLE_DISCOVERY_BASE_H_
#define DEVICE_FIDO_BLE_FIDO_BLE_DISCOVERY_BASE_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/fido/fido_device_discovery.h"

namespace device {

class BluetoothDiscoverySession;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleDiscoveryBase
    : public FidoDeviceDiscovery,
      public BluetoothAdapter::Observer {
 public:
  explicit FidoBleDiscoveryBase(FidoTransportProtocol transport);
  ~FidoBleDiscoveryBase() override;

 protected:
  static const BluetoothUUID& CableAdvertisementUUID();

  virtual void OnSetPowered() = 0;
  virtual void OnStartDiscoverySessionWithFilter(
      std::unique_ptr<BluetoothDiscoverySession>);

  void OnSetPoweredError();
  void OnStartDiscoverySessionError();
  void SetDiscoverySession(
      std::unique_ptr<BluetoothDiscoverySession> discovery_session);
  bool IsCableDevice(const BluetoothDevice* device) const;

  BluetoothAdapter* adapter() { return adapter_.get(); }

 private:
  void OnGetAdapter(scoped_refptr<BluetoothAdapter> adapter);

  // FidoDeviceDiscovery:
  void StartInternal() override;

  scoped_refptr<BluetoothAdapter> adapter_;
  std::unique_ptr<BluetoothDiscoverySession> discovery_session_;

  base::WeakPtrFactory<FidoBleDiscoveryBase> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoBleDiscoveryBase);
};

}  // namespace device

#endif  // DEVICE_FIDO_BLE_FIDO_BLE_DISCOVERY_BASE_H_
