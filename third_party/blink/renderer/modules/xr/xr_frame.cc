// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_frame.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_light_estimate.h"
#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_world_information.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

const char kInactiveFrame[] =
    "XRFrame access outside the callback that produced it is invalid.";

const char kInvalidView[] =
    "XRView passed in to the method did not originate from current XRFrame.";

const char kNonAnimationFrame[] =
    "getViewerPose can only be called on XRFrame objects passed to "
    "XRSession.requestAnimationFrame callbacks.";

const char kSessionMismatch[] = "XRSpace and XRFrame sessions do not match.";

const char kCannotReportPoses[] =
    "Poses cannot be given out for the current state.";

const char kHitTestSourceUnavailable[] =
    "Unable to obtain hit test results for specified hit test source. Ensure "
    "that it was not already canceled.";

const char kCannotObtainNativeOrigin[] =
    "The operation was unable to obtain necessary information and could not be "
    "completed.";

}  // namespace

XRFrame::XRFrame(XRSession* session, XRWorldInformation* world_information)
    : world_information_(world_information), session_(session) {}

XRViewerPose* XRFrame::getViewerPose(XRReferenceSpace* reference_space,
                                     ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (!is_animation_frame_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNonAnimationFrame);
    return nullptr;
  }

  if (!reference_space) {
    DVLOG(1) << __func__ << ": reference space not present, returning null";
    return nullptr;
  }

  // Must use a reference space created from the same session.
  if (reference_space->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  if (!session_->CanReportPoses()) {
    exception_state.ThrowSecurityError(kCannotReportPoses);
    return nullptr;
  }

  session_->LogGetPose();

  base::Optional<TransformationMatrix> offset_space_from_viewer =
      reference_space->OffsetFromViewer();

  // Can only update an XRViewerPose's views with an invertible matrix.
  if (!(offset_space_from_viewer && offset_space_from_viewer->IsInvertible())) {
    DVLOG(1) << __func__
             << ": offset_space_from_viewer is invalid or not invertible - "
                "returning nullptr, offset_space_from_viewer valid? "
             << (offset_space_from_viewer ? true : false);
    return nullptr;
  }

  return MakeGarbageCollected<XRViewerPose>(this, *offset_space_from_viewer);
}

XRAnchorSet* XRFrame::trackedAnchors() const {
  return session_->TrackedAnchors();
}

XRLightEstimate* XRFrame::getLightEstimate(
    XRLightProbe* light_probe,
    ExceptionState& exception_state) const {
  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (!is_animation_frame_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNonAnimationFrame);
    return nullptr;
  }

  if (!light_probe) {
    return nullptr;
  }

  // Must use a light probe created from the same session.
  if (light_probe->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  return light_probe->getLightEstimate();
}

XRDepthInformation* XRFrame::getDepthInformation(
    XRView* view,
    ExceptionState& exception_state) const {
  DVLOG(2) << __func__;

  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (this != view->frame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidView);
    return nullptr;
  }

  return session_->GetDepthInformation();
}

