// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_target_ray_space.h"

#include <utility>

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"

namespace blink {

XRTargetRaySpace::XRTargetRaySpace(XRSession* session, XRInputSource* source)
    : XRSpace(session), input_source_(source) {}

std::unique_ptr<TransformationMatrix> XRTargetRaySpace::OtherSpaceFromScreenTap(
    XRSpace* other_space,
    const TransformationMatrix& mojo_from_viewer) {
  // If the pointer origin is the screen, the input space is viewer space, and
  // we need the head's viewer pose and the pointer pose in continue. The
  // pointer transform will represent the point the canvas was clicked as an
  // offset from the view.
  if (!input_source_->InputFromPointer()) {
    return nullptr;
  }

  // other_from_pointer = other_from_input * input_from_pointer,
  // where input space is equivalent to viewer space for screen taps.
  std::unique_ptr<TransformationMatrix> other_from_pointer =
      other_space->SpaceFromViewer(mojo_from_viewer);
  if (!other_from_pointer) {
    return nullptr;
  }
  other_from_pointer->Multiply(*(input_source_->InputFromPointer()));
  return other_from_pointer;
}

std::unique_ptr<TransformationMatrix>
XRTargetRaySpace::OtherSpaceFromTrackedPointer(
    XRSpace* other_space,
    const TransformationMatrix& mojo_from_viewer) {
  if (!input_source_->MojoFromInput()) {
    return nullptr;
  }

  // Calculate other_from_pointer = other_from_input * input_from_pointer
  std::unique_ptr<TransformationMatrix> other_from_pointer =
      other_space->SpaceFromInputForViewer(*(input_source_->MojoFromInput()),
                                           mojo_from_viewer);

  if (!other_from_pointer) {
    return nullptr;
  }
  if (input_source_->InputFromPointer()) {
    other_from_pointer->Multiply(*(input_source_->InputFromPointer()));
  }
  return other_from_pointer;
}

XRPose* XRTargetRaySpace::getPose(
    XRSpace* other_space,
    const TransformationMatrix* mojo_from_viewer) {
  // If we don't have a valid base pose (most common when tracking is lost),
  // we can't get a target ray pose regardless of the mode.
  if (!mojo_from_viewer) {
    DVLOG(2) << __func__ << " : mojo_from_viewer is null, returning nullptr";
    return nullptr;
  }

  std::unique_ptr<TransformationMatrix> other_from_ray = nullptr;
  switch (input_source_->TargetRayMode()) {
    case device::mojom::XRTargetRayMode::TAPPING: {
      other_from_ray = OtherSpaceFromScreenTap(other_space, *mojo_from_viewer);
      break;
    }
    case device::mojom::XRTargetRayMode::GAZING: {
      // If the pointer origin is the users head, this is a gaze cursor and the
      // returned pointer is based on the device pose. Just return the head pose
      // as the pointer pose.
      other_from_ray = other_space->SpaceFromViewer(*mojo_from_viewer);
      break;
    }
    case device::mojom::XRTargetRayMode::POINTING: {
      other_from_ray =
          OtherSpaceFromTrackedPointer(other_space, *mojo_from_viewer);
      break;
    }
  }

  if (!other_from_ray) {
    DVLOG(2) << __func__ << " : "
             << "other_from_ray is null, input_source_->TargetRayMode() = "
             << input_source_->TargetRayMode();
    return nullptr;
  }

  // Account for any changes made to the reference space's origin offset so that
  // things like teleportation works.
  //
  // otheroffset_from_ray = otheroffset_from_other * other_from_ray
  // where otheroffset_from_other = inverse(other_from_otheroffset)
  // TODO(https://crbug.com/1008466): move originOffset to separate class?
  TransformationMatrix otheroffset_from_ray =
      other_space->InverseOriginOffsetMatrix().Multiply(*other_from_ray);
  return MakeGarbageCollected<XRPose>(otheroffset_from_ray,
                                      input_source_->emulatedPosition());
}

base::Optional<XRNativeOriginInformation> XRTargetRaySpace::NativeOrigin()
    const {
  return input_source_->nativeOrigin();
}

void XRTargetRaySpace::Trace(blink::Visitor* visitor) {
  visitor->Trace(input_source_);
  XRSpace::Trace(visitor);
}

}  // namespace blink
