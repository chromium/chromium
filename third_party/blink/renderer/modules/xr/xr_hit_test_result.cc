// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hit_test_result.h"

#include "third_party/blink/renderer/modules/xr/vr_service_type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

XRHitTestResult::XRHitTestResult(
    XRSession* session,
    const device::mojom::blink::XRHitResult& hit_result)
    : session_(session),
      mojo_from_this_(hit_result.mojo_from_result),
      plane_id_(hit_result.plane_id != 0
                    ? std::optional<uint64_t>(hit_result.plane_id)
                    : std::nullopt) {}

XRPose* XRHitTestResult::getPose(XRSpace* other,
                                 ExceptionState& exception_state) {
  if (!session_->CanReportPoses()) {
    DVLOG(3) << __func__ << ": cannot report poses";
    exception_state.ThrowSecurityError(XRSession::kCannotReportPoses);
    return nullptr;
  }

  auto maybe_other_space_native_from_mojo = other->NativeFromMojo();
  if (!maybe_other_space_native_from_mojo) {
    return nullptr;
  }

  auto mojo_from_this = mojo_from_this_.ToTransform();

  auto other_native_from_mojo = *maybe_other_space_native_from_mojo;
  auto other_offset_from_other_native = other->OffsetFromNativeMatrix();

  auto other_offset_from_mojo =
      other_offset_from_other_native * other_native_from_mojo;

  auto other_offset_from_this = other_offset_from_mojo * mojo_from_this;

  return MakeGarbageCollected<XRPose>(other_offset_from_this, false);
}

ScriptPromise<XRAnchor> XRHitTestResult::createAnchor(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DVLOG(2) << __func__;

  if (!session_->IsFeatureEnabled(device::mojom::XRSessionFeature::ANCHORS)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      XRSession::kAnchorsFeatureNotSupported);
    DVLOG(3) << __func__ << ": anchors not supported on the session";
    return {};
  }

  // TODO(https://crbug.com/954236): Revisit the approach of plane poses not
  // being stable from frame to frame - if we could guarantee that anchor poses
  // are not so dynamic, anchor creation could be improved.
  //
  // Planes are not considered stationary for the purpose of anchor creation
  // (their poses may change dramatically on a frame-by-frame basis). Grab an
  // information about reference space that is well-suited for anchor creation
  // from session:
  std::optional<XRSession::ReferenceSpaceInformation>
      reference_space_information = session_->GetStationaryReferenceSpace();

  if (!reference_space_information) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      XRSession::kUnableToRetrieveMatrix);
    DVLOG(3) << __func__ << ": unable to obtain stationary reference space";
    return {};
  }

  auto space_from_mojo =
      reference_space_information->mojo_from_space.GetCheckedInverse();
  auto space_from_anchor = space_from_mojo * mojo_from_this_.ToTransform();

  return session_->CreateAnchorHelper(
      script_state, space_from_anchor,
      reference_space_information->native_origin, plane_id_, exception_state);
}

void XRHitTestResult::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
