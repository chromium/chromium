// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/xr/xr_frame.h"

#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_result.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_joint_space.h"
#include "third_party/blink/renderer/modules/xr/xr_light_estimate.h"
#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"
#include "third_party/blink/renderer/modules/xr/xr_plane_set.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_result.h"
#include "third_party/blink/renderer/modules/xr/xr_transient_input_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

const char kInvalidView[] =
    "XRView passed in to the method did not originate from current XRFrame.";

const char kSessionMismatch[] = "XRSpace and XRFrame sessions do not match.";

const char kHitTestSourceUnavailable[] =
    "Unable to obtain hit test results for specified hit test source. Ensure "
    "that it was not already canceled.";

const char kCannotObtainNativeOrigin[] =
    "The operation was unable to obtain necessary information and could not be "
    "completed.";

const char kSpacesSequenceTooLarge[] =
    "Insufficient buffer capacity for pose results.";

const char kMismatchedBufferSizes[] = "Buffer sizes must be equal";

std::optional<uint64_t> GetPlaneId(
    const device::mojom::blink::XRNativeOriginInformation& native_origin) {
  if (native_origin.is_plane_id()) {
    return native_origin.get_plane_id();
  }

  return std::nullopt;
}

}  // namespace

constexpr char XRFrame::kInactiveFrame[];
constexpr char XRFrame::kNonAnimationFrame[];

XRFrame::XRFrame(XRSession* session, bool is_animation_frame)
    : session_(session), is_animation_frame_(is_animation_frame) {}

XRViewerPose* XRFrame::getViewerPose(XRReferenceSpace* reference_space,
                                     ExceptionState& exception_state) {
  DCHECK(reference_space);

  DVLOG(3) << __func__ << ": is_active_=" << is_active_
           << ", is_animation_frame_=" << is_animation_frame_
           << ", reference_space->ToString()=" << reference_space->ToString();

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

  // Must use a reference space created from the same session.
  if (!IsSameSession(reference_space->session(), exception_state)) {
    return nullptr;
  }

  if (!session_->CanReportPoses()) {
    exception_state.ThrowSecurityError(XRSession::kCannotReportPoses);
    return nullptr;
  }

  session_->LogGetPose();

  std::optional<gfx::Transform> native_from_mojo =
      reference_space->NativeFromMojo();
  if (!native_from_mojo) {
    DVLOG(1) << __func__ << ": native_from_mojo is invalid";
    return nullptr;
  }

  gfx::Transform ref_space_from_mojo =
      reference_space->OffsetFromNativeMatrix();
  ref_space_from_mojo.PreConcat(*native_from_mojo);

  // Can only update an XRViewerPose's views with an invertible matrix.
  if (!ref_space_from_mojo.IsInvertible()) {
    DVLOG(1) << __func__ << ": ref_space_from_mojo is not invertible";
    return nullptr;
  }

  std::optional<gfx::Transform> offset_space_from_viewer =
      reference_space->OffsetFromViewer();

  // Can only update an XRViewerPose's views with an invertible matrix.
  if (!(offset_space_from_viewer && offset_space_from_viewer->IsInvertible())) {
    DVLOG(1) << __func__
             << ": offset_space_from_viewer is invalid or not invertible - "
                "returning nullptr, offset_space_from_viewer valid? "
             << (offset_space_from_viewer ? true : false);
    return nullptr;
  }

  device::mojom::blink::XRReferenceSpaceType type = reference_space->GetType();

  // If the |reference_space| type is kViewer, we know that the pose is not
  // emulated. Otherwise, ask the session if the poses are emulated or not.
  return MakeGarbageCollected<XRViewerPose>(
      this, ref_space_from_mojo, *offset_space_from_viewer,
      (type == device::mojom::blink::XRReferenceSpaceType::kViewer)
          ? false
          : session_->EmulatedPosition());
}

XRAnchorSet* XRFrame::trackedAnchors() const {
  return session_->TrackedAnchors();
}

XRPlaneSet* XRFrame::detectedPlanes(ExceptionState& exception_state) const {
  DVLOG(3) << __func__;

  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  return session_->GetDetectedPlanes();
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
  if (!IsSameSession(light_probe->session(), exception_state)) {
    return nullptr;
  }

  return light_probe->getLightEstimate();
}

