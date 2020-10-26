// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_grip_space.h"

#include <utility>

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"

namespace blink {

XRGripSpace::XRGripSpace(XRSession* session, XRInputSource* source)
    : XRSpace(session), input_source_(source) {}

base::Optional<TransformationMatrix> XRGripSpace::MojoFromNative() {
  // Grip is only available when using tracked pointer for input.
  if (input_source_->TargetRayMode() !=
      device::mojom::XRTargetRayMode::POINTING) {
    return base::nullopt;
  }

  return input_source_->MojoFromInput();
}

bool XRGripSpace::EmulatedPosition() const {
  return input_source_->emulatedPosition();
}

base::Optional<device::mojom::blink::XRNativeOriginInformation>
XRGripSpace::NativeOrigin() const {
  // Grip space's native origin is equal to input source's native origin, but
  // only when using tracked pointer for input.
  if (input_source_->TargetRayMode() !=
      device::mojom::XRTargetRayMode::POINTING) {
    return base::nullopt;
  }

  return input_source_->nativeOrigin();
}

bool XRGripSpace::IsStationary() const {
  // Grip space is a space derived off of input source, so it is not considered
  // stationary.
  return false;
}

std::string XRGripSpace::ToString() const {
  return "XRGripSpace";
}

void XRGripSpace::Trace(Visitor* visitor) const {
  visitor->Trace(input_source_);
  XRSpace::Trace(visitor);
}

}  // namespace blink
