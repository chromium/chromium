// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEW_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"

namespace blink {

class XRSession;
class XRViewData;

class MODULES_EXPORT XRView final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRView(XRSession*, const XRViewData&);

  enum XREye { kEyeNone = 0, kEyeLeft = 1, kEyeRight = 2 };

  const String& eye() const { return eye_string_; }
  XREye EyeValue() const { return eye_; }

  XRSession* session() const;
  DOMFloat32Array* projectionMatrix() const;
  XRRigidTransform* transform() const;

  void Trace(blink::Visitor*) override;

 private:
  XREye eye_;
  String eye_string_;
  Member<XRSession> session_;
  Member<XRRigidTransform> ref_space_from_eye_;
  Member<DOMFloat32Array> projection_matrix_;
};

class MODULES_EXPORT XRViewData {
 public:
  XRViewData(XRView::XREye eye) : eye_(eye) {}

  void UpdatePoseMatrix(const TransformationMatrix& ref_space_from_head);
  void UpdateProjectionMatrixFromFoV(float up_rad,
                                     float down_rad,
                                     float left_rad,
                                     float right_rad,
                                     float near_depth,
                                     float far_depth);
  void UpdateProjectionMatrixFromAspect(float fovy,
                                        float aspect,
                                        float near_depth,
                                        float far_depth);

  void SetHeadFromEyeTransform(const TransformationMatrix& head_from_eye);

  TransformationMatrix UnprojectPointer(double x,
                                        double y,
                                        double canvas_width,
                                        double canvas_height);

  XRView::XREye Eye() const { return eye_; }
  const TransformationMatrix& Transform() const { return ref_space_from_eye_; }
  const TransformationMatrix& ProjectionMatrix() const {
    return projection_matrix_;
  }

 private:
  const XRView::XREye eye_;
  TransformationMatrix ref_space_from_eye_;
  TransformationMatrix projection_matrix_;
  TransformationMatrix inv_projection_;
  TransformationMatrix head_from_eye_;
  bool inv_projection_dirty_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_VIEW_H_
