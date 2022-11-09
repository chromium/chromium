// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hand.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_joint_space.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

class XRHandIterationSource final
    : public PairIterable<String,
                          IDLString,
                          Member<XRJointSpace>,
                          XRJointSpace>::IterationSource {
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
    PairIterable<String, IDLString, Member<XRJointSpace>,
                 XRJointSpace>::IterationSource::Trace(visitor);
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
        this, input_source->session(), nullptr, joint, 0.0f,
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
  bool new_missing_poses = false;  // hand was updated with a null pose
  bool new_poses = false;          // hand was updated with a valid pose

  for (const auto& hand_joint : state->hand_joint_data) {
    unsigned joint_index = static_cast<unsigned>(hand_joint->joint);

    std::unique_ptr<gfx::Transform> mojo_from_joint = nullptr;
    if (hand_joint->mojo_from_joint) {
      new_poses = true;
      mojo_from_joint =
          std::make_unique<gfx::Transform>(*hand_joint->mojo_from_joint);
    } else {
      new_missing_poses = true;
    }

    joints_[joint_index]->UpdateTracking(std::move(mojo_from_joint),
                                         hand_joint->radius);
  }

  if (new_missing_poses) {
    // There is at least one missing pose.
    has_missing_poses_ = true;
  } else if (has_missing_poses_ && new_poses) {
    // Need to check if there are any missing poses
    has_missing_poses_ =
        !base::ranges::all_of(joints_, &XRJointSpace::MojoFromNative);
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
