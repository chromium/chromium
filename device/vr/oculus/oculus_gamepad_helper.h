// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OCULUS_OCULUS_GAMEPAD_HELPER_H_
#define DEVICE_VR_OCULUS_OCULUS_GAMEPAD_HELPER_H_

#include "base/optional.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"

namespace device {

class OculusGamepadHelper {
 public:
  static base::Optional<Gamepad> CreateGamepad(ovrSession session,
                                               ovrHandType hand);
};

}  // namespace device

#endif  // DEVICE_VR_OCULUS_OCULUS_GAMEPAD_HELPER_H_
