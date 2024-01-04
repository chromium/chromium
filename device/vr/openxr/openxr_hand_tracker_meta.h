// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_HAND_TRACKER_META_H_
#define DEVICE_VR_OPENXR_OPENXR_HAND_TRACKER_META_H_

#include "device/vr/openxr/openxr_hand_tracker.h"

#include "base/memory/raw_ref.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrHandTrackerMeta : public OpenXrHandTracker,
                              public OpenXrHandController {
 public:
  OpenXrHandTrackerMeta(const OpenXrExtensionHelper& extension_helper,
                        XrSession session,
                        OpenXrHandednessType type);
  ~OpenXrHandTrackerMeta() override;

  const OpenXrHandController* controller() const override;

  // OpenXrHandController
  mojom::OpenXrInteractionProfileType interaction_profile() const override;
  GamepadMapping gamepad_mapping() const override;
  std::optional<gfx::Transform> GetBaseFromGripTransform() const override;
  std::optional<gfx::Transform> GetGripFromPointerTransform() const override;
  std::optional<GamepadButton> GetButton(OpenXrButtonType type) const override;

 private:
  void AppendToLocationStruct(XrHandJointLocationsEXT& locations) override;

  XrHandTrackingAimStateFB aim_state_ = {XR_TYPE_HAND_TRACKING_AIM_STATE_FB};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_HAND_TRACKER_META_H_
