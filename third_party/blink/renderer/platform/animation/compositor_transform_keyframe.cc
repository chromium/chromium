// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_transform_keyframe.h"

#include <memory>

namespace blink {

CompositorTransformKeyframe::CompositorTransformKeyframe(
    double time,
    CompositorTransformOperations value,
    const TimingFunction& timing_function)
    : transform_keyframe_(
          gfx::TransformKeyframe::Create(base::Seconds(time),
                                         value.ReleaseGfxTransformOperations(),
                                         timing_function.CloneToCC())) {}

CompositorTransformKeyframe::~CompositorTransformKeyframe() = default;

base::TimeDelta CompositorTransformKeyframe::Time() const {
  return transform_keyframe_->Time();
}

const gfx::TimingFunction* CompositorTransformKeyframe::CcTimingFunction()
    const {
  return transform_keyframe_->timing_function();
}

std::unique_ptr<gfx::TransformKeyframe> CompositorTransformKeyframe::CloneToCC()
    const {
  return transform_keyframe_->Clone();
}

}  // namespace blink
