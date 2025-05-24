// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_image_tracking_result.h"

#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/modules/xr/xr_object_space.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

XRImageTrackingResult::XRImageTrackingResult(
    XRSession* session,
    const device::mojom::blink::XRTrackedImageData& result)
    : session_(session),
      index_(result.index),
      mojo_from_this_(result.mojo_from_image),
      width_in_meters_(result.width_in_meters) {
  DVLOG(2) << __func__ << ": image index=" << index_;
  if (result.actively_tracked) {
    tracking_state_ =
        V8XRImageTrackingState(V8XRImageTrackingState::Enum::kTracked);
  } else {
    tracking_state_ =
        V8XRImageTrackingState(V8XRImageTrackingState::Enum::kEmulated);
  }
}

std::optional<gfx::Transform> XRImageTrackingResult::MojoFromObject() const {
  if (!mojo_from_this_) {
    return std::nullopt;
  }

  return mojo_from_this_->ToTransform();
}

XRSpace* XRImageTrackingResult::imageSpace() const {
  if (!image_space_) {
    image_space_ = MakeGarbageCollected<XRObjectSpace<XRImageTrackingResult>>(
        session_, this);
  }

  return image_space_.Get();
}

device::mojom::blink::XRNativeOriginInformationPtr
XRImageTrackingResult::NativeOrigin() const {
  return device::mojom::blink::XRNativeOriginInformation::NewImageIndex(index_);
}

void XRImageTrackingResult::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(image_space_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
