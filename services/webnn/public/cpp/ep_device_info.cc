// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/ep_device_info.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "services/webnn/public/cpp/webnn_device_util.h"

namespace webnn {

std::string EpDeviceInfo::ToSwitchValue() const {
  return base::StringPrintf("%s,%s,%04x,%04x", ep_name,
                            DeviceTypeToString(device_type), device_id,
                            vendor_id);
}

// static
std::optional<EpDeviceInfo> EpDeviceInfo::FromSwitchValue(
    std::string_view value) {
  std::vector<std::string_view> parts = base::SplitStringPiece(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() != 4) {
    return std::nullopt;
  }

  std::optional<mojom::Device> device_type = DeviceTypeFromString(parts[1]);
  if (!device_type.has_value()) {
    return std::nullopt;
  }

  uint32_t device_id = 0;
  if (!base::HexStringToUInt(parts[2], &device_id)) {
    return std::nullopt;
  }

  uint32_t vendor_id = 0;
  if (!base::HexStringToUInt(parts[3], &vendor_id)) {
    return std::nullopt;
  }

  return EpDeviceInfo{.ep_name = std::string(parts[0]),
                      .device_type = *device_type,
                      .device_id = device_id,
                      .vendor_id = vendor_id};
}

}  // namespace webnn
