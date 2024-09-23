// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/fb/openxr_hand_tracker_fb.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

OpenXrHandTrackerFb::OpenXrHandTrackerFb(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    OpenXrHandednessType type)
    : OpenXrHandTracker(extension_helper, session, type) {}

OpenXrHandTrackerFb::~OpenXrHandTrackerFb() = default;

const OpenXrHandController* OpenXrHandTrackerFb::controller() const {
  return this;
}

mojom::OpenXrInteractionProfileType OpenXrHandTrackerFb::interaction_profile()
    const {
  return mojom::OpenXrInteractionProfileType::kMetaHandAim;
}

std::optional<gfx::Transform> OpenXrHandTrackerFb::GetBaseFromGripTransform()
    const {
  // We will treat the palm as our grip.
  return GetBaseFromPalmTransform();
}

std::optional<gfx::Transform>
OpenXrHandTrackerFb::GetGripFromPointerTransform() const {
  if (!IsDataValid()) {
    return std::nullopt;
  }

  std::optional<gfx::Transform> maybe_base_from_grip =
      GetBaseFromGripTransform();
  CHECK(maybe_base_from_grip.has_value());

  // base_from_grip should be a rigid transform, so it's an error if it's not
  // invertible.
  auto grip_from_base = maybe_base_from_grip.value().GetCheckedInverse();

  // The aimPose is in the same space as the hand was updated in, which is
  // considered the base space.
  gfx::Transform base_from_pointer = XrPoseToGfxTransform(aim_state_.aimPose);
  return (grip_from_base * base_from_pointer);
}

std::optional<GamepadButton> OpenXrHandTrackerFb::GetButton(
    OpenXrButtonType type) const {
  if (!IsDataValid()) {
    return std::nullopt;
  }

  if (type == OpenXrButtonType::kTrigger) {
    bool pressed =
        (aim_state_.status & XR_HAND_TRACKING_AIM_INDEX_PINCHING_BIT_FB) != 0;
    return GamepadButton(pressed, /*touched=*/pressed,
                         aim_state_.pinchStrengthIndex);
  }

  return std::nullopt;
}

void OpenXrHandTrackerFb::ExtendHandTrackingNextChain(void** next) {
  *next = &aim_state_;
}

OpenXrHandTrackerFbFactory::OpenXrHandTrackerFbFactory() = default;
OpenXrHandTrackerFbFactory::~OpenXrHandTrackerFbFactory() = default;

const base::flat_set<std::string_view>&
OpenXrHandTrackerFbFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_EXT_HAND_TRACKING_EXTENSION_NAME,
       XR_FB_HAND_TRACKING_AIM_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrHandTrackerFbFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::HAND_INPUT};
}

std::unique_ptr<OpenXrHandTracker>
OpenXrHandTrackerFbFactory::CreateHandTracker(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    OpenXrHandednessType type) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrHandTrackerFb>(extension_helper, session,
                                                   type);
  }

  return nullptr;
}

}  // namespace device
