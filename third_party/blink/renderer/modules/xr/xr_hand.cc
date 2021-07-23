// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hand.h"

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_joint_space.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

class XRHandIterationSource final
    : public PairIterable<String, Member<XRJointSpace>>::IterationSource {
 public:
  explicit XRHandIterationSource(HeapVector<Member<XRJointSpace>>& joints)
      : index_(0), joints_(joints) {}

  bool Next(ScriptState*,
            String& key,
            Member<XRJointSpace>& value,
            ExceptionState&) override {
    if (index_ >= joints_.size())
      return false;

    key = MojomHandJointToString(
        static_cast<device::mojom::blink::XRHandJoint>(index_));
    value = joints_.at(index_);
    index_++;
    return true;
  }

  void Trace(Visitor* visitor) const override {
    PairIterable<String, Member<XRJointSpace>>::IterationSource::Trace(visitor);
  }

 private:
  wtf_size_t index_;
  const HeapVector<Member<XRJointSpace>>& joints_;
};

XRHand::XRHand(const device::mojom::blink::XRHandTrackingData* state,
               XRInputSource* input_source)
    : joints_(kNumJoints) {
  for (unsigned i = 0; i < kNumJoints; ++i) {
    device::mojom::blink::XRHandJoint joint =
        static_cast<device::mojom::blink::XRHandJoint>(i);
    joints_[i] = MakeGarbageCollected<XRJointSpace>(
        input_source->session(), nullptr, joint, 0.0f,
        input_source->xr_handedness());
  }

  updateFromHandTrackingData(state, input_source);
}

XRJointSpace* XRHand::get(const String& key) {
  device::mojom::blink::XRHandJoint joint = StringToMojomHandJoint(key);
  unsigned index = static_cast<unsigned>(joint);
  return joints_[index];
}

void XRHand::updateFromHandTrackingData(
    const device::mojom::blink::XRHandTrackingData* state,
    XRInputSource* input_source) {
  for (unsigned i = 0; i < state->hand_joint_data.size(); i++) {
    const auto& hand_joint_data = state->hand_joint_data[i];
    unsigned joint_index = static_cast<unsigned>(hand_joint_data->joint);

    joints_[joint_index]->UpdateTracking(
        std::make_unique<TransformationMatrix>(
            hand_joint_data->mojo_from_joint.matrix()),
        hand_joint_data->radius);
  }
}

XRHand::IterationSource* XRHand::StartIteration(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<XRHandIterationSource>(joints_);
}

void XRHand::Trace(Visitor* visitor) const {
  visitor->Trace(joints_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