XRCPUDepthInformation* XRFrame::getDepthInformation(
    XRView* view,
    ExceptionState& exception_state) const {
  DVLOG(2) << __func__;

  if (!session_->IsFeatureEnabled(device::mojom::XRSessionFeature::DEPTH)) {
    DVLOG(2) << __func__ << ": depth sensing is not enabled on a session";
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        XRSession::kDepthSensingFeatureNotSupported);
    return nullptr;
  }

  if (!is_active_) {
    DVLOG(2) << __func__ << ": frame is not active";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (!is_animation_frame_) {
    DVLOG(2) << __func__ << ": frame is not animating";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kNonAnimationFrame);
    return nullptr;
  }

  if (this != view->frame()) {
    DVLOG(2) << __func__ << ": view did not originate from the frame";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInvalidView);
    return nullptr;
  }

  return view->GetCpuDepthInformation(exception_state);
}

XRPose* XRFrame::getPose(XRSpace* space,
                         XRSpace* basespace,
                         ExceptionState& exception_state) {
  DCHECK(space);
  DCHECK(basespace);

  DVLOG(2) << __func__ << ": is_active=" << is_active_
           << ", space->ToString()=" << space->ToString()
           << ", basespace->ToString()=" << basespace->ToString();

  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (!IsSameSession(space->session(), exception_state) ||
      !IsSameSession(basespace->session(), exception_state)) {
    return nullptr;
  }

  if (!session_->CanReportPoses()) {
    exception_state.ThrowSecurityError(XRSession::kCannotReportPoses);
    return nullptr;
  }

  // If the addresses match, the pose between the spaces is definitely an
  // identity & we can skip the rest of the logic. The pose is not emulated.
  if (space == basespace) {
    DVLOG(3) << __func__ << ": addresses match, returning identity";
    return MakeGarbageCollected<XRPose>(gfx::Transform{}, false);
  }

  // If the native origins match, the pose between the spaces is fixed and
  // depends only on their offsets from the same native origin - we can compute
  // it here and skip the rest of the logic. The pose is not emulated.
  if (space->NativeOrigin() == basespace->NativeOrigin()) {
    DVLOG(3) << __func__
             << ": native origins match, returning a pose based on offesets";
    auto basespace_from_native_origin = basespace->OffsetFromNativeMatrix();
    auto native_origin_from_space = space->NativeFromOffsetMatrix();

    return MakeGarbageCollected<XRPose>(
        basespace_from_native_origin * native_origin_from_space, false);
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

const FrozenArray<XRHitTestResult>& XRFrame::getHitTestResults(
    XRHitTestSource* hit_test_source,
    ExceptionState& exception_state) {
  if (!hit_test_source ||
      !session_->ValidateHitTestSourceExists(hit_test_source)) {
    // This should only happen when hit test source was already canceled.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kHitTestSourceUnavailable);
    return *MakeGarbageCollected<FrozenArray<XRHitTestResult>>();
  }

  return *MakeGarbageCollected<FrozenArray<XRHitTestResult>>(
      hit_test_source->Results());
}

const FrozenArray<XRTransientInputHitTestResult>&
XRFrame::getHitTestResultsForTransientInput(
    XRTransientInputHitTestSource* hit_test_source,
    ExceptionState& exception_state) {
  if (!hit_test_source ||
      !session_->ValidateHitTestSourceExists(hit_test_source)) {
    // This should only happen when hit test source was already canceled.
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kHitTestSourceUnavailable);
    return *MakeGarbageCollected<FrozenArray<XRTransientInputHitTestResult>>();
  }

  return *MakeGarbageCollected<FrozenArray<XRTransientInputHitTestResult>>(
      hit_test_source->Results());
}

ScriptPromise<XRAnchor> XRFrame::createAnchor(
    ScriptState* script_state,
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

  device::mojom::blink::XRNativeOriginInformationPtr maybe_native_origin =
      space->NativeOrigin();
  if (!maybe_native_origin) {
    DVLOG(2) << __func__ << ": native origin not set, failing anchor creation";
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kCannotObtainNativeOrigin);
    return {};
  }

  DVLOG(3) << __func__ << ": space->ToString()=" << space->ToString();
  auto maybe_plane_id = GetPlaneId(*maybe_native_origin);

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
                                        maybe_native_origin, maybe_plane_id,
                                        exception_state);
  }

  return CreateAnchorFromNonStationarySpace(script_state,
                                            native_origin_from_anchor, space,
                                            maybe_plane_id, exception_state);
}

