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
#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/xr_hand_tracking_data.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
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

bool OpenXrHandTracker::CanSupplyHandTrackingData() const {
  // We enable the OpenXrHandTracker because we may use it to synthesize a
  // controller, but if we cannot standardize the bone sizes, then we should not
  // return the hand tracking data. Note that data being potentially invalid is
  // a temporary state, and as such does not factor into this calculation.
  return mesh_scale_enabled_;
}

mojom::XRHandTrackingDataPtr OpenXrHandTracker::GetHandTrackingData() const {
  if (!CanSupplyHandTrackingData() || !IsDataValid()) {
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
