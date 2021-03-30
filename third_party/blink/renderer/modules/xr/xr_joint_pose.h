// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_JOINT_POSE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_JOINT_POSE_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace blink {

class XRJointPose : public XRPose {
  DEFINE_WRAPPERTYPEINFO();

 public:
  XRJointPose(const TransformationMatrix& transform,
              float radius);

  float radius() const { return radius_; }

 private:
  float radius_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_JOINT_POSE_H_
