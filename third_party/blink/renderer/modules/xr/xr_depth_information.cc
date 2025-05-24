// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_depth_information.h"

#include <cstdlib>

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
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
    const XRView* xr_view,
    const device::mojom::blink::XRViewGeometryPtr& view_geometry,
    const gfx::Size& size,
    const gfx::Transform& norm_depth_buffer_from_norm_view,
    float raw_value_to_meters)
    : xr_view_(xr_view),
      size_(size),
      norm_depth_buffer_from_norm_view_(norm_depth_buffer_from_norm_view),
      raw_value_to_meters_(raw_value_to_meters),
      view_geometry_(view_geometry ? std::make_optional<XRViewGeometry>(
                                         view_geometry,
                                         xr_view->session()->GraphicsApi())
                                   : std::nullopt) {
  CHECK_NE(raw_value_to_meters_, 0);
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

XRRigidTransform* XRDepthInformation::viewGeometryTransform() const {
  // Transform is given in the view's reference space:
  // https://immersive-web.github.io/depth-sensing/#xr-depth-info-section
  if (view_geometry_) {
    return MakeGarbageCollected<XRRigidTransform>(
        xr_view_->refSpaceFromMojo() * view_geometry_->MojoFromView());
  }

  // If we don't have a view_geometry_ we need to return data from the xr_view_.
  return xr_view_->viewGeometryTransform();
}

NotShared<DOMFloat32Array> XRDepthInformation::projectionMatrix() const {
  if (view_geometry_) {
    return transformationMatrixToDOMFloat32Array(
        view_geometry_->ProjectionMatrix());
  }

  // If we don't have a view_geometry_ we need to return data from the xr_view_.
  return xr_view_->projectionMatrix();
}

bool XRDepthInformation::ValidateFrame(ExceptionState& exception_state) const {
  if (!xr_view_->frame()->IsActive()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kFrameInactive);
    return false;
  }

  if (!xr_view_->frame()->IsAnimationFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kFrameNotAnimated);
    return false;
  }

  return true;
}

void XRDepthInformation::Trace(Visitor* visitor) const {
  visitor->Trace(xr_view_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
