// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENVR_OPENVR_GAMEPAD_HELPER_H_
#define DEVICE_VR_OPENVR_OPENVR_GAMEPAD_HELPER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openvr/src/headers/openvr.h"

namespace device {

struct OpenVRInputSourceData {
  OpenVRInputSourceData();
  ~OpenVRInputSourceData();
  OpenVRInputSourceData(const OpenVRInputSourceData& other);
  base::Optional<Gamepad> gamepad;
  std::vector<std::string> profiles;
};

class OpenVRGamepadHelper {
 public:
  static OpenVRInputSourceData GetXRInputSourceData(
      vr::IVRSystem* system,
      uint32_t controller_id,
      vr::VRControllerState_t controller_state,
      mojom::XRHandedness handedness);
};

}  // namespace device
#endif  // DEVICE_VR_OPENVR_OPENVR_GAMEPAD_HELPER_H_