ScriptPromise<XRAnchor> XRFrame::CreateAnchorFromNonStationarySpace(
    ScriptState* script_state,
    const gfx::Transform& native_origin_from_anchor,
    XRSpace* space,
    std::optional<uint64_t> maybe_plane_id,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  // Space is not considered stationary - need to adjust the app-provided pose.
  // Let's ask the session about the appropriate stationary reference space:
  std::optional<XRSession::ReferenceSpaceInformation>
      reference_space_information = session_->GetStationaryReferenceSpace();

  if (!reference_space_information) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kUnableToRetrieveMatrix);
    return {};
  }

  auto stationary_space_from_mojo =
      reference_space_information->mojo_from_space.GetCheckedInverse();

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
      reference_space_information->native_origin, maybe_plane_id,
      exception_state);
}

bool XRFrame::IsSameSession(XRSession* space_session,
                            ExceptionState& exception_state) const {
  if (space_session != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return false;
  }
  return true;
}

const FrozenArray<XRImageTrackingResult>& XRFrame::getImageTrackingResults(
    ExceptionState& exception_state) {
  return session_->ImageTrackingResults(exception_state);
}

XRJointPose* XRFrame::getJointPose(XRJointSpace* joint,
                                   XRSpace* baseSpace,
                                   ExceptionState& exception_state) const {
  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (!IsSameSession(baseSpace->session(), exception_state) ||
      !IsSameSession(joint->session(), exception_state)) {
    return nullptr;
  }

  if (!session_->CanReportPoses()) {
    exception_state.ThrowSecurityError(XRSession::kCannotReportPoses);
    return nullptr;
  }

  const XRPose* pose = joint->getPose(baseSpace);
  if (!pose) {
    return nullptr;
  }

  const float radius = joint->radius();

  return MakeGarbageCollected<XRJointPose>(pose->transform()->TransformMatrix(),
                                           radius);
}

bool XRFrame::fillJointRadii(HeapVector<Member<XRJointSpace>>& jointSpaces,
                             NotShared<DOMFloat32Array> radii,
                             ExceptionState& exception_state) const {
  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return false;
  }

  for (const auto& joint_space : jointSpaces) {
    if (!IsSameSession(joint_space->session(), exception_state)) {
      return false;
    }
  }

  if (jointSpaces.size() != radii->length()) {
    exception_state.ThrowTypeError(kMismatchedBufferSizes);
    return false;
  }

  bool all_valid = true;

  for (unsigned offset = 0; offset < jointSpaces.size(); offset++) {
    const XRJointSpace* joint_space = jointSpaces[offset];
    if (joint_space->handHasMissingPoses()) {
      radii->Data()[offset] = NAN;
      all_valid = false;
    } else {
      radii->Data()[offset] = joint_space->radius();
    }
  }

  return all_valid;
}

bool XRFrame::fillPoses(HeapVector<Member<XRSpace>>& spaces,
                        XRSpace* baseSpace,
                        NotShared<DOMFloat32Array> transforms,
                        ExceptionState& exception_state) const {
  const unsigned floats_per_transform = 16;

  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return false;
  }

  for (const auto& space : spaces) {
    if (!IsSameSession(space->session(), exception_state)) {
      return false;
    }
  }

  if (!IsSameSession(baseSpace->session(), exception_state)) {
    return false;
  }

  if (spaces.size() * floats_per_transform > transforms->length()) {
    exception_state.ThrowTypeError(kSpacesSequenceTooLarge);
    return false;
  }

  if (!session_->CanReportPoses()) {
    exception_state.ThrowSecurityError(XRSession::kCannotReportPoses);
    return false;
  }

  bool allValid = true;
  unsigned offset = 0;
  for (const auto& space : spaces) {
    const XRPose* pose = space->getPose(baseSpace);
    if (!pose) {
      for (unsigned i = 0; i < floats_per_transform; i++) {
        transforms->Data()[offset + i] = NAN;
      }
      allValid = false;
    } else {
      const float* const poseMatrix = pose->transform()->matrix()->Data();
      for (unsigned i = 0; i < floats_per_transform; i++) {
        transforms->Data()[offset + i] = poseMatrix[i];
      }
    }

    offset += floats_per_transform;
  }

  return allValid;
}

void XRFrame::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
