// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_target_ray_space.h"

#include <string>
#include <utility>

#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

XRTargetRaySpace::XRTargetRaySpace(XRSession* session, XRInputSource* source)
    : XRSpace(session), input_source_(source) {}

std::optional<gfx::Transform> XRTargetRaySpace::MojoFromNative() const {
  auto mojo_from_viewer = session()->GetMojoFrom(
      device::mojom::blink::XRReferenceSpaceType::kViewer);
  switch (input_source_->TargetRayMode()) {
    case device::mojom::XRTargetRayMode::TAPPING: {
      // If the pointer origin is the screen, we need mojo_from_viewer, as the
      // viewer space is the input space.
      // So our result will be mojo_from_viewer * viewer_from_pointer
      if (!(mojo_from_viewer && input_source_->InputFromPointer()))
        return std::nullopt;

      return *mojo_from_viewer * *(input_source_->InputFromPointer());
    }
    case device::mojom::XRTargetRayMode::GAZING: {
      // If the pointer origin is gaze, then the pointer offset is just
      // mojo_from_viewer.

      return mojo_from_viewer;
    }
    case device::mojom::XRTargetRayMode::POINTING: {
      // mojo_from_pointer is just: MojoFromInput*InputFromPointer;
      if (!(input_source_->MojoFromInput() &&
            input_source_->InputFromPointer()))
        return std::nullopt;

      return *(input_source_->MojoFromInput()) *
             *(input_source_->InputFromPointer());
    }
  }
}

bool XRTargetRaySpace::EmulatedPosition() const {
  return input_source_->emulatedPosition();
}

device::mojom::blink::XRNativeOriginInformationPtr
XRTargetRaySpace::NativeOrigin() const {
  return device::mojom::blink::XRNativeOriginInformation::
      NewInputSourceSpaceInfo(device::mojom::blink::XRInputSourceSpaceInfo::New(
          input_source_->source_id(),
          device::mojom::blink::XRInputSourceSpaceType::kTargetRay));
}

std::string XRTargetRaySpace::ToString() const {
  return "XRTargetRaySpace";
}

bool XRTargetRaySpace::IsStationary() const {
  // Target ray space is a space derived off of input source, so it is not
  // considered stationary.
  return false;
}

void XRTargetRaySpace::Trace(Visitor* visitor) const {
  visitor->Trace(input_source_);
  XRSpace::Trace(visitor);
}

}  // namespace blink
