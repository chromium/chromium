// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/utils/setupdi_utils_win.h"

#include <ostream>
#include <string_view>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/win_util.h"

namespace device {

namespace {

std::ostream& operator<<(std::ostream& os, const DEVPROPKEY& value) {
  os << "{" << base::win::WStringFromGUID(value.fmtid) << ", " << value.pid
     << "}";
  return os;
}

}  // namespace

std::optional<std::wstring> GetDeviceStringProperty(
    HDEVINFO dev_info,
    SP_DEVINFO_DATA* dev_info_data,
    const DEVPROPKEY& property,
    device_event_log::LogType log_type) {
  DEVPROPTYPE property_type;
  DWORD required_size;
  if (SetupDiGetDeviceProperty(dev_info, dev_info_data, &property,
                               &property_type, /*PropertyBuffer=*/nullptr,
                               /*PropertyBufferSize=*/0, &required_size,
                               /*Flags=*/0)) {
    DEVICE_LOG(log_type, device_event_log::LOG_LEVEL_ERROR)
        << "SetupDiGetDeviceProperty(" << property
        << ") unexpectedly succeeded";
    return std::nullopt;
  }

  if (GetLastError() == ERROR_NOT_FOUND) {
    return std::nullopt;
  }

  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    DEVICE_PLOG(log_type, device_event_log::LOG_LEVEL_ERROR)
        << "SetupDiGetDeviceProperty(" << property << ") failed";
    return std::nullopt;
  }

  if (property_type != DEVPROP_TYPE_STRING) {
    DEVICE_LOG(log_type, device_event_log::LOG_LEVEL_ERROR)
        << "SetupDiGetDeviceProperty(" << property
        << ") returned unexpected type (" << property_type
        << " != " << DEVPROP_TYPE_STRING << ")";
    return std::nullopt;
  }

  std::wstring buffer;
  if (!SetupDiGetDeviceProperty(
          dev_info, dev_info_data, &property, &property_type,
          reinterpret_cast<PBYTE>(base::WriteInto(&buffer, required_size)),
          required_size, /*RequiredSize=*/nullptr, /*Flags=*/0)) {
    DEVICE_PLOG(log_type, device_event_log::LOG_LEVEL_ERROR)
        << "SetupDiGetDeviceProperty(" << property << ") failed";
    return std::nullopt;
  }

  return buffer;
}

std::optional<std::vector<std::wstring>> GetDeviceStringListProperty(
    HDEVINFO dev_info,
    SP_DEVINFO_DATA* dev_info_data,
    const DEVPROPKEY& property,
    device_event_log::LogType log_type) {
  DEVPROPTYPE property_type;
  DWORD required_size;
  if (SetupDiGetDeviceProperty(dev_info, dev_info_data, &property,
                               &property_type, /*PropertyBuffer=*/nullptr,
                               /*PropertyBufferSize=*/0, &required_size,
                               /*Flags=*/0)) {
    DEVICE_LOG(log_type, device_event_log::LOG_LEVEL_ERROR)
        << "SetupDiGetDeviceProperty(" << property
        << ") unexpectedly succeeded";
    return std::nullopt;
  }

  if (GetLastError() == ERROR_NOT_FOUND) {
    // Simplify callers by returning an empty list when the property isn't
    // found.
    return std::vector<std::wstring>();
  }

  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    DEVICE_PLOG(log_type, device_event_log::LOG_LEVEL_ERROR)
        << "SetupDiGetDeviceProperty(" << property << ") failed";
    return std::nullopt;
  }

  if (property_type != DEVPROP_TYPE_STRING_LIST) {
    DEVICE_LOG(log_type, device_event_log::LOG_LEVEL_ERROR)
        << "SetupDiGetDeviceProperty(" << property
        << ") returned unexpected type (" << property_type
        << " != " << DEVPROP_TYPE_STRING_LIST << ")";
    return std::nullopt;
  }

  std::wstring buffer;
  if (!SetupDiGetDeviceProperty(
          dev_info, dev_info_data, &property, &property_type,
          reinterpret_cast<PBYTE>(base::WriteInto(&buffer, required_size)),
          required_size, /*RequiredSize=*/nullptr, /*Flags=*/0)) {
    DEVICE_PLOG(log_type, device_event_log::LOG_LEVEL_ERROR)
        << "SetupDiGetDeviceProperty(" << property << ") failed";
    return std::nullopt;
  }

  // Windows string list properties use a NUL character as the delimiter.
  return base::SplitString(buffer, std::wstring_view(L"\0", 1),
                           base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
}

std::optional<uint32_t> GetDeviceUint32Property(
    HDEVINFO dev_info,
    SP_DEVINFO_DATA* dev_info_data,
    const DEVPROPKEY& property,
    device_event_log::LogType log_type) {
  DEVPROPTYPE property_type;
  uint32_t buffer;
  if (!SetupDiGetDeviceProperty(dev_info, dev_info_data, &property,
                                &property_type,
                                reinterpret_cast<PBYTE>(&buffer),
                                sizeof(buffer), /*RequiredSize=*/nullptr,
                                /*Flags=*/0)) {
    if (GetLastError() != ERROR_NOT_FOUND) {
      DEVICE_PLOG(log_type, device_event_log::LOG_LEVEL_ERROR)
          << "SetupDiGetDeviceProperty(" << property << ") failed";
    }
    return std::nullopt;
  }

  if (property_type != DEVPROP_TYPE_UINT32) {
    DEVICE_LOG(log_type, device_event_log::LOG_LEVEL_ERROR)
        << "SetupDiGetDeviceProperty(" << property
        << ") returned unexpected type (" << property_type
        << " != " << DEVPROP_TYPE_UINT32 << ")";
    return std::nullopt;
  }

  return buffer;
}

std::optional<std::string> GetDeviceGuidProperty(
    HDEVINFO dev_info,
    SP_DEVINFO_DATA* dev_info_data,
    const DEVPROPKEY& property,
    device_event_log::LogType log_type) {
  DEVPROPTYPE property_type;
  GUID buffer;
  if (!SetupDiGetDeviceProperty(dev_info, dev_info_data, &property,
                                &property_type,
                                reinterpret_cast<PBYTE>(&buffer),
                                sizeof(buffer), /*RequiredSize=*/nullptr,
                                /*Flags=*/0)) {
    if (GetLastError() != ERROR_NOT_FOUND) {
      DEVICE_PLOG(log_type, device_event_log::LOG_LEVEL_ERROR)
          << "SetupDiGetDeviceProperty(" << property << ") failed";
    }
    return std::nullopt;
  }

  if (property_type != DEVPROP_TYPE_GUID) {
    DEVICE_LOG(log_type, device_event_log::LOG_LEVEL_ERROR)
        << "SetupDiGetDeviceProperty(" << property
        << ") returned unexpected type (" << property_type
        << " != " << DEVPROP_TYPE_GUID << ")";
    return std::nullopt;
  }

  return base::SysWideToUTF8(base::win::WStringFromGUID(buffer));
}

}  // namespace device
