// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_hand_utils.h"

#include "device/vr/public/mojom/xr_hand_tracking_data.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace device {

namespace {
struct XRFingerMapping {
  uint32_t start_index;
  uint32_t num_joints;
};

constexpr std::array<XRFingerMapping, 5> kFingerMappings({
    {static_cast<uint32_t>(mojom::XRHandJoint::kThumbMetacarpal), 4},
    {static_cast<uint32_t>(mojom::XRHandJoint::kIndexFingerMetacarpal), 5},
    {static_cast<uint32_t>(mojom::XRHandJoint::kMiddleFingerMetacarpal), 5},
    {static_cast<uint32_t>(mojom::XRHandJoint::kRingFingerMetacarpal), 5},
    {static_cast<uint32_t>(mojom::XRHandJoint::kPinkyFingerMetacarpal), 5},
});

// Helper method to create a fake hand with equally sized joints (while this
// isn't realistic, it is the easiest way to populate a structure, and the joint
// sizes should all be adjusted by the anonymization code to a fixed set of
// values anyway, and not a scale of their current values).
std::vector<mojom::XRHandJointDataPtr> GetHandData(
    gfx::Transform wrist_position,
    float joint_size) {
  std::vector<mojom::XRHandJointDataPtr> hand_data;
  hand_data.reserve(kNumWebXRJoints);

  // Specify the wrist, which is essentially the origin of the hand.
  auto wrist_data = mojom::XRHandJointData::New();
  wrist_data->joint = mojom::XRHandJoint::kWrist;
  wrist_data->mojo_from_joint = wrist_position;
  wrist_data->radius = 0.01;
  hand_data.push_back(std::move(wrist_data));

  // Copy off the wrist_position, as we'll be rotating it to help generate the
  // appropriate translation for each of the fingers.
  auto hand_origin = wrist_position;

  // This will create a hand with the middle finger in line with the wrist and
  // the other two fingers on either side "splayed" equally around it.
  constexpr double kFingerDegreeSeparation = 25;
  hand_origin.Rotate(-2 * kFingerDegreeSeparation);

  for (const auto& finger : kFingerMappings) {
    // Create a reusable transform. We'll keep extending it with each joint and
    // copying off the values as we move down the finger.
    gfx::Transform joint_translation;
    for (uint32_t i = finger.start_index;
         i < finger.start_index + finger.num_joints; i++) {
      joint_translation.Translate(joint_size, 0);
      auto joint_data = mojom::XRHandJointData::New();
      joint_data->joint = static_cast<mojom::XRHandJoint>(i);
      joint_data->radius = 0.01;
      joint_data->mojo_from_joint = hand_origin * joint_translation;
      hand_data.push_back(std::move(joint_data));
    }

    // Rotate the origin for the next hand so that it will be in a different
    // line
    hand_origin.Rotate(kFingerDegreeSeparation);
  }

  return hand_data;
}

bool TryGetLocation(const mojom::XRHandJointDataPtr& joint,
                    gfx::Vector3dF* location) {
  if (!joint || !joint->mojo_from_joint) {
    return false;
  }

  *location = joint->mojo_from_joint->To3dTranslation();
  return true;
}

bool HandJointSizesEqual(const std::vector<mojom::XRHandJointDataPtr>& hand1,
                         const std::vector<mojom::XRHandJointDataPtr>& hand2) {
  if (hand1.size() != hand2.size()) {
    return false;
  }

  if (!hand1[0]->mojo_from_joint || !hand2[0]->mojo_from_joint) {
    return false;
  }

  for (const auto& finger : kFingerMappings) {
    // Save the location of the "previous" joint to use to calculate the size
    // of the current joint. This will be updated as we iterate down the hand.
    gfx::Vector3dF hand1_previous_joint_pos =
        hand1[0]->mojo_from_joint->To3dTranslation();
    gfx::Vector3dF hand2_previous_joint_pos =
        hand2[0]->mojo_from_joint->To3dTranslation();
    for (uint32_t i = finger.start_index;
         i < finger.start_index + finger.num_joints; i++) {
      // Get the location of the current joint in each hand. If we cannot locate
      // either, then we assume that the joint sizes are not equal.
      gfx::Vector3dF hand1_current_joint_pos;
      if (!TryGetLocation(hand1[i], &hand1_current_joint_pos)) {
        return false;
      }

      gfx::Vector3dF hand2_current_joint_pos;
      if (!TryGetLocation(hand2[i], &hand2_current_joint_pos)) {
        return false;
      }

      // Calculate the distances and compare that they are the same to within a
      // reasonable error tolerance due to floating point numbers. Because our
      // units are typically in meters, this represents the joints being equal
      // to within 0.01mm.
      constexpr float kOneHundredthMillimeter = 0.00001;
      float distance1 =
          (hand1_current_joint_pos - hand1_previous_joint_pos).Length();
      float distance2 =
          (hand2_current_joint_pos - hand2_previous_joint_pos).Length();
      if ((distance2 - distance1) > kOneHundredthMillimeter) {
        return false;
      }

      hand1_previous_joint_pos = hand1_current_joint_pos;
      hand2_previous_joint_pos = hand2_current_joint_pos;
    }
  }

  return true;
}
}  // namespace

// Tests that two hands who are not originally the same are the same after
// running the anonymize function.
TEST(OpenXrHandUtils, BasicAnonymize) {
  gfx::Transform wrist1;
  wrist1.Translate(-1, 1);
  auto hand1 = GetHandData(wrist1, 0.07);

  gfx::Transform wrist2;
  wrist2.Translate(1, 1);
  auto hand2 = GetHandData(wrist2, 0.08);
  EXPECT_FALSE(HandJointSizesEqual(hand1, hand2));

  EXPECT_TRUE(AnonymizeHand(hand1));
  EXPECT_TRUE(AnonymizeHand(hand2));
  EXPECT_TRUE(HandJointSizesEqual(hand1, hand2));
}

// Tests that AnonymizeHand fails if the wrist is missing.
TEST(OpenXrHandUtils, MissingWristFails) {
  gfx::Transform wrist1;
  wrist1.Translate(-1, 1);
  auto hand1 = GetHandData(wrist1, 0.07);
  hand1[0]->mojo_from_joint = std::nullopt;

  EXPECT_FALSE(AnonymizeHand(hand1));
}

// Tests that AnonymizeHand fails if a finger is missing.
TEST(OpenXrHandUtils, MissingFingerFails) {
  gfx::Transform wrist1;
  wrist1.Translate(-1, 1);
  auto hand1 = GetHandData(wrist1, 0.07);
  auto pinky_info = kFingerMappings[4];
  for (uint32_t i = pinky_info.start_index;
       i < pinky_info.start_index + pinky_info.num_joints; i++) {
    hand1[i]->mojo_from_joint = std::nullopt;
  }

  EXPECT_FALSE(AnonymizeHand(hand1));
}

}  // namespace device
