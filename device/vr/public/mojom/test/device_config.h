// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_TEST_DEVICE_CONFIG_H_
#define DEVICE_VR_PUBLIC_MOJOM_TEST_DEVICE_CONFIG_H_

#include <array>

namespace device {

struct DeviceConfig {
  float interpupillary_distance = 0.0f;
  // Both viewports are in the form of {left, right, top, bottom} FOVs.
  std::array<float, 4> viewport_left = {0.0f, 0.0f, 0.0f, 0.0f};
  std::array<float, 4> viewport_right = {0.0f, 0.0f, 0.0f, 0.0f};
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_TEST_DEVICE_CONFIG_H_
