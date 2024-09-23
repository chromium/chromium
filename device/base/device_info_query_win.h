// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BASE_DEVICE_INFO_QUERY_WIN_H_
#define DEVICE_BASE_DEVICE_INFO_QUERY_WIN_H_

#include <windows.h>

#include <setupapi.h>

#include <string>

#include "device/base/device_base_export.h"

namespace device {

// Wraps HDEVINFO and SP_DEVINFO_DATA into a class that can automatically
// release them. Provides interfaces that can add a device using its
// device path, get device info and get device string property.
class DEVICE_BASE_EXPORT DeviceInfoQueryWin {
 public:
  DeviceInfoQueryWin();

  DeviceInfoQueryWin(const DeviceInfoQueryWin&) = delete;
  DeviceInfoQueryWin& operator=(const DeviceInfoQueryWin&) = delete;

  ~DeviceInfoQueryWin();

  // Add a device to |device_info_list_| using its |device_path| so that
  // its device info can be retrieved.
  bool AddDevice(const std::wstring& device_path);
  // Get the device info and store it into |device_info_data_|, this function
  // should be called at most once.
  bool GetDeviceInfo();
  // Get device string property and store it into |property_buffer|.
  bool GetDeviceStringProperty(const DEVPROPKEY& property,
                               std::string* property_buffer);

  bool device_info_list_valid() {
    return device_info_list_ != INVALID_HANDLE_VALUE;
  }

 private:
  HDEVINFO device_info_list_ = INVALID_HANDLE_VALUE;
  // When device_info_data_.cbSize != 0, |device_info_data_| is valid.
  SP_DEVINFO_DATA device_info_data_;
};

}  // namespace device

#endif  // DEVICE_BASE_DEVICE_INFO_QUERY_WIN_H_
