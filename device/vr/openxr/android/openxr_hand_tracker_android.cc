// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/android/openxr_hand_tracker_android.h"

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

OpenXrHandTrackerAndroid::OpenXrHandTrackerAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    OpenXrHandednessType type)
    : OpenXrHandTracker(extension_helper, session, type) {}

OpenXrHandTrackerAndroid::~OpenXrHandTrackerAndroid() = default;

const OpenXrHandController* OpenXrHandTrackerAndroid::controller() const {
  return this;
}

mojom::OpenXrInteractionProfileType
OpenXrHandTrackerAndroid::interaction_profile() const {
  return mojom::OpenXrInteractionProfileType::kAndroidHandGestures;
}

std::optional<gfx::Transform>
OpenXrHandTrackerAndroid::GetBaseFromGripTransform() const {
  // We will treat the palm as our grip.
  return GetBaseFromPalmTransform();
}

std::optional<gfx::Transform>
OpenXrHandTrackerAndroid::GetGripFromPointerTransform() const {
  if (!IsDataValid()) {
    return std::nullopt;
  }

  std::optional<gfx::Transform> maybe_base_from_grip =
      GetBaseFromGripTransform();
  CHECK(maybe_base_from_grip.has_value());

  // base_from_grip should be a rigid transform, so it's an error if it's not
  // invertible.
  auto grip_from_base = maybe_base_from_grip.value().GetCheckedInverse();

  gfx::Transform base_from_pointer = XrPoseToGfxTransform(gesture_.ray);
  return (grip_from_base * base_from_pointer);
}

std::optional<GamepadButton> OpenXrHandTrackerAndroid::GetButton(
    OpenXrButtonType type) const {
  if (!IsDataValid()) {
    return std::nullopt;
  }

  if (type == OpenXrButtonType::kTrigger) {
    bool pressed = (gesture_.gestureTypeFlags &
                    XR_HAND_GESTURE_TYPE_PINCH_PRESSED_BIT_ANDROID) != 0;
    return GamepadButton(pressed, /*touched=*/pressed, pressed ? 1.0f : 0.0f);
  }

  return std::nullopt;
}

void OpenXrHandTrackerAndroid::AppendToLocationStruct(
    XrHandJointLocationsEXT& locations) {
  locations.next = &gesture_;
}

OpenXrHandTrackerAndroidFactory::OpenXrHandTrackerAndroidFactory() = default;
OpenXrHandTrackerAndroidFactory::~OpenXrHandTrackerAndroidFactory() = default;

const base::flat_set<std::string_view>&
OpenXrHandTrackerAndroidFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_EXT_HAND_TRACKING_EXTENSION_NAME,
       XR_ANDROID_HAND_GESTURE_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrHandTrackerAndroidFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::HAND_INPUT};
}

std::unique_ptr<OpenXrHandTracker>
OpenXrHandTrackerAndroidFactory::CreateHandTracker(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    OpenXrHandednessType type) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrHandTrackerAndroid>(extension_helper, session,
                                                      type);
  }

  return nullptr;
}

}  // namespace device
