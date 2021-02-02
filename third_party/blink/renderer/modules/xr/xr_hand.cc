// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hand.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_joint_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_joint_space.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

XRHand::XRHand(const device::mojom::blink::XRHandTrackingData* state,
               XRInputSource* input_source) {
  DCHECK_EQ(state->hand_joint_data.size(), kNumJoints);

  for (unsigned i = 0; i < state->hand_joint_data.size(); i++) {
    device::mojom::blink::XRHandJoint joint = state->hand_joint_data[i]->joint;
    DCHECK(static_cast<unsigned>(joint) < state->hand_joint_data.size());

    XRJointSpace* joint_space = MakeGarbageCollected<XRJointSpace>(
        input_source->session(),
        std::make_unique<TransformationMatrix>(
            state->hand_joint_data[i]->mojo_from_joint.matrix()),
        joint, state->hand_joint_data[i]->radius,
        input_source->xr_handedness());

    joint_spaces_[static_cast<unsigned>(joint)] = {
        MojomHandJointToString(joint), std::move(joint_space)};
  }
}

XRJointSpace* XRHand::get(const String& key) {
  return std::get<THandJointsMapValue>(
      joint_spaces_[static_cast<unsigned>(StringToMojomHandJoint(key))]);
}

XRHand::HandJointIterationSource::HandJointIterationSource(
    const THandJointCollection& joint_spaces)
    : joint_spaces_(joint_spaces), current(0) {}

bool XRHand::HandJointIterationSource::Next(ScriptState* script_state,
                                            THandJointsMapKey& key,
                                            THandJointsMapValue& value,
                                            ExceptionState& exception_state) {
  if (current >= joint_spaces_.size())
    return false;
  key = std::get<THandJointsMapKey>(joint_spaces_[current]);
  value = std::get<THandJointsMapValue>(joint_spaces_[current]);
  current++;
  return true;
}

void XRHand::Trace(Visitor* visitor) const {
  for (const auto& key_value_pair : joint_spaces_) {
    visitor->Trace(std::get<THandJointsMapValue>(key_value_pair));
  }
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
