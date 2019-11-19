// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_H_

#include <memory>

#include "device/vr/public/mojom/vr_service.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExceptionState;
class XRHitTestResult;
class XRHitTestSource;
class XRInputSource;
class XRPose;
class XRReferenceSpace;
class XRSession;
class XRSpace;
class XRViewerPose;
class XRAnchorSet;
class XRWorldInformation;

class XRFrame final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit XRFrame(XRSession* session, XRWorldInformation* world_information);

  XRSession* session() const { return session_; }

  XRViewerPose* getViewerPose(XRReferenceSpace*, ExceptionState&) const;
  XRPose* getPose(XRSpace*, XRSpace*, ExceptionState&);
  XRWorldInformation* worldInformation() const { return world_information_; }
  XRAnchorSet* trackedAnchors() const;

  void SetMojoFromViewer(const TransformationMatrix&, bool emulated_position);

  void Trace(blink::Visitor*) override;

  void Deactivate();

  void SetAnimationFrame(bool is_animation_frame) {
    is_animation_frame_ = is_animation_frame;
  }

  HeapVector<Member<XRHitTestResult>> getHitTestResults(
      XRHitTestSource* hit_test_source);

  bool EmulatedPosition() const { return emulated_position_; }

 private:
  std::unique_ptr<TransformationMatrix> GetAdjustedPoseMatrix(XRSpace*) const;
  XRPose* GetTargetRayPose(XRInputSource*, XRSpace*) const;
  XRPose* GetGripPose(XRInputSource*, XRSpace*) const;

  Member<XRWorldInformation> world_information_;

  const Member<XRSession> session_;

  // Viewer pose in mojo space, the matrix maps from viewer (headset) space to
  // mojo space.
  std::unique_ptr<TransformationMatrix> mojo_from_viewer_;

  // Frames are only active during callbacks. getPose and getViewerPose should
  // only be called from JS on active frames.
  bool is_active_ = true;

  // Only frames created by XRSession.requestAnimationFrame callbacks are
  // animation frames. getViewerPose should only be called from JS on active
  // animation frames.
  bool is_animation_frame_ = false;

  bool emulated_position_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_FRAME_H_
