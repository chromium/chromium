// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_view.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_eye.h"
#include "third_party/blink/renderer/modules/xr/xr_camera.h"
#include "third_party/blink/renderer/modules/xr/xr_depth_manager.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_utils.h"
#include "third_party/blink/renderer/modules/xr/xr_view_geometry.h"
#include "ui/gfx/geometry/point3_f.h"

namespace blink {

namespace {

// Arbitrary minimum size multiplier for dynamic viewport scaling,
// where 1.0 is full framebuffer size (which may in turn be adjusted
// by framebufferScaleFactor). This should be less than or equal to
// kMinScale in xr_session_viewport_scaler.cc to allow use of the full
// dynamic viewport scaling range.
constexpr double kMinViewportScale = 0.125;

}  // namespace

XRView::XRView(XRFrame* frame,
               XRViewData* view_data,
               const gfx::Transform& ref_space_from_mojo)
    : eye_(view_data->Eye()),
      ref_space_from_mojo_(ref_space_from_mojo),
      frame_(frame),
      view_data_(view_data) {
  ref_space_from_view_ = MakeGarbageCollected<XRRigidTransform>(
      ref_space_from_mojo_ * view_data->MojoFromView());
  projection_matrix_ =
      transformationMatrixToDOMFloat32Array(view_data->ProjectionMatrix());
}

XRViewport* XRView::Viewport(double framebuffer_scale) {
  if (!viewport_) {
    const gfx::Rect& viewport = view_data_->Viewport();
    double scale = framebuffer_scale * view_data_->CurrentViewportScale();

    viewport_ = MakeGarbageCollected<XRViewport>(
        viewport.x() * scale, viewport.y() * scale, viewport.width() * scale,
        viewport.height() * scale);
  }

  return viewport_.Get();
}

V8XREye XRView::eye() const {
  return GetV8Eye(eye_);
}

unsigned XRView::index() const {
  return view_data_->index();
}

XRFrame* XRView::frame() const {
  return frame_.Get();
}

XRSession* XRView::session() const {
  return frame_->session();
}

NotShared<DOMFloat32Array> XRView::projectionMatrix() const {
  if (!projection_matrix_ || projection_matrix_->IsDetached()) {
    // A page may take the projection matrix value and detach it so
    // projection_matrix_ is a detached array buffer.  This breaks the
    // inspector, so return an empty array instead.
    projection_matrix_ =
        transformationMatrixToDOMFloat32Array(view_data_->ProjectionMatrix());
  }

  return projection_matrix_;
}

XRCPUDepthInformation* XRView::GetCpuDepthInformation(
    ExceptionState& exception_state) const {
  return view_data_->GetCpuDepthInformation(this, exception_state);
}

XRWebGLDepthInformation* XRView::GetWebGLDepthInformation(
    ExceptionState& exception_state) const {
  return view_data_->GetWebGLDepthInformation(this, exception_state);
}

XRRigidTransform* XRView::viewGeometryTransform() const {
  // The viewGeometryTransform for XRView is `ref_space_from_view`.
  // https://immersive-web.github.io/webxr/#ref-for-dom-xrviewgeometry-transform%E2%91%A2
  return ref_space_from_view_.Get();
}

std::optional<double> XRView::recommendedViewportScale() const {
  return view_data_->recommendedViewportScale();
}

void XRView::requestViewportScale(std::optional<double> scale) {
  view_data_->requestViewportScale(scale);
}

XRCamera* XRView::camera() const {
  const bool camera_access_enabled = frame_->session()->IsFeatureEnabled(
      device::mojom::XRSessionFeature::CAMERA_ACCESS);
  const bool is_immersive_ar_session =
      frame_->session()->mode() ==
      device::mojom::blink::XRSessionMode::kImmersiveAr;

  DVLOG(3) << __func__ << ": camera_access_enabled=" << camera_access_enabled
           << ", is_immersive_ar_session=" << is_immersive_ar_session;

  if (camera_access_enabled && is_immersive_ar_session) {
    // The feature is enabled and we're in immersive-ar session, so let's return
    // a camera object if the camera image was received in the current frame.
    // Note: currently our only implementation of AR sessions is provided by
    // ARCore device, which should *not* return a frame data with camera image
    // that is not set in case the raw camera access is enabled, so we could
    // DCHECK that the camera image size has value. Since there may be other AR
    // devices that implement raw camera access via a different mechanism that's
    // not neccessarily frame-aligned, a DCHECK here would affect them.
    if (frame_->session()->CameraImageSize().has_value()) {
      return MakeGarbageCollected<XRCamera>(frame_);
    }
  }

  return nullptr;
}

bool XRView::isFirstPersonObserver() const {
  return view_data_->IsFirstPersonObserver();
}

void XRView::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(projection_matrix_);
  visitor->Trace(ref_space_from_view_);
  visitor->Trace(view_data_);
  visitor->Trace(viewport_);
  ScriptWrappable::Trace(visitor);
}

