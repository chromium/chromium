// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_UTILS_SETUPDI_UTILS_WIN_H_
#define SERVICES_DEVICE_UTILS_SETUPDI_UTILS_WIN_H_

#include <windows.h>

#include <devpropdef.h>
#include <setupapi.h>
#include <stdint.h>

// LogSeverity is both a macro in setupapi.h and an enum in absl, which is used
// indirectly via //base.
#undef LogSeverity

#include <optional>
#include <string>
#include <vector>

#include "components/device_event_log/device_event_log.h"

namespace device {

// The functions below look up device properties using the SetupDi API. They
// make RPCs to the device manager which may block. Failures are logged to the
// device event log under `log_type`, except for a property which is not
// present since callers are expected to handle a missing property.

// Returns the value of `property` for the device described by
// `dev_info_data`, or nullopt if the property is not present or does not have
// type DEVPROP_TYPE_STRING.
std::optional<std::wstring> GetDeviceStringProperty(
    HDEVINFO dev_info,
    SP_DEVINFO_DATA* dev_info_data,
    const DEVPROPKEY& property,
    device_event_log::LogType log_type);

// Returns the value of `property` for the device described by
// `dev_info_data`, or an empty vector if the property is not present. Returns
// nullopt if the property does not have type DEVPROP_TYPE_STRING_LIST or an
// error occurs.
std::optional<std::vector<std::wstring>> GetDeviceStringListProperty(
    HDEVINFO dev_info,
    SP_DEVINFO_DATA* dev_info_data,
    const DEVPROPKEY& property,
    device_event_log::LogType log_type);

// Returns the value of `property` for the device described by
// `dev_info_data`, or nullopt if the property is not present or does not have
// type DEVPROP_TYPE_UINT32.
std::optional<uint32_t> GetDeviceUint32Property(
    HDEVINFO dev_info,
    SP_DEVINFO_DATA* dev_info_data,
    const DEVPROPKEY& property,
    device_event_log::LogType log_type);

// Returns the value of `property` for the device described by
// `dev_info_data` formatted as a string, or nullopt if the property is not
// present or does not have type DEVPROP_TYPE_GUID.
std::optional<std::string> GetDeviceGuidProperty(
    HDEVINFO dev_info,
    SP_DEVINFO_DATA* dev_info_data,
    const DEVPROPKEY& property,
    device_event_log::LogType log_type);

}  // namespace device

#endif  // SERVICES_DEVICE_UTILS_SETUPDI_UTILS_WIN_H_
