// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "third_party/blink/renderer/modules/xr/xr_joint_space.h"
#include "third_party/blink/renderer/modules/xr/xr_hand.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

XRJointSpace::XRJointSpace(XRHand* hand,
                           XRSession* session,
                           std::unique_ptr<gfx::Transform> mojo_from_joint,
                           device::mojom::blink::XRHandJoint joint,
                           float radius,
                           device::mojom::blink::XRHandedness handedness)
    : XRSpace(session),
      hand_(hand),
      mojo_from_joint_space_(std::move(mojo_from_joint)),
      joint_(joint),
      radius_(radius),
      handedness_(handedness) {}

std::optional<gfx::Transform> XRJointSpace::MojoFromNative() const {
  return *mojo_from_joint_space_.get();
}

device::mojom::blink::XRNativeOriginInformationPtr XRJointSpace::NativeOrigin()
    const {
  device::mojom::blink::XRHandJointSpaceInfoPtr joint_space_info =
      device::mojom::blink::XRHandJointSpaceInfo::New();
  joint_space_info->handedness = this->handedness();
  joint_space_info->joint = this->joint();
  return device::mojom::blink::XRNativeOriginInformation::NewHandJointSpaceInfo(
      std::move(joint_space_info));
}

bool XRJointSpace::EmulatedPosition() const {
  return false;
}

XRPose* XRJointSpace::getPose(const XRSpace* other_space) const {
  // If any of the spaces belonging to the same XRHand return null when
  // populating the pose, all the spaces of that XRHand must also return
  // null when populating the pose.
  if (handHasMissingPoses()) {
    return nullptr;
  }

  // Return the base class' value if we are tracked.
  return XRSpace::getPose(other_space);
}

void XRJointSpace::UpdateTracking(
    std::unique_ptr<gfx::Transform> mojo_from_joint,
    float radius) {
  mojo_from_joint_space_ = std::move(mojo_from_joint);
  radius_ = radius;
}

bool XRJointSpace::IsStationary() const {
  return false;
}

V8XRHandJoint XRJointSpace::jointName() const {
  return V8XRHandJoint(MojomHandJointToV8Enum(joint_));
}

std::string XRJointSpace::ToString() const {
  return "XRJointSpace";
}

bool XRJointSpace::handHasMissingPoses() const {
  return hand_->hasMissingPoses();
}

void XRJointSpace::Trace(Visitor* visitor) const {
  visitor->Trace(hand_);
  XRSpace::Trace(visitor);
}

}  // namespace blink
