// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_TEST_DEVICE_CONFIG_H_
#define DEVICE_VR_PUBLIC_MOJOM_TEST_DEVICE_CONFIG_H_

#include <array>

namespace device {

struct DeviceConfig {
  float interpupillary_distance;
  std::array<float, 4>
      viewport_left;  // raw projection left {left, right, top, bottom}
  std::array<float, 4>
      viewport_right;  // raw projection right {left, right, top, bottom}
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_TEST_DEVICE_CONFIG_H_
