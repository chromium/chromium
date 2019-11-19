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

XRPose* XRGripSpace::getPose(XRSpace* other_space,
                             const TransformationMatrix* mojo_from_viewer) {
  // Grip is only available when using tracked pointer for input.
  if (input_source_->TargetRayMode() !=
      device::mojom::XRTargetRayMode::POINTING) {
    return nullptr;
  }

  // Make sure the required pose matrices are available.
  if (!mojo_from_viewer || !input_source_->MojoFromInput()) {
    return nullptr;
  }

  // Calculate grip pose in other_space, aka other_from_grip
  std::unique_ptr<TransformationMatrix> other_from_grip =
      other_space->SpaceFromInputForViewer(*(input_source_->MojoFromInput()),
                                           *mojo_from_viewer);
  if (!other_from_grip) {
    return nullptr;
  }

  // Account for any changes made to the reference space's origin offset so
  // that things like teleportation works.
  //
  // This is offset_from_grip = offset_from_other * other_from_grip,
  // where offset_from_other = inverse(other_from_offset).
  // TODO(https://crbug.com/1008466): move originOffset to separate class?
  TransformationMatrix offset_from_grip =
      other_space->InverseOriginOffsetMatrix().Multiply(*other_from_grip);
  return MakeGarbageCollected<XRPose>(offset_from_grip,
                                      input_source_->emulatedPosition());
}

base::Optional<XRNativeOriginInformation> XRGripSpace::NativeOrigin() const {
  return input_source_->nativeOrigin();
}

void XRGripSpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(input_source_);
  XRSpace::Trace(visitor);
}

}  // namespace blink
