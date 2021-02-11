// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_H_

#include <memory>

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix.h"
#include "third_party/blink/renderer/modules/xr/xr_joint_pose.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExceptionState;
class XRAnchorSet;
class XRCPUDepthInformation;
class XRHitTestResult;
class XRHitTestSource;
class XRImageTrackingResult;
class XRInputSource;
class XRJointPose;
class XRLightEstimate;
class XRLightProbe;
class XRJointSpace;
class XRPlaneSet;
class XRPose;
class XRReferenceSpace;
class XRRigidTransform;
class XRSession;
class XRSpace;
class XRTransientInputHitTestResult;
class XRTransientInputHitTestSource;
class XRView;
class XRViewerPose;

class XRFrame final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static constexpr char kInactiveFrame[] =
      "XRFrame access outside the callback that produced it is invalid.";
  static constexpr char kNonAnimationFrame[] =
      "This method can only be called on XRFrame objects passed to "
      "XRSession.requestAnimationFrame callbacks.";

  explicit XRFrame(XRSession* session, bool is_animation_frame = false);

  XRSession* session() const { return session_; }

  XRViewerPose* getViewerPose(XRReferenceSpace*, ExceptionState&);
  XRPose* getPose(XRSpace*, XRSpace*, ExceptionState&);
  XRAnchorSet* trackedAnchors() const;
  XRLightEstimate* getLightEstimate(XRLightProbe*, ExceptionState&) const;
  XRCPUDepthInformation* getDepthInformation(
      XRView* view,
      ExceptionState& exception_state) const;
  XRPlaneSet* detectedPlanes(ExceptionState& exception_state) const;

  void Trace(Visitor*) const override;

  void Deactivate();

  bool IsActive() const;

  bool IsAnimationFrame() const { return is_animation_frame_; }

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

  HeapVector<Member<XRImageTrackingResult>> getImageTrackingResults(
      ExceptionState&);

  XRJointPose* getJointPose(XRJointSpace* joint,
                            XRSpace* baseSpace,
                            ExceptionState& exception_state);
  bool fillJointRadii(HeapVector<Member<XRJointSpace>>& jointSpaces,
                      NotShared<DOMFloat32Array> radii,
                      ExceptionState& exception_state);
  bool fillPoses(HeapVector<Member<XRSpace>>& spaces,
                 XRSpace* baseSpace,
                 NotShared<DOMFloat32Array> transforms,
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

  const Member<XRSession> session_;

  // Frames are only active during callbacks. getPose and getViewerPose should
  // only be called from JS on active frames.
  bool is_active_ = true;

  // Only frames created by XRSession.requestAnimationFrame callbacks are
  // animation frames. getViewerPose should only be called from JS on active
  // animation frames.
  bool is_animation_frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_H_
