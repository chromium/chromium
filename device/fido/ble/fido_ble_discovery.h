// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_BLE_FIDO_BLE_DISCOVERY_H_
#define DEVICE_FIDO_BLE_FIDO_BLE_DISCOVERY_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "device/fido/ble/fido_ble_discovery_base.h"

namespace device {

class BluetoothDevice;
class BluetoothUUID;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleDiscovery
    : public FidoBleDiscoveryBase {
 public:
  FidoBleDiscovery();
  ~FidoBleDiscovery() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(FidoBleDiscoveryTest,
                           DiscoveryNotifiesObserverWhenDeviceInPairingMode);
  FRIEND_TEST_ALL_PREFIXES(FidoBleDiscoveryTest,
                           DiscoveryNotifiesObserverWhenDeviceInNonPairingMode);

  enum class PairingModeChangeType {
    kUnobserved,
    kObserved,
  };

  static const BluetoothUUID& FidoServiceUUID();

  // FidoBleDiscoveryBase:
  void OnSetPowered() override;

  // BluetoothAdapter::Observer:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void AdapterPoweredChanged(BluetoothAdapter* adapter, bool powered) override;
  void DeviceAddressChanged(BluetoothAdapter* adapter,
                            BluetoothDevice* device,
                            const std::string& old_address) override;

  // Returns true if |device| is a Cable device. If so, add address of |device|
  // to |blacklisted_cable_device_addresses_|.
  bool CheckForExcludedDeviceAndCacheAddress(const BluetoothDevice* device);

  void CheckAndRecordDevicePairingModeOnDiscovery(std::string authenticator_id);

  // If |device_id| does not exist in |pairing_mode_device_tracker_|, add
  // |device_id| to the map and start a timer. If the map element already
  // exists, restart the timer.
  void RecordDevicePairingStatus(std::string device_id,
                                 PairingModeChangeType type);
  void RemoveDeviceFromPairingTracker(const std::string& device_id);

  std::set<std::string> excluded_cable_device_addresses_;

  // Maps Bluetooth FIDO authenticators that are known to be in pairing mode.
  std::map<std::string, std::unique_ptr<base::OneShotTimer>>
      pairing_mode_device_tracker_;
  base::WeakPtrFactory<FidoBleDiscovery> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FidoBleDiscovery);
};

}  // namespace device

#endif  // DEVICE_FIDO_BLE_FIDO_BLE_DISCOVERY_H_
