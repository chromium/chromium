// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/webnn_device_util.h"

namespace webnn {

namespace {

constexpr std::string_view kDeviceTypeCpu = "CPU";
constexpr std::string_view kDeviceTypeGpu = "GPU";
constexpr std::string_view kDeviceTypeNpu = "NPU";

}  // namespace

std::string_view DeviceTypeToString(mojom::Device device_type) {
  switch (device_type) {
    case mojom::Device::kCpu:
      return kDeviceTypeCpu;
    case mojom::Device::kGpu:
      return kDeviceTypeGpu;
    case mojom::Device::kNpu:
      return kDeviceTypeNpu;
  }
}

std::optional<mojom::Device> DeviceTypeFromString(std::string_view value) {
  if (value == kDeviceTypeCpu) {
    return mojom::Device::kCpu;
  }
  if (value == kDeviceTypeGpu) {
    return mojom::Device::kGpu;
  }
  if (value == kDeviceTypeNpu) {
    return mojom::Device::kNpu;
  }
  return std::nullopt;
}

}  // namespace webnn
