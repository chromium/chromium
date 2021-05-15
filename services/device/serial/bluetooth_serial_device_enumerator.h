// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_BLUETOOTH_SERIAL_DEVICE_ENUMERATOR_H_
#define SERVICES_DEVICE_SERIAL_BLUETOOTH_SERIAL_DEVICE_ENUMERATOR_H_

#include <map>

#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "services/device/public/mojom/serial.mojom-forward.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace device {

class BluetoothSerialDeviceEnumerator : public BluetoothAdapter::Observer,
                                        public SerialDeviceEnumerator {
 public:
  BluetoothSerialDeviceEnumerator();
  BluetoothSerialDeviceEnumerator(const BluetoothSerialDeviceEnumerator&) =
      delete;
  BluetoothSerialDeviceEnumerator& operator=(
      const BluetoothSerialDeviceEnumerator&) = delete;
  ~BluetoothSerialDeviceEnumerator() override;

  // BluetoothAdapter::Observer methods:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;

  scoped_refptr<BluetoothAdapter> GetAdapter();

  // This method will search the map of Bluetooth ports and find the
  // address with the matching token.
  absl::optional<std::string> GetAddressFromToken(
      const base::UnguessableToken& token);

 protected:
  scoped_refptr<BluetoothAdapter> adapter_;

 private:
  void OnGotClassicAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  std::unordered_map<std::string, base::UnguessableToken> bluetooth_ports_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_BLUETOOTH_SERIAL_DEVICE_ENUMERATOR_H_
