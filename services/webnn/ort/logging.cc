// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/logging.h"

#include <sstream>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "services/webnn/webnn_switches.h"

namespace webnn::ort {

namespace {

// Helper to convert a string to OrtLoggingLevel enum.
OrtLoggingLevel StringToOrtLoggingLevel(std::string_view logging_level) {
  if (logging_level == "VERBOSE") {
    return ORT_LOGGING_LEVEL_VERBOSE;
  } else if (logging_level == "INFO") {
    return ORT_LOGGING_LEVEL_INFO;
  } else if (logging_level == "WARNING") {
    return ORT_LOGGING_LEVEL_WARNING;
  } else if (logging_level == "ERROR") {
    return ORT_LOGGING_LEVEL_ERROR;
  } else if (logging_level == "FATAL") {
    return ORT_LOGGING_LEVEL_FATAL;
  }
  // Default to ERROR if the input is invalid.
  LOG(WARNING) << "[WebNN] Unrecognized logging level: " << logging_level
               << ". Default ERROR level will be used.";
  return ORT_LOGGING_LEVEL_ERROR;
}

std::string_view OrtHardwareDeviceTypeToString(
    OrtHardwareDeviceType device_type) {
  switch (device_type) {
    case OrtHardwareDeviceType_CPU:
      return "CPU";
    case OrtHardwareDeviceType_GPU:
      return "GPU";
    case OrtHardwareDeviceType_NPU:
      return "NPU";
  }
}

std::string OrtKeyValuePairsToString(
    const OrtApi* ort_api,
    const OrtKeyValuePairs* ort_key_value_pairs) {
  size_t num_entries = 0;
  const char* const* keys = nullptr;
  const char* const* values = nullptr;
  ort_api->GetKeyValuePairs(ort_key_value_pairs, &keys, &values, &num_entries);
  std::string result = "{";
  for (size_t i = 0; i < num_entries; ++i) {
    result = base::StrCat(
        {result,
         // SAFETY: ORT guarantees that `keys[i]` is valid and null-terminated.
         UNSAFE_BUFFERS(base::cstring_view(keys[i])), ": ",
         // SAFETY: ORT guarantees that `values[i]` is valid and
         // null-terminated.
         UNSAFE_BUFFERS(base::cstring_view(values[i])),
         i != num_entries - 1 ? ", " : ""});
  }
  return base::StrCat({result, "}"});
}

std::string Uint32ToHexString(uint32_t value) {
  std::stringstream ss;
  ss << "0x" << std::hex << value;
  return ss.str();
}

}  // namespace

OrtLoggingLevel GetOrtLoggingLevel() {
  OrtLoggingLevel ort_logging_level = ORT_LOGGING_LEVEL_ERROR;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebNNOrtLoggingLevel)) {
    std::string user_logging_level =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kWebNNOrtLoggingLevel);
    ort_logging_level = StringToOrtLoggingLevel(user_logging_level);
  }
  return ort_logging_level;
}

void LogEpDevices(const OrtApi* ort_api,
                  base::span<const OrtEpDevice* const> ep_devices,
                  std::string_view prefix) {
  int i = 0;
  for (const auto* ep_device : ep_devices) {
    CHECK(ep_device);
    std::string ep_device_info = base::StrCat(
        {"[WebNN] [INFO] ", prefix, " #", base::NumberToString(i++),
         ": {ep_name: ",
         // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
         UNSAFE_BUFFERS(
             base::cstring_view(ort_api->EpDevice_EpName(ep_device))),
         ", ep_vendor: ",
         // SAFETY: ORT guarantees that `ep_vendor` is valid and
         // null-terminated.
         UNSAFE_BUFFERS(
             base::cstring_view(ort_api->EpDevice_EpVendor(ep_device)))});

    const OrtKeyValuePairs* ep_metadata =
        ort_api->EpDevice_EpMetadata(ep_device);
    CHECK(ep_metadata);
    base::StrAppend(
        &ep_device_info,
        {", ep_metadata: ", OrtKeyValuePairsToString(ort_api, ep_metadata)});

    const OrtKeyValuePairs* ep_options = ort_api->EpDevice_EpOptions(ep_device);
    CHECK(ep_options);
    base::StrAppend(
        &ep_device_info,
        {", ep_options: ", OrtKeyValuePairsToString(ort_api, ep_options), "}"});

    const OrtHardwareDevice* hardware_device =
        ort_api->EpDevice_Device(ep_device);
    CHECK(hardware_device);
    base::StrAppend(
        &ep_device_info,
        {", OrtHardwareDevice: {type: ",
         OrtHardwareDeviceTypeToString(
             ort_api->HardwareDevice_Type(hardware_device)),
         ", vendor: ",
         // SAFETY: ORT guarantees that `OrtHardwareDevice::vendor` is valid and
         // null-terminated.
         UNSAFE_BUFFERS(base::cstring_view(
             ort_api->HardwareDevice_Vendor(hardware_device))),
         ", vendor_id: ",
         Uint32ToHexString(ort_api->HardwareDevice_VendorId(hardware_device)),
         ", device_id: ",
         Uint32ToHexString(ort_api->HardwareDevice_DeviceId(hardware_device))});
    const OrtKeyValuePairs* device_metadata =
        ort_api->HardwareDevice_Metadata(hardware_device);
    CHECK(device_metadata);
    base::StrAppend(&ep_device_info,
                    {", device_metadata: ",
                     OrtKeyValuePairsToString(ort_api, device_metadata), "}"});
    LOG(ERROR) << ep_device_info;
  }
}

}  // namespace webnn::ort
