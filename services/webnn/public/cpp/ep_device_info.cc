// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/ep_device_info.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "services/webnn/public/cpp/webnn_device_util.h"

namespace webnn {

bool IsValidEpTargetArchitecture(std::string_view value) {
  return !value.empty() && std::ranges::all_of(value, [](char c) {
    return base::IsAsciiAlphaNumeric(c) || c == '_' || c == '-' || c == '.';
  });
}

std::string EpDeviceInfo::ToSwitchValue() const {
  std::string value =
      base::StringPrintf("%s,%s,%04x,%04x", ep_name,
                         DeviceTypeToString(device_type), device_id, vendor_id);
  if (!target_architecture.empty()) {
    CHECK(IsValidEpTargetArchitecture(target_architecture));
    base::StrAppend(&value, {",", target_architecture});
  }
  return value;
}

// static
EpDeviceInfo EpDeviceInfo::FromSwitchValue(std::string_view value) {
  // This parses a switch the browser process wrote from ToSwitchValue(), so a
  // malformed value is a bug on that side rather than untrusted input. CHECKing
  // each field here names the one that went wrong in the crash dump.
  std::vector<std::string_view> parts = base::SplitStringPiece(
      value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  // The target architecture is optional and therefore may be absent.
  CHECK(parts.size() == 4 || parts.size() == 5) << "malformed value: " << value;

  std::optional<mojom::Device> device_type = DeviceTypeFromString(parts[1]);
  CHECK(device_type.has_value()) << "bad device type: " << parts[1];

  uint32_t device_id = 0;
  CHECK(base::HexStringToUInt(parts[2], &device_id))
      << "bad device id: " << parts[2];

  uint32_t vendor_id = 0;
  CHECK(base::HexStringToUInt(parts[3], &vendor_id))
      << "bad vendor id: " << parts[3];

  std::string target_architecture;
  if (parts.size() == 5) {
    CHECK(IsValidEpTargetArchitecture(parts[4]))
        << "bad target architecture: " << parts[4];
    target_architecture = std::string(parts[4]);
  }

  return EpDeviceInfo{.ep_name = std::string(parts[0]),
                      .device_type = *device_type,
                      .device_id = device_id,
                      .vendor_id = vendor_id,
                      .target_architecture = std::move(target_architecture)};
}

}  // namespace webnn
