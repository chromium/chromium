// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_PUBLIC_MOJOM_TEST_CONTROLLER_FRAME_DATA_H_
#define DEVICE_VR_PUBLIC_MOJOM_TEST_CONTROLLER_FRAME_DATA_H_

#include <optional>

#include "base/component_export.h"
#include "device/gamepad/public/mojom/gamepad.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom-shared.h"
#include "device/vr/public/mojom/xr_hand_tracking_data.mojom.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

inline constexpr uint32_t kMaxControllers = 5;

// Overall struct for all data that may be sent up for a controller per frame.
struct COMPONENT_EXPORT(VR_PUBLIC_TEST_TYPEMAPS) ControllerFrameData {
  device::mojom::XRHandedness handedness = device::mojom::XRHandedness::NONE;
  device::mojom::GamepadPtr gamepad;
  std::optional<gfx::Transform> pose_data;
  device::mojom::XRHandTrackingDataPtr hand_data;
  bool is_valid = false;

  ControllerFrameData();
  ~ControllerFrameData();
  ControllerFrameData(const ControllerFrameData& other);
  ControllerFrameData(ControllerFrameData&& other);
  ControllerFrameData& operator=(const ControllerFrameData& other);
  ControllerFrameData& operator=(ControllerFrameData&& other);
};

}  // namespace device

#endif  // DEVICE_VR_PUBLIC_MOJOM_TEST_CONTROLLER_FRAME_DATA_H_
