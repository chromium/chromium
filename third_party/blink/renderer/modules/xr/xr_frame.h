// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_H_

#include <memory>

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExceptionState;
class XRAnchorSet;
class XRDepthInformation;
class XRHitTestResult;
class XRHitTestSource;
class XRInputSource;
class XRLightEstimate;
class XRLightProbe;
class XRPose;
class XRReferenceSpace;
class XRRigidTransform;
class XRSession;
class XRSpace;
class XRTransientInputHitTestResult;
class XRTransientInputHitTestSource;
class XRView;
class XRViewerPose;
class XRWorldInformation;

class XRFrame final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRFrame(XRSession* session, XRWorldInformation* world_information);

  XRSession* session() const { return session_; }

  XRViewerPose* getViewerPose(XRReferenceSpace*, ExceptionState&);
  XRPose* getPose(XRSpace*, XRSpace*, ExceptionState&);
  XRWorldInformation* worldInformation() const { return world_information_; }
  XRAnchorSet* trackedAnchors() const;
  XRLightEstimate* getLightEstimate(XRLightProbe*, ExceptionState&) const;
  XRDepthInformation* getDepthInformation(XRView* view) const;

  void Trace(Visitor*) const override;

  void Deactivate();

  bool IsActive() const;

  void SetAnimationFrame(bool is_animation_frame) {
    is_animation_frame_ = is_animation_frame;
  }

  HeapVector<Member<XRHitTestResult>> getHitTestResults(
      XRHitTestSource* hit_test_source,
      ExceptionState& exception_state);

  HeapVector<Member<XRTransientInputHitTestResult>>
  getHitTestResultsForTransientInput(
      XRTransientInputHitTestSource* hit_test_source,
      ExceptionState& exception_state);

  ScriptPromise createAnchor(ScriptState* script_state,
                             XRRigidTransform* initial_pose,
                             XRSpace* space,
                             ExceptionState& exception_state);

 private:
  std::unique_ptr<TransformationMatrix> GetAdjustedPoseMatrix(XRSpace*) const;
  XRPose* GetTargetRayPose(XRInputSource*, XRSpace*) const;
  XRPose* GetGripPose(XRInputSource*, XRSpace*) const;

  // Helper that creates an anchor with the assumption that the conversion from
  // passed in space to a stationary space is required.
  // |native_origin_from_anchor| is a transform from |space|'s native origin to
  // the desired anchor position (i.e. the origin-offset of the |space| is
  // already taken into account).
  ScriptPromise CreateAnchorFromNonStationarySpace(
      ScriptState* script_state,
      const blink::TransformationMatrix& native_origin_from_anchor,
      XRSpace* space,
      ExceptionState& exception_state);

  Member<XRWorldInformation> world_information_;

  const Member<XRSession> session_;

  // Frames are only active during callbacks. getPose and getViewerPose should
  // only be called from JS on active frames.
  bool is_active_ = true;

  // Only frames created by XRSession.requestAnimationFrame callbacks are
  // animation frames. getViewerPose should only be called from JS on active
  // animation frames.
  bool is_animation_frame_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_H_
