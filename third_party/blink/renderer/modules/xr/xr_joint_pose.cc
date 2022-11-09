// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_joint_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"

namespace blink {

XRJointPose::XRJointPose(const gfx::Transform& transform, float radius)
    : XRPose(transform, /* emulatedPosition */ false), radius_(radius) {}

}  // namespace blink
