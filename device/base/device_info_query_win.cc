// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/base/device_info_query_win.h"

#include <stddef.h>
#include <string.h>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace device {

DeviceInfoQueryWin::DeviceInfoQueryWin()
    : device_info_list_(SetupDiCreateDeviceInfoList(nullptr, nullptr)) {
  memset(&device_info_data_, 0, sizeof(device_info_data_));
}

DeviceInfoQueryWin::~DeviceInfoQueryWin() {
  if (device_info_list_valid()) {
    // Release |device_info_data_| only when it is valid.
    if (device_info_data_.cbSize != 0)
      SetupDiDeleteDeviceInfo(device_info_list_, &device_info_data_);
    SetupDiDestroyDeviceInfoList(device_info_list_);
  }
}

bool DeviceInfoQueryWin::AddDevice(const std::wstring& device_path) {
  return SetupDiOpenDeviceInterface(device_info_list_, device_path.c_str(), 0,
                                    nullptr) != FALSE;
}

bool DeviceInfoQueryWin::GetDeviceInfo() {
  DCHECK_EQ(0U, device_info_data_.cbSize);
  device_info_data_.cbSize = sizeof(device_info_data_);
  if (!SetupDiEnumDeviceInfo(device_info_list_, 0, &device_info_data_)) {
    // Clear cbSize to maintain the invariant.
    device_info_data_.cbSize = 0;
    return false;
  }
  return true;
}

bool DeviceInfoQueryWin::GetDeviceStringProperty(const DEVPROPKEY& property,
                                                 std::string* property_buffer) {
  DEVPROPTYPE property_type;
  DWORD required_size;
  if (SetupDiGetDeviceProperty(device_info_list_, &device_info_data_, &property,
                               &property_type, nullptr, 0, &required_size, 0) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
      property_type != DEVPROP_TYPE_STRING) {
    return false;
  }

  std::u16string buffer;
  if (!SetupDiGetDeviceProperty(
          device_info_list_, &device_info_data_, &property, &property_type,
          reinterpret_cast<PBYTE>(base::WriteInto(&buffer, required_size)),
          required_size, nullptr, 0)) {
    return false;
  }

  *property_buffer = base::UTF16ToUTF8(buffer);
  return true;
}

}  // namespace device
