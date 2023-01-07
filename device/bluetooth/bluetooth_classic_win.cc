// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_classic_win.h"

#include "base/threading/scoped_thread_priority.h"

namespace device {
namespace win {

BluetoothClassicWrapper::BluetoothClassicWrapper() {}
BluetoothClassicWrapper::~BluetoothClassicWrapper() {}

HBLUETOOTH_RADIO_FIND BluetoothClassicWrapper::FindFirstRadio(
    const BLUETOOTH_FIND_RADIO_PARAMS* params) {
  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868). There is evidence from slow reports that this
  // method can acquire the loader lock each time it's invoked.
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();

  HANDLE radio_handle = INVALID_HANDLE_VALUE;
  HBLUETOOTH_RADIO_FIND radio_find_handle =
      BluetoothFindFirstRadio(params, &radio_handle);
  if (radio_find_handle) {
    DCHECK_NE(radio_handle, INVALID_HANDLE_VALUE);
    opened_radio_handle_.Set(radio_handle);
  }
  return radio_find_handle;
}

DWORD BluetoothClassicWrapper::GetRadioInfo(
    PBLUETOOTH_RADIO_INFO out_radio_info) {
  DCHECK(opened_radio_handle_.IsValid());
  return BluetoothGetRadioInfo(opened_radio_handle_.Get(), out_radio_info);
}

BOOL BluetoothClassicWrapper::FindRadioClose(HBLUETOOTH_RADIO_FIND handle) {
  return BluetoothFindRadioClose(handle);
}

BOOL BluetoothClassicWrapper::IsConnectable() {
  DCHECK(opened_radio_handle_.IsValid());
  return BluetoothIsConnectable(opened_radio_handle_.Get());
}

HBLUETOOTH_DEVICE_FIND BluetoothClassicWrapper::FindFirstDevice(
    const BLUETOOTH_DEVICE_SEARCH_PARAMS* params,
    BLUETOOTH_DEVICE_INFO* out_device_info) {
  return BluetoothFindFirstDevice(params, out_device_info);
}

BOOL BluetoothClassicWrapper::FindNextDevice(
    HBLUETOOTH_DEVICE_FIND handle,
    BLUETOOTH_DEVICE_INFO* out_device_info) {
  return BluetoothFindNextDevice(handle, out_device_info);
}

BOOL BluetoothClassicWrapper::FindDeviceClose(HBLUETOOTH_DEVICE_FIND handle) {
  return BluetoothFindDeviceClose(handle);
}

BOOL BluetoothClassicWrapper::EnableDiscovery(BOOL is_enable) {
  DCHECK(opened_radio_handle_.IsValid());
  return BluetoothEnableDiscovery(opened_radio_handle_.Get(), is_enable);
}

BOOL BluetoothClassicWrapper::EnableIncomingConnections(BOOL is_enable) {
  DCHECK(opened_radio_handle_.IsValid());
  return BluetoothEnableIncomingConnections(opened_radio_handle_.Get(),
                                            is_enable);
}

DWORD BluetoothClassicWrapper::LastError() {
  return GetLastError();
}

bool BluetoothClassicWrapper::HasHandle() {
  return opened_radio_handle_.IsValid();
}

}  // namespace win
}  // namespace device
