// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_hit_test_result.h"

#include "third_party/blink/renderer/modules/xr/xr_hit_test_source.h"
#include "third_party/blink/renderer/modules/xr/xr_pose.h"
#include "third_party/blink/renderer/modules/xr/xr_space.h"

namespace blink {

XRHitTestResult::XRHitTestResult(XRHitTestSource* hit_test_source,
                                 const TransformationMatrix& pose)
    : hit_test_source_(hit_test_source),
      pose_(std::make_unique<TransformationMatrix>(pose)) {}

XRPose* XRHitTestResult::getPose(XRSpace* relative_to) {
  DCHECK(relative_to->MojoFromSpace());

  auto mojo_from_this = *pose_;

  auto mojo_from_other = *relative_to->MojoFromSpace();
  DCHECK(mojo_from_other.IsInvertible());

  auto other_from_mojo = mojo_from_other.Inverse();

  auto other_from_this = other_from_mojo * mojo_from_this;

  return MakeGarbageCollected<XRPose>(other_from_this, false);
}

void XRHitTestResult::Trace(blink::Visitor* visitor) {
  visitor->Trace(hit_test_source_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
