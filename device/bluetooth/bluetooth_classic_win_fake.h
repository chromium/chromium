// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_WIN_FAKE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_WIN_FAKE_H_

#include <memory>
#include <string>

#include "device/bluetooth/bluetooth_classic_win.h"

namespace device {
namespace win {

struct BluetoothRadio {
  BLUETOOTH_RADIO_INFO radio_info;
  BOOL is_connectable;
};

// Fake implementation of BluetoothClassicWrapper. Used for BluetoothTestWin.
class BluetoothClassicWrapperFake : public BluetoothClassicWrapper {
 public:
  BluetoothClassicWrapperFake();
  ~BluetoothClassicWrapperFake() override;

  HBLUETOOTH_RADIO_FIND FindFirstRadio(
      const BLUETOOTH_FIND_RADIO_PARAMS* params) override;
  DWORD GetRadioInfo(PBLUETOOTH_RADIO_INFO out_radio_info) override;
  BOOL FindRadioClose(HBLUETOOTH_RADIO_FIND handle) override;
  BOOL IsConnectable() override;
  HBLUETOOTH_DEVICE_FIND FindFirstDevice(
      const BLUETOOTH_DEVICE_SEARCH_PARAMS* params,
      BLUETOOTH_DEVICE_INFO* out_device_info) override;
  BOOL FindNextDevice(HBLUETOOTH_DEVICE_FIND handle,
                      BLUETOOTH_DEVICE_INFO* out_device_info) override;
  BOOL FindDeviceClose(HBLUETOOTH_DEVICE_FIND handle) override;
  BOOL EnableDiscovery(BOOL is_enable) override;
  BOOL EnableIncomingConnections(BOOL is_enable) override;
  DWORD LastError() override;
  bool HasHandle() override;

  BluetoothRadio* SimulateARadio(std::u16string name,
                                 BLUETOOTH_ADDRESS address);

 private:
  std::unique_ptr<BluetoothRadio> simulated_radios_;
  DWORD last_error_;
};

}  // namespace device
}  // namespace win

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_WIN_FAKE_H_
