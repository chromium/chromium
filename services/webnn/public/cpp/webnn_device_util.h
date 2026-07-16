// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_WEBNN_DEVICE_UTIL_H_
#define SERVICES_WEBNN_PUBLIC_CPP_WEBNN_DEVICE_UTIL_H_

#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"

namespace webnn {

// Returns a human-readable name ("CPU", "GPU" or "NPU") for `device_type`.
COMPONENT_EXPORT(WEBNN_PUBLIC_CPP_WIN)
std::string_view DeviceTypeToString(mojom::Device device_type);

// Inverse of DeviceTypeToString(). Returns std::nullopt if `value` does not
// match a known device type.
COMPONENT_EXPORT(WEBNN_PUBLIC_CPP_WIN)
std::optional<mojom::Device> DeviceTypeFromString(std::string_view value);

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_WEBNN_DEVICE_UTIL_H_