// Return an XRPose that has a transform of basespace_from_space, while
// accounting for the base pose matrix of this frame. If computing a transform
// isn't possible, return nullptr.
XRPose* XRFrame::getPose(XRSpace* space,
                         XRSpace* basespace,
                         ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (!space || !basespace) {
    DVLOG(2) << __func__ << " : space or basespace is null, space =" << space
             << ", basespace = " << basespace;
    return nullptr;
  }

  if (space->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  if (basespace->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  if (!session_->CanReportPoses()) {
    exception_state.ThrowSecurityError(kCannotReportPoses);
    return nullptr;
  }

  return space->getPose(basespace);
}

void XRFrame::Deactivate() {
  is_active_ = false;
  is_animation_frame_ = false;
}

bool XRFrame::IsActive() const {
  return is_active_;
}

HeapVector<Member<XRHitTestResult>> XRFrame::getHitTestResults(
    XRHitTestSource* hit_test_source,
    ExceptionState& exception_state) {
  if (!hit_test_source ||
      !session_->ValidateHitTestSourceExists(hit_test_source)) {
    // This should only happen when hit test source was already canceled.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kHitTestSourceUnavailable);
    return {};
  }

  return hit_test_source->Results();
}

HeapVector<Member<XRTransientInputHitTestResult>>
XRFrame::getHitTestResultsForTransientInput(
    XRTransientInputHitTestSource* hit_test_source,
    ExceptionState& exception_state) {
  if (!hit_test_source ||
      !session_->ValidateHitTestSourceExists(hit_test_source)) {
    // This should only happen when hit test source was already canceled.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kHitTestSourceUnavailable);
    return {};
  }

  return hit_test_source->Results();
}

ScriptPromise XRFrame::createAnchor(ScriptState* script_state,
                                    XRRigidTransform* offset_space_from_anchor,
                                    XRSpace* space,
                                    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (!session_->IsFeatureEnabled(device::mojom::XRSessionFeature::ANCHORS)) {
    DVLOG(2) << __func__
             << ": feature not enabled on a session, failing anchor creation";
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      XRSession::kAnchorsFeatureNotSupported);
    return {};
  }

  if (!is_active_) {
    DVLOG(2) << __func__ << ": frame not active, failing anchor creation";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return {};
  }

  if (!offset_space_from_anchor) {
    DVLOG(2) << __func__
             << ": offset_space_from_anchor not set, failing anchor creation";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kNoRigidTransformSpecified);
    return {};
  }

  if (!space) {
    DVLOG(2) << __func__ << ": space not set, failing anchor creation";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kNoSpaceSpecified);
    return {};
  }

  base::Optional<device::mojom::blink::XRNativeOriginInformation>
      maybe_native_origin = space->NativeOrigin();
  if (!maybe_native_origin) {
    DVLOG(2) << __func__ << ": native origin not set, failing anchor creation";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kCannotObtainNativeOrigin);
    return {};
  }

  // The passed in space may be an offset space, we need to transform the pose
  // to account for origin-offset:
  auto native_origin_from_offset_space = space->NativeFromOffsetMatrix();
  auto native_origin_from_anchor = native_origin_from_offset_space *
                                   offset_space_from_anchor->TransformMatrix();

  // We should strive to create an anchor whose location aligns with the pose
  // |offset_space_from_anchor| relative to |space|. For spaces that are
  // dynamically changing, this means we need to convert the pose to be relative
  // to stationary space, using data valid in the current frame, and change the
  // native origin relative to which the pose is expressed when communicating
  // with the device. For spaces that are classified as stationary, this
  // adjustment is not needed.

  if (space->IsStationary()) {
    // Space is considered stationary, no adjustments are needed.
    return session_->CreateAnchorHelper(script_state, native_origin_from_anchor,
                                        *maybe_native_origin, exception_state);
  }

  return CreateAnchorFromNonStationarySpace(
      script_state, native_origin_from_anchor, space, exception_state);
}

ScriptPromise XRFrame::CreateAnchorFromNonStationarySpace(
    ScriptState* script_state,
    const blink::TransformationMatrix& native_origin_from_anchor,
    XRSpace* space,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  // Space is not considered stationary - need to adjust the app-provided pose.
  // Let's ask the session about the appropriate stationary reference space:
  base::Optional<XRSession::ReferenceSpaceInformation>
      reference_space_information = session_->GetStationaryReferenceSpace();

  if (!reference_space_information) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kUnableToRetrieveMatrix);
    return {};
  }

  const TransformationMatrix& mojo_from_stationary_space =
      reference_space_information->mojo_from_space;

  DCHECK(mojo_from_stationary_space.IsInvertible());
  auto stationary_space_from_mojo = mojo_from_stationary_space.Inverse();

  // We now have 2 spaces - the dynamic one passed in to create anchor
  // call, and the stationary one. We also have a rigid transform
  // expressed relative to the dynamic space. Time to convert it so that it's
  // expressed relative to stationary space.

  auto mojo_from_native_origin = space->MojoFromNative();
  if (!mojo_from_native_origin) {
    DVLOG(2) << __func__ << ": native_origin not set, failing anchor creation";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kUnableToRetrieveMatrix);
    return {};
  }

  auto mojo_from_anchor = *mojo_from_native_origin * native_origin_from_anchor;
  auto stationary_space_from_anchor =
      stationary_space_from_mojo * mojo_from_anchor;

  // Conversion done, make the adjusted call:
  return session_->CreateAnchorHelper(
      script_state, stationary_space_from_anchor,
      reference_space_information->native_origin, exception_state);
}

void XRFrame::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(world_information_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
