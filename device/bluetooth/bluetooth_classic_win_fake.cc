// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_classic_win_fake.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace device {
namespace win {

BluetoothClassicWrapperFake::BluetoothClassicWrapperFake()
    : last_error_(ERROR_SUCCESS) {}
BluetoothClassicWrapperFake::~BluetoothClassicWrapperFake() {}

HBLUETOOTH_RADIO_FIND BluetoothClassicWrapperFake::FindFirstRadio(
    const BLUETOOTH_FIND_RADIO_PARAMS* params) {
  if (simulated_radios_) {
    last_error_ = ERROR_SUCCESS;
    return (PVOID)simulated_radios_.get();
  }
  last_error_ = ERROR_NO_MORE_ITEMS;
  return NULL;
}

DWORD BluetoothClassicWrapperFake::GetRadioInfo(
    PBLUETOOTH_RADIO_INFO out_radio_info) {
  if (simulated_radios_) {
    *out_radio_info = simulated_radios_->radio_info;
    last_error_ = ERROR_SUCCESS;
    return last_error_;
  }
  last_error_ = ERROR_INVALID_HANDLE;
  return last_error_;
}

BOOL BluetoothClassicWrapperFake::FindRadioClose(HBLUETOOTH_RADIO_FIND handle) {
  DCHECK_EQ(handle, (PVOID)simulated_radios_.get());
  return TRUE;
}

BOOL BluetoothClassicWrapperFake::IsConnectable() {
  if (simulated_radios_) {
    last_error_ = ERROR_SUCCESS;
    return simulated_radios_->is_connectable;
  }
  last_error_ = ERROR_INVALID_HANDLE;
  return FALSE;
}

HBLUETOOTH_DEVICE_FIND BluetoothClassicWrapperFake::FindFirstDevice(
    const BLUETOOTH_DEVICE_SEARCH_PARAMS* params,
    BLUETOOTH_DEVICE_INFO* out_device_info) {
  last_error_ = ERROR_NO_MORE_ITEMS;
  return NULL;
}

BOOL BluetoothClassicWrapperFake::FindNextDevice(
    HBLUETOOTH_DEVICE_FIND handle,
    BLUETOOTH_DEVICE_INFO* out_device_info) {
  NOTIMPLEMENTED();
  return TRUE;
}

BOOL BluetoothClassicWrapperFake::FindDeviceClose(
    HBLUETOOTH_DEVICE_FIND handle) {
  return TRUE;
}

BOOL BluetoothClassicWrapperFake::EnableDiscovery(BOOL is_enable) {
  return TRUE;
}

BOOL BluetoothClassicWrapperFake::EnableIncomingConnections(BOOL is_enable) {
  return TRUE;
}

DWORD BluetoothClassicWrapperFake::LastError() {
  return last_error_;
}

bool BluetoothClassicWrapperFake::HasHandle() {
  return bool(simulated_radios_);
}

BluetoothRadio* BluetoothClassicWrapperFake::SimulateARadio(
    base::string16 name,
    BLUETOOTH_ADDRESS address) {
  BluetoothRadio* radio = new BluetoothRadio();
  radio->is_connectable = true;  // set it connectable by default.
  size_t length =
      ((name.size() > BLUETOOTH_MAX_NAME_SIZE) ? BLUETOOTH_MAX_NAME_SIZE
                                               : name.size());
  wcsncpy(radio->radio_info.szName, base::as_wcstr(name), length);
  radio->radio_info.address = address;
  simulated_radios_.reset(radio);
  return radio;
}
}  // namespace device
}  // namespace win
