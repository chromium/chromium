// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_pose.h"

#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"

namespace blink {

XRPose::XRPose(const TransformationMatrix& pose_model_matrix,
               bool emulated_position)
    : transform_(MakeGarbageCollected<XRRigidTransform>(pose_model_matrix)),
      emulated_position_(emulated_position) {}

void XRPose::Trace(blink::Visitor* visitor) {
  visitor->Trace(transform_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
