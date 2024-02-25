// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_grip_space.h"

#include <utility>

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"

namespace blink {

XRGripSpace::XRGripSpace(XRSession* session, XRInputSource* source)
    : XRSpace(session), input_source_(source) {}

std::optional<gfx::Transform> XRGripSpace::MojoFromNative() const {
  // Grip is only available when using tracked pointer for input.
  if (input_source_->TargetRayMode() !=
      device::mojom::XRTargetRayMode::POINTING) {
    return std::nullopt;
  }

  return input_source_->MojoFromInput();
}

bool XRGripSpace::EmulatedPosition() const {
  return input_source_->emulatedPosition();
}

device::mojom::blink::XRNativeOriginInformationPtr XRGripSpace::NativeOrigin()
    const {
  // Grip space's native origin is valid only when using tracked pointer for
  // input.
  if (input_source_->TargetRayMode() !=
      device::mojom::XRTargetRayMode::POINTING) {
    return nullptr;
  }

  return device::mojom::blink::XRNativeOriginInformation::
      NewInputSourceSpaceInfo(device::mojom::blink::XRInputSourceSpaceInfo::New(
          input_source_->source_id(),
          device::mojom::blink::XRInputSourceSpaceType::kGrip));
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
