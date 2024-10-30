// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/vr/openxr/openxr_hand_tracker.h"

#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_hand_utils.h"
#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/xr_hand_tracking_data.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
OpenXrHandTracker::OpenXrHandTracker(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    OpenXrHandednessType type)
    : extension_helper_(extension_helper),
      session_(session),
      type_(type),
      mesh_scale_enabled_(
          extension_helper_->ExtensionEnumeration()->ExtensionSupported(
              XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME)) {
  locations_.jointCount = std::extent<decltype(joint_locations_buffer_)>::value;
  locations_.jointLocations = joint_locations_buffer_;

  // This is only used if mesh_scale_enabled_ is true, but it doesn't hurt to
  // initialize it anyway.
  // Setting `overrideHandScale` to true and `overrideValueInput` to 1 will
  // scale the hands to the size of the "standard" hand mesh per:
  // https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XrHandTrackingScaleFB
  mesh_scale_.overrideHandScale = true;
  mesh_scale_.overrideValueInput = 1.0f;
}

OpenXrHandTracker::~OpenXrHandTracker() {
  if (hand_tracker_ != XR_NULL_HANDLE) {
    extension_helper_->ExtensionMethods().xrDestroyHandTrackerEXT(
        hand_tracker_);
  }
}

XrResult OpenXrHandTracker::Update(XrSpace base_space,
                                   XrTime predicted_display_time) {
  // Lazy init hand tracking as we only need it if the app requests it.
  if (hand_tracker_ == XR_NULL_HANDLE) {
    RETURN_IF_XR_FAILED(InitializeHandTracking());
  }

  XrHandJointsLocateInfoEXT locate_info{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
  locate_info.baseSpace = base_space;
  locate_info.time = predicted_display_time;

  void** next = &locations_.next;
  if (mesh_scale_enabled_) {
    *next = &mesh_scale_;
    next = &mesh_scale_.next;
  }

  ExtendHandTrackingNextChain(next);

  XrResult result = extension_helper_->ExtensionMethods().xrLocateHandJointsEXT(
      hand_tracker_, &locate_info, &locations_);
  if (XR_FAILED(result)) {
    locations_.isActive = false;
  }

  return result;
}

mojom::XRHandTrackingDataPtr OpenXrHandTracker::GetHandTrackingData() const {
  if (!IsDataValid()) {
    return nullptr;
  }

  mojom::XRHandTrackingDataPtr hand_tracking_data =
      device::mojom::XRHandTrackingData::New();
  hand_tracking_data->hand_joint_data =
      std::vector<mojom::XRHandJointDataPtr>{};

  hand_tracking_data->hand_joint_data.reserve(kNumWebXRJoints);
  for (uint32_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++) {
    // We skip the palm joint as WebXR does not support it. All other joints are
    // supported
    if (i == XR_HAND_JOINT_PALM_EXT) {
      continue;
    }

    mojom::XRHandJointDataPtr joint_data =
        device::mojom::XRHandJointData::New();
    joint_data->joint =
        OpenXRHandJointToMojomJoint(static_cast<XrHandJointEXT>(i));
    joint_data->mojo_from_joint =
        XrPoseToGfxTransform(joint_locations_buffer_[i].pose);
    joint_data->radius = joint_locations_buffer_[i].radius;
    hand_tracking_data->hand_joint_data.push_back(std::move(joint_data));
  }

  if (!mesh_scale_enabled_ &&
      !AnonymizeHand({hand_tracking_data->hand_joint_data.data(),
                      hand_tracking_data->hand_joint_data.size()})) {
    return nullptr;
  }

  return hand_tracking_data;
}

const OpenXrHandController* OpenXrHandTracker::controller() const {
  return nullptr;
}

XrResult OpenXrHandTracker::InitializeHandTracking() {
  XrHandTrackerCreateInfoEXT create_info{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
  create_info.hand = type_ == OpenXrHandednessType::kRight ? XR_HAND_RIGHT_EXT
                                                           : XR_HAND_LEFT_EXT;
  create_info.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
  return extension_helper_->ExtensionMethods().xrCreateHandTrackerEXT(
      session_, &create_info, &hand_tracker_);
}

bool OpenXrHandTracker::IsDataValid() const {
  return hand_tracker_ != XR_NULL_HANDLE && locations_.isActive;
}

std::optional<gfx::Transform> OpenXrHandTracker::GetBaseFromPalmTransform()
    const {
  if (!IsDataValid()) {
    return std::nullopt;
  }

  return XrPoseToGfxTransform(
      joint_locations_buffer_[XR_HAND_JOINT_PALM_EXT].pose);
}

OpenXrHandTrackerFactory::OpenXrHandTrackerFactory() = default;
OpenXrHandTrackerFactory::~OpenXrHandTrackerFactory() = default;

const base::flat_set<std::string_view>&
OpenXrHandTrackerFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_EXT_HAND_TRACKING_EXTENSION_NAME,
       XR_EXT_HAND_INTERACTION_EXTENSION_NAME,
       XR_MSFT_HAND_INTERACTION_EXTENSION_NAME,
       XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrHandTrackerFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::HAND_INPUT};
}

bool OpenXrHandTrackerFactory::IsEnabled(
    const OpenXrExtensionEnumeration* extension_enum) const {
  // We can support the hand tracker if the basic hand tracking extension is
  // supported and at least one of our other required extensions is supported.
  return extension_enum->ExtensionSupported(
             XR_EXT_HAND_TRACKING_EXTENSION_NAME) &&
         base::ranges::any_of(
             GetRequestedExtensions(),
             [&extension_enum](std::string_view extension) {
               return strcmp(extension.data(),
                             XR_EXT_HAND_TRACKING_EXTENSION_NAME) != 0 &&
                      extension_enum->ExtensionSupported(extension.data());
             });
}

std::unique_ptr<OpenXrHandTracker> OpenXrHandTrackerFactory::CreateHandTracker(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    OpenXrHandednessType type) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrHandTracker>(extension_helper, session, type);
  }

  return nullptr;
}

}  // namespace device
