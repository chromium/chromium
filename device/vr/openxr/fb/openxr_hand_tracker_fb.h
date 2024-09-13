// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_FB_OPENXR_HAND_TRACKER_FB_H_
#define DEVICE_VR_OPENXR_FB_OPENXR_HAND_TRACKER_FB_H_

#include "device/vr/openxr/openxr_hand_tracker.h"

#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrHandTrackerFb : public OpenXrHandTracker,
                              public OpenXrHandController {
 public:
  OpenXrHandTrackerFb(const OpenXrExtensionHelper& extension_helper,
                        XrSession session,
                        OpenXrHandednessType type);
  ~OpenXrHandTrackerFb() override;

  const OpenXrHandController* controller() const override;

  // OpenXrHandController
  mojom::OpenXrInteractionProfileType interaction_profile() const override;
  std::optional<gfx::Transform> GetBaseFromGripTransform() const override;
  std::optional<gfx::Transform> GetGripFromPointerTransform() const override;
  std::optional<GamepadButton> GetButton(OpenXrButtonType type) const override;

 private:
  void ExtendHandTrackingNextChain(void** next) override;

  XrHandTrackingAimStateFB aim_state_ = {XR_TYPE_HAND_TRACKING_AIM_STATE_FB};
};

class OpenXrHandTrackerFbFactory : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrHandTrackerFbFactory();
  ~OpenXrHandTrackerFbFactory() override;

  const base::flat_set<std::string_view>& GetRequestedExtensions()
      const override;
  std::set<device::mojom::XRSessionFeature> GetSupportedFeatures(
      const OpenXrExtensionEnumeration* extension_enum) const override;

  std::unique_ptr<OpenXrHandTracker> CreateHandTracker(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      OpenXrHandednessType type) const override;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_FB_OPENXR_HAND_TRACKER_FB_H_
