// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_hand_utils.h"

#include <iomanip>
#include <sstream>

#include "base/containers/span.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "device/vr/public/mojom/xr_hand_tracking_data.mojom.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace device {
namespace {
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

constexpr size_t kThumbJointCount = 4;
constexpr size_t kIndexJointCount = 5;
constexpr size_t kMiddleJointCount = 5;
constexpr size_t kRingJointCount = 5;
constexpr size_t kPinkyJointCount = 5;

// Add one for the wrist
static_assert(1 + kThumbJointCount + kIndexJointCount + kMiddleJointCount +
                  kRingJointCount + kPinkyJointCount ==
              kNumWebXRJoints);

// Values reported by Meta Quest runtime with XrHandTrackingScaleFB extension
constexpr std::array<float, kNumWebXRJoints> kJointRadii = {
    0.01,   0.0194, .0123,  0.0098, 0.0088, 0.0212, 0.0103, 0.0085, 0.0076,
    0.0066, 0.0212, 0.0112, 0.0080, 0.0076, 0.0066, 0.0191, 0.0099, 0.0076,
    0.0072, 0.0062, 0.0181, 0.0085, 0.0068, 0.0064, 0.0054};

constexpr std::array<float, kThumbJointCount> kThumbDistances = {
    0.0486, 0.0325, 0.0338, 0.0246};
constexpr std::array<float, kIndexJointCount> kIndexDistances = {
    0.0425, 0.0597, 0.0379, 0.0243, 0.0224};
constexpr std::array<float, kMiddleJointCount> kMiddleDistances = {
    0.0353, 0.0616, 0.0429, 0.0275, 0.0250};
constexpr std::array<float, kRingJointCount> kRingDistances = {
    0.0383, 0.0540, 0.0390, 0.0266, 0.0244};
constexpr std::array<float, kPinkyJointCount> kPinkyDistances = {
    0.0422, 0.0457, 0.0307, 0.0203, 0.0220};

constexpr size_t kThumbOrigin = static_cast<size_t>(mojom::XRHandJoint::kWrist);
constexpr size_t kIndexOrigin = static_cast<size_t>(mojom::XRHandJoint::kWrist);
constexpr size_t kMiddleOrigin =
    static_cast<size_t>(mojom::XRHandJoint::kWrist);
constexpr size_t kRingOrigin = static_cast<size_t>(mojom::XRHandJoint::kWrist);
constexpr size_t kPinkyOrigin = static_cast<size_t>(mojom::XRHandJoint::kWrist);

constexpr size_t kThumbStartIndex =
    static_cast<size_t>(mojom::XRHandJoint::kThumbMetacarpal);
constexpr size_t kIndexStartIndex =
    static_cast<size_t>(mojom::XRHandJoint::kIndexFingerMetacarpal);
constexpr size_t kMiddleStartIndex =
    static_cast<size_t>(mojom::XRHandJoint::kMiddleFingerMetacarpal);
constexpr size_t kRingStartIndex =
    static_cast<size_t>(mojom::XRHandJoint::kRingFingerMetacarpal);
constexpr size_t kPinkyStartIndex =
    static_cast<size_t>(mojom::XRHandJoint::kPinkyFingerMetacarpal);

struct XRFingerMapping {
  // The index of the "origin" of the finger. This value will not be remapped.
  const size_t origin_index;
  // The first joint of the finger to be remapped.
  const size_t start_index;

  // TODO(367764863) Rewrite to base::raw_span.
  RAW_PTR_EXCLUSION const base::span<const float> standard_joint_sizes;

  size_t JointCount() const { return standard_joint_sizes.size(); }
};

constexpr std::array<XRFingerMapping, 5> kHandMapping({
    {kThumbOrigin, kThumbStartIndex, kThumbDistances},
    {kIndexOrigin, kIndexStartIndex, kIndexDistances},
    {kMiddleOrigin, kMiddleStartIndex, kMiddleDistances},
    {kRingOrigin, kRingStartIndex, kRingDistances},
    {kPinkyOrigin, kPinkyStartIndex, kPinkyDistances},
});
}  // namespace

