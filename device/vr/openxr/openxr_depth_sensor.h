// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_DEPTH_SENSOR_H_
#define DEVICE_VR_OPENXR_OPENXR_DEPTH_SENSOR_H_

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
class OpenXrDepthSensor {
 public:
  OpenXrDepthSensor();
  virtual ~OpenXrDepthSensor();

  virtual XrResult Initialize() = 0;
  virtual mojom::XRDepthConfigPtr GetDepthConfig() = 0;

  // Updates the passed in `views` with the appropriate `XRDepthData` depending
  // on the state of the world.
  virtual void PopulateDepthData(
      XrTime frame_time,
      const std::vector<mojom::XRViewPtr>& views) = 0;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_DEPTH_SENSOR_H_
