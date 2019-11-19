// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_frame.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"
#include "third_party/blink/renderer/modules/xr/xr_view.h"
#include "third_party/blink/renderer/modules/xr/xr_viewer_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_world_information.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

namespace {

const char kInactiveFrame[] =
    "XRFrame access outside the callback that produced it is invalid.";

const char kNonAnimationFrame[] =
    "getViewerPose can only be called on XRFrame objects passed to "
    "XRSession.requestAnimationFrame callbacks.";

const char kSessionMismatch[] = "XRSpace and XRFrame sessions do not match.";

const char kCannotReportPoses[] =
    "Poses cannot be given out for the current state.";

}  // namespace

XRFrame::XRFrame(XRSession* session, XRWorldInformation* world_information)
    : world_information_(world_information), session_(session) {}

XRViewerPose* XRFrame::getViewerPose(XRReferenceSpace* reference_space,
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

  if (!reference_space) {
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

  std::unique_ptr<TransformationMatrix> pose =
      reference_space->SpaceFromViewerWithDefaultAndOffset(
          mojo_from_viewer_.get());
  if (!pose) {
    return nullptr;
  }

  return MakeGarbageCollected<XRViewerPose>(session(), *pose);
}

XRAnchorSet* XRFrame::trackedAnchors() const {
  return session_->trackedAnchors();
}

// Return an XRPose that has a transform mapping to space A from space B, while
// accounting for the base pose matrix of this frame. If computing a transform
// isn't possible, return nullptr.
XRPose* XRFrame::getPose(XRSpace* space_A,
                         XRSpace* space_B,
                         ExceptionState& exception_state) {
  if (!is_active_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kInactiveFrame);
    return nullptr;
  }

  if (!space_A || !space_B) {
    DVLOG(2) << __func__
             << " : space_A or space_B is null, space_A =" << space_A
             << ", space_B = " << space_B;
    return nullptr;
  }

  if (space_A->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  if (space_B->session() != session_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kSessionMismatch);
    return nullptr;
  }

  if (!session_->CanReportPoses()) {
    exception_state.ThrowSecurityError(kCannotReportPoses);
    return nullptr;
  }

  return space_A->getPose(space_B, mojo_from_viewer_.get());
}

void XRFrame::SetMojoFromViewer(const TransformationMatrix& mojo_from_viewer,
                                bool emulated_position) {
  mojo_from_viewer_ = std::make_unique<TransformationMatrix>(mojo_from_viewer);
  emulated_position_ = emulated_position;
}

void XRFrame::Deactivate() {
  is_active_ = false;
  is_animation_frame_ = false;
}

HeapVector<Member<XRHitTestResult>> XRFrame::getHitTestResults(
    XRHitTestSource* hit_test_source) {
  if (!session_->ValidateHitTestSourceExists(hit_test_source))
    return {};
  return hit_test_source->Results();
}

void XRFrame::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(world_information_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
