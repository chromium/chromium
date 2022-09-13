// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_WIN_H_

#include "base/win/scoped_handle.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_init_win.h"

namespace device {
namespace win {

// Wraps Windows APIs used to access local Bluetooth radios and classic
// Bluetooth devices, providing an interface that can be replaced with fakes in
// tests.
class DEVICE_BLUETOOTH_EXPORT BluetoothClassicWrapper {
 public:
  BluetoothClassicWrapper();
  virtual ~BluetoothClassicWrapper();

  virtual HBLUETOOTH_RADIO_FIND FindFirstRadio(
      const BLUETOOTH_FIND_RADIO_PARAMS* params);
  virtual DWORD GetRadioInfo(PBLUETOOTH_RADIO_INFO out_radio_info);
  virtual BOOL FindRadioClose(HBLUETOOTH_RADIO_FIND handle);
  virtual BOOL IsConnectable();
  virtual HBLUETOOTH_DEVICE_FIND FindFirstDevice(
      const BLUETOOTH_DEVICE_SEARCH_PARAMS* params,
      BLUETOOTH_DEVICE_INFO* out_device_info);
  virtual BOOL FindNextDevice(HBLUETOOTH_DEVICE_FIND handle,
                              BLUETOOTH_DEVICE_INFO* out_device_info);
  virtual BOOL FindDeviceClose(HBLUETOOTH_DEVICE_FIND handle);
  virtual BOOL EnableDiscovery(BOOL is_enable);
  virtual BOOL EnableIncomingConnections(BOOL is_enable);
  virtual DWORD LastError();
  virtual bool HasHandle();

 private:
  base::win::ScopedHandle opened_radio_handle_;
};

}  // namespace win
}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_CLASSIC_WIN_H_
