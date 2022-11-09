// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_depth_information.h"

#include <cstdlib>

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "ui/gfx/geometry/transform.h"

namespace {

constexpr char kFrameInactive[] =
    "XRDepthInformation members are only accessible when their XRFrame's "
    "`active` boolean is `true`.";
constexpr char kFrameNotAnimated[] =
    "XRDepthInformation members are only accessible when their XRFrame's "
    "`animationFrame` boolean is `true`.";
}

namespace blink {

XRDepthInformation::XRDepthInformation(
    const XRFrame* xr_frame,
    const gfx::Size& size,
    const gfx::Transform& norm_depth_buffer_from_norm_view,
    float raw_value_to_meters)
    : xr_frame_(xr_frame),
      size_(size),
      norm_depth_buffer_from_norm_view_(norm_depth_buffer_from_norm_view),
      raw_value_to_meters_(raw_value_to_meters) {
  DVLOG(3) << __func__ << ": size_=" << size_.ToString()
           << ", norm_depth_buffer_from_norm_view_="
           << norm_depth_buffer_from_norm_view_.ToString()
           << ", raw_value_to_meters_=" << raw_value_to_meters_;
}

uint32_t XRDepthInformation::width() const {
  return size_.width();
}

uint32_t XRDepthInformation::height() const {
  return size_.height();
}

float XRDepthInformation::rawValueToMeters() const {
  return raw_value_to_meters_;
}

XRRigidTransform* XRDepthInformation::normDepthBufferFromNormView() const {
  return MakeGarbageCollected<XRRigidTransform>(
      norm_depth_buffer_from_norm_view_);
}

bool XRDepthInformation::ValidateFrame(ExceptionState& exception_state) const {
  if (!xr_frame_->IsActive()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kFrameInactive);
    return false;
  }

  if (!xr_frame_->IsAnimationFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kFrameNotAnimated);
    return false;
  }

  return true;
}

void XRDepthInformation::Trace(Visitor* visitor) const {
  visitor->Trace(xr_frame_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
