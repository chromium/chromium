// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_hand_tracker.h"

#include <vector>

#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

namespace {
static constexpr mojom::XRHandJoint OpenXRHandJointToMojomJoint(
    XrHandJointEXT openxr_joint) {
  CHECK_NE(openxr_joint, XR_HAND_JOINT_PALM_EXT);
  // The OpenXR joints have palm at 0, but from that point are the same as the
  // mojom joints. Hence they are offset by 1.
  return static_cast<mojom::XRHandJoint>(openxr_joint - 1);
}

constexpr unsigned kNumWebXRJoints =
    static_cast<unsigned>(mojom::XRHandJoint::kMaxValue) + 1u;

// WebXR doesn't expose the palm joint, so there's not a corresponding mojom
// value to check, but validate which index we're skipping for it.
static_assert(XR_HAND_JOINT_PALM_EXT == 0u,
              "OpenXR palm joint expected to be the 0th joint");

// Because we do not expose the PALM joint (which is the first joint in OpenXR),
// we have one less joint than OpenXR.
static_assert(kNumWebXRJoints == XR_HAND_JOINT_COUNT_EXT - 1u);

// Enforce that the conversion is correct at compilation time.
// The mojom hand joints must match the WebXR spec. If these are ever out of
// sync, this mapping will need to be updated.
static_assert(mojom::XRHandJoint::kWrist ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_WRIST_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kThumbMetacarpal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_THUMB_METACARPAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kThumbPhalanxProximal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_THUMB_PROXIMAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kThumbPhalanxDistal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_THUMB_DISTAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kThumbTip ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_THUMB_TIP_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kIndexFingerMetacarpal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_INDEX_METACARPAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kIndexFingerPhalanxProximal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_INDEX_PROXIMAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kIndexFingerPhalanxIntermediate ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_INDEX_INTERMEDIATE_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kIndexFingerPhalanxDistal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_INDEX_DISTAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kIndexFingerTip ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_INDEX_TIP_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kMiddleFingerMetacarpal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_MIDDLE_METACARPAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kMiddleFingerPhalanxProximal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_MIDDLE_PROXIMAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kMiddleFingerPhalanxIntermediate ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_MIDDLE_INTERMEDIATE_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kMiddleFingerPhalanxDistal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_MIDDLE_DISTAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kMiddleFingerTip ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_MIDDLE_TIP_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kRingFingerMetacarpal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_RING_METACARPAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kRingFingerPhalanxProximal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_RING_PROXIMAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kRingFingerPhalanxIntermediate ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_RING_INTERMEDIATE_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kRingFingerPhalanxDistal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_RING_DISTAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kRingFingerTip ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_RING_TIP_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kPinkyFingerMetacarpal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_LITTLE_METACARPAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kPinkyFingerPhalanxProximal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_LITTLE_PROXIMAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kPinkyFingerPhalanxIntermediate ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_LITTLE_INTERMEDIATE_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kPinkyFingerPhalanxDistal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_LITTLE_DISTAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kPinkyFingerTip ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_LITTLE_TIP_EXT),
              "WebXR - OpenXR joint enum value mismatch");
}  // namespace

OpenXrHandTracker::OpenXrHandTracker(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    OpenXrHandednessType type)
    : extension_helper_(extension_helper), session_(session), type_(type) {}

OpenXrHandTracker::~OpenXrHandTracker() {
  if (hand_tracker_ != XR_NULL_HANDLE) {
    extension_helper_->ExtensionMethods().xrDestroyHandTrackerEXT(
        hand_tracker_);
  }
}

mojom::XRHandTrackingDataPtr OpenXrHandTracker::GetHandTrackingData(
    XrSpace base_space,
    XrTime predicted_display_time) {
  // Lazy init hand tracking as we only need it if the app requests it.
  if (hand_tracker_ == XR_NULL_HANDLE) {
    if (XR_FAILED(InitializeHandTracking())) {
      return nullptr;
    }
  }

  XrHandJointLocationEXT joint_locations_buffer[XR_HAND_JOINT_COUNT_EXT];
  XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
  locations.jointCount = std::extent<decltype(joint_locations_buffer)>::value;
  locations.jointLocations = joint_locations_buffer;

  XrHandJointsLocateInfoEXT locate_info{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
  locate_info.baseSpace = base_space;
  locate_info.time = predicted_display_time;

  if (XR_FAILED(extension_helper_->ExtensionMethods().xrLocateHandJointsEXT(
          hand_tracker_, &locate_info, &locations)) ||
      !locations.isActive) {
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
        XrPoseToGfxTransform(joint_locations_buffer[i].pose);
    joint_data->radius = joint_locations_buffer[i].radius;
    hand_tracking_data->hand_joint_data.push_back(std::move(joint_data));
  }

  return hand_tracking_data;
}

XrResult OpenXrHandTracker::InitializeHandTracking() {
  XrHandTrackerCreateInfoEXT create_info{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
  create_info.hand = type_ == OpenXrHandednessType::kRight ? XR_HAND_RIGHT_EXT
                                                           : XR_HAND_LEFT_EXT;
  create_info.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
  return extension_helper_->ExtensionMethods().xrCreateHandTrackerEXT(
      session_, &create_info, &hand_tracker_);
}

}  // namespace device