bool AnonymizeHand(base::span<mojom::XRHandJointDataPtr> hand_data) {
  // First set all of the radii to their intended values.
  for (const auto& joint : hand_data) {
    joint->radius = kJointRadii[static_cast<uint32_t>(joint->joint)];
  }

  // Now work our way down each finger to set the distances appropriately. The
  // basic strategy here is that we have two points. The "base" or "origin" (e.g
  // the tip of the previous joint) and the "tip" of the current joint. These
  // two points represent a Euclidean vector in space, which has both magnitude
  // and direction. This magnitude is what needs to be anonymized to a standard
  // joint distance rather than the user's actual joint distance. We do this by
  // adjusting where the "tip" of the current joint is. This has ripple effects
  // down the rest of the finger as each joint now has a new origin. So we use
  // the original joint positions to calculate the magnitude and direction,
  // scale to the new magnitude, and then reset both the joint's magnitude and
  // origin, keeping the relative direction between the two joints the same.
  for (const auto& finger_info : kHandMapping) {
    // Find the position of the initial base joint for the finger, since this
    // base isn't moving, we also set it as the first
    // previous_joint_anonymized_position|, which the next joint will use to
    // calculate it's new position. For now, if this base is unlocatable, fail
    // anonymization.
    const auto& base_joint = hand_data[finger_info.origin_index];
    if (!base_joint->mojo_from_joint) {
      return false;
    }

    const auto& base_transform = base_joint->mojo_from_joint.value();

    // |previous_joint_real_position| is the real origin of the joint previous
    // to the one that we are currently working on, and is what we will use to
    // calculate the current vector, which needs to be scaled.
    gfx::Vector3dF previous_joint_real_position =
        base_transform.To3dTranslation();

    // |previous_joint_anonymized_position| represents the adjusted position of
    // the joint previous to the  one that we are currently working on, and is
    // the location that the new location of the tip of the current one needs to
    // be calculated from.
    gfx::Vector3dF previous_joint_anonymized_position =
        base_transform.To3dTranslation();

    // For a little bit of simplicity, extract this finger as a subspan from the
    // overall hand joint data vector. However, we'll still need to use an index
    // based iterator, as we need to index into both this array and the array
    // with the standard sizes simultaneously.
    base::span<mojom::XRHandJointDataPtr> joints =
        hand_data.subspan(finger_info.start_index, finger_info.JointCount());
    for (size_t i = 0; i < joints.size(); i++) {
      // If we cannot locate the joint fail anonymization. We could try to
      // accommodate, but that would in the best case require simply sending a
      // "double long" joint and in the worst case emulating an entire finger.
      if (!joints[i]->mojo_from_joint.has_value()) {
        return false;
      }
      gfx::Transform& current_joint_transform =
          joints[i]->mojo_from_joint.value();

      // To find the vector between two points simply subtract the origin from
      // the destination.
      auto previous_joint_to_current_joint =
          current_joint_transform.To3dTranslation() -
          previous_joint_real_position;

      // If two joints are reported as on top of each other, we have no way of
      // knowing the actual direction and must return false.
      if (previous_joint_to_current_joint.Length() <
          std::numeric_limits<float>::epsilon()) {
        return false;
      }

      // Calculate and adjust how far off from the standard length it is.
      float scale_factor = finger_info.standard_joint_sizes[i] /
                           previous_joint_to_current_joint.Length();
      previous_joint_to_current_joint.Scale(scale_factor);

      // Save the original location of this joint for the next calculation.
      previous_joint_real_position = current_joint_transform.To3dTranslation();

      // Need to move previous_joint_to_current_joint from the previous base to
      // get to the base of this joint.
      const auto current_joint_anonymized_position =
          previous_joint_anonymized_position + previous_joint_to_current_joint;

      // We essentially want to set the translation component of
      // |current_joint_transform| to |current_joint_anonymized_position|. We
      // do this by directly manipulating the matrix, because any rotation of
      // the joint shouldn't affect this calculation.
      current_joint_transform.set_rc(0, 3,
                                     current_joint_anonymized_position.x());
      current_joint_transform.set_rc(1, 3,
                                     current_joint_anonymized_position.y());
      current_joint_transform.set_rc(2, 3,
                                     current_joint_anonymized_position.z());

      // Save this position as the new "previous" position for the next joint.
      previous_joint_anonymized_position = current_joint_anonymized_position;
    }
  }

  return true;
}

}  // namespace device
