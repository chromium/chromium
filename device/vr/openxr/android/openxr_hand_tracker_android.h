// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_HAND_TRACKER_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_HAND_TRACKER_ANDROID_H_

#include "device/vr/openxr/openxr_hand_tracker.h"

#include "base/memory/raw_ref.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrHandTrackerAndroid : public OpenXrHandTracker,
                                 public OpenXrHandController {
 public:
  OpenXrHandTrackerAndroid(const OpenXrExtensionHelper& extension_helper,
                           XrSession session,
                           OpenXrHandednessType type);
  ~OpenXrHandTrackerAndroid() override;

  const OpenXrHandController* controller() const override;

  // OpenXrHandController
  mojom::OpenXrInteractionProfileType interaction_profile() const override;
  GamepadMapping gamepad_mapping() const override;
  std::optional<gfx::Transform> GetBaseFromGripTransform() const override;
  std::optional<gfx::Transform> GetGripFromPointerTransform() const override;
  std::optional<GamepadButton> GetButton(OpenXrButtonType type) const override;

 private:
  void AppendToLocationStruct(XrHandJointLocationsEXT& locations) override;

  XrHandGestureANDROID gesture_ = {XR_TYPE_HAND_GESTURE_ANDROID};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_HAND_TRACKER_ANDROID_H_
