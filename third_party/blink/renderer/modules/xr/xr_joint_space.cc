// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_joint_space.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"

namespace blink {

XRJointSpace::XRJointSpace(
    XRSession* session,
    std::unique_ptr<TransformationMatrix> mojo_from_joint,
    String joint_name,
    float radius)
    : XRSpace(session),
      mojo_from_joint_space_(std::move(mojo_from_joint)),
      joint_name_(joint_name),
      radius_(radius) {}

base::Optional<TransformationMatrix> XRJointSpace::MojoFromNative() {
  return *mojo_from_joint_space_.get();
}

bool XRJointSpace::EmulatedPosition() const {
  NOTIMPLEMENTED();
  return false;
}

base::Optional<device::mojom::blink::XRNativeOriginInformation>
XRJointSpace::NativeOrigin() const {
  NOTIMPLEMENTED();
  return base::nullopt;
}

bool XRJointSpace::IsStationary() const {
  return false;
}

std::string XRJointSpace::ToString() const {
  return "XRJointSpace";
}

void XRJointSpace::Trace(Visitor* visitor) const {
  XRSpace::Trace(visitor);
}

}  // namespace blink
