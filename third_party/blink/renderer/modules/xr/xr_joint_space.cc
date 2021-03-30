// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_joint_space.h"
#include "third_party/blink/renderer/modules/xr/xr_hand.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"

namespace blink {

XRJointSpace::XRJointSpace(
    XRSession* session,
    std::unique_ptr<TransformationMatrix> mojo_from_joint,
    device::mojom::blink::XRHandJoint joint,
    float radius,
    device::mojom::blink::XRHandedness handedness)
    : XRSpace(session),
      mojo_from_joint_space_(std::move(mojo_from_joint)),
      joint_(joint),
      radius_(radius),
      handedness_(handedness) {}

base::Optional<TransformationMatrix> XRJointSpace::MojoFromNative() {
  return *mojo_from_joint_space_.get();
}

bool XRJointSpace::EmulatedPosition() const {
  return false;
}

base::Optional<device::mojom::blink::XRNativeOriginInformation>
XRJointSpace::NativeOrigin() const {
  return XRNativeOriginInformation::Create(this);
}

bool XRJointSpace::IsStationary() const {
  return false;
}

const String XRJointSpace::jointName() const {
  return MojomHandJointToString(joint_);
}

std::string XRJointSpace::ToString() const {
  return "XRJointSpace";
}

void XRJointSpace::Trace(Visitor* visitor) const {
  XRSpace::Trace(visitor);
}

}  // namespace blink