// XRViewData
XRViewData::XRViewData(
    wtf_size_t index,
    device::mojom::blink::XRViewPtr view,
    double depth_near,
    double depth_far,
    const device::mojom::blink::XRSessionDeviceConfig& device_config,
    const HashSet<device::mojom::XRSessionFeature>& enabled_feature_set,
    XRGraphicsBinding::Api graphics_api)
    : XRViewGeometry(graphics_api),
      index_(index),
      eye_(view->eye),
      viewport_(view->viewport) {
  if (base::Contains(enabled_feature_set,
                     device::mojom::XRSessionFeature::DEPTH)) {
    if (!device_config.depth_configuration) {
      DCHECK(false)
          << "The session reports that depth sensing is supported but "
             "did not report depth sensing API configuration!";
    }
    depth_manager_ = MakeGarbageCollected<XRDepthManager>(
        base::PassKey<XRViewData>{}, *device_config.depth_configuration);
  }

  UpdateView(std::move(view), depth_near, depth_far);
}

void XRViewData::UpdateView(device::mojom::blink::XRViewPtr view,
                            double depth_near,
                            double depth_far) {
  DCHECK_EQ(eye_, view->eye);

  UpdateViewGeometry(view->geometry, depth_near, depth_far);

  viewport_ = view->viewport;
  is_first_person_observer_ = view->is_first_person_observer;
  if (depth_manager_) {
    depth_manager_->ProcessDepthInformation(std::move(view->depth_data));
  }

  visibility_mask_ = std::move(view->visibility_mask);
  visibility_mask_id_ = view->visibility_mask_id;
}

XRCPUDepthInformation* XRViewData::GetCpuDepthInformation(
    const XRView* xr_view,
    ExceptionState& exception_state) const {
  if (!depth_manager_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        XRSession::kDepthSensingFeatureNotSupported);
    return nullptr;
  }

  return depth_manager_->GetCpuDepthInformation(xr_view, exception_state);
}

XRWebGLDepthInformation* XRViewData::GetWebGLDepthInformation(
    const XRView* xr_view,
    ExceptionState& exception_state) const {
  if (!depth_manager_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        XRSession::kDepthSensingFeatureNotSupported);
    return nullptr;
  }

  return depth_manager_->GetWebGLDepthInformation(xr_view, exception_state);
}

std::optional<double> XRViewData::recommendedViewportScale() const {
  return recommended_viewport_scale_;
}

void XRViewData::requestViewportScale(std::optional<double> scale) {
  if (!scale)
    return;

  requested_viewport_scale_ = std::clamp(*scale, kMinViewportScale, 1.0);
}

bool XRViewData::ApplyViewportScaleForFrame() {
  bool changed = false;

  // Dynamic viewport scaling, see steps 6 and 7 in
  // https://immersive-web.github.io/webxr/#dom-xrwebgllayer-getviewport
  if (ViewportModifiable() &&
      CurrentViewportScale() != RequestedViewportScale()) {
    DVLOG(2) << __func__
             << ": apply ViewportScale=" << RequestedViewportScale();
    SetCurrentViewportScale(RequestedViewportScale());
    changed = true;
  }
  TRACE_COUNTER1("xr", "XR viewport scale (%)", CurrentViewportScale() * 100);
  SetViewportModifiable(false);

  return changed;
}

void XRViewData::OnVisibilityMaskChangeEvent() {
  last_evented_visibility_mask_id_ = visibility_mask_id_;
}

bool XRViewData::NeedsVisibilityMaskChangeEvent() const {
  if (!last_evented_visibility_mask_id_) {
    return true;
  }

  return last_evented_visibility_mask_id_.value() != visibility_mask_id_;
}

void XRViewData::Trace(Visitor* visitor) const {
  visitor->Trace(depth_manager_);
}

}  // namespace blink
