// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_EP_DEVICE_INFO_H_
#define SERVICES_WEBNN_PUBLIC_CPP_EP_DEVICE_INFO_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"

namespace webnn {

// Whether `value` is safe to carry as a target architecture: it is opaque to
// WebNN and originates from an execution provider library, but it is placed on
// a command line, so its character set is limited to keep the switch parseable.
// An empty value is not valid; an empty `EpDeviceInfo::target_architecture`
// means the execution provider published none.
COMPONENT_EXPORT(WEBNN_PUBLIC_CPP_WIN)
bool IsValidEpTargetArchitecture(std::string_view value);

// Identifies an execution provider device by EP name, device type, hardware
// device ID and vendor ID. This is the C++ counterpart of
// webnn.mojom.EpDeviceInfo.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP_WIN) EpDeviceInfo {
  std::string ep_name;
  mojom::Device device_type = mojom::Device::kCpu;
  uint32_t device_id = 0;
  uint32_t vendor_id = 0;
  // The name the execution provider gave to this device's compute
  // architecture, if it published one during device enumeration. It is
  // meaningful only to that execution provider, which accepts it back through
  // `EpInfo::target_architecture_env_config_key` to compile for this device
  // without inspecting the hardware. Empty when unavailable.
  std::string target_architecture;

  auto operator<=>(const EpDeviceInfo&) const = default;
  bool operator==(const EpDeviceInfo&) const = default;

  // Serializes this info into a string. The format is
  // "<ep_name>,<device_type>,<device_id>,<vendor_id>" followed by
  // ",<target_architecture>" when one is present.
  std::string ToSwitchValue() const;

  // Parses a value produced by ToSwitchValue(). CHECKs on a malformed `value`;
  // see the definition for why that is a bug rather than an error.
  static EpDeviceInfo FromSwitchValue(std::string_view value);
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_EP_DEVICE_INFO_H_
