// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_hand_tracker_meta.h"

#include <optional>

#include "base/memory/raw_ref.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

OpenXrHandTrackerMeta::OpenXrHandTrackerMeta(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    OpenXrHandednessType type)
    : OpenXrHandTracker(extension_helper, session, type) {}

OpenXrHandTrackerMeta::~OpenXrHandTrackerMeta() = default;

const OpenXrHandController* OpenXrHandTrackerMeta::controller() const {
  return this;
}

mojom::OpenXrInteractionProfileType OpenXrHandTrackerMeta::interaction_profile()
    const {
  return mojom::OpenXrInteractionProfileType::kMetaHandAim;
}

GamepadMapping OpenXrHandTrackerMeta::gamepad_mapping() const {
  return GamepadMapping::kNone;
}

std::optional<gfx::Transform> OpenXrHandTrackerMeta::GetBaseFromGripTransform()
    const {
  // We will treat the palm as our grip.
  return GetBaseFromPalmTransform();
}

std::optional<gfx::Transform>
OpenXrHandTrackerMeta::GetGripFromPointerTransform() const {
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

std::optional<GamepadButton> OpenXrHandTrackerMeta::GetButton(
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

void OpenXrHandTrackerMeta::AppendToLocationStruct(
    XrHandJointLocationsEXT& locations) {
  locations.next = &aim_state_;
}

}  // namespace device
