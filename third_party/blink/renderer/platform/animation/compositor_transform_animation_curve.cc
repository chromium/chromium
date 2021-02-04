// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_transform_animation_curve.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "third_party/blink/renderer/platform/animation/compositor_transform_operations.h"
#include "ui/gfx/transform_operations.h"

namespace blink {

CompositorTransformAnimationCurve::CompositorTransformAnimationCurve()
    : curve_(cc::KeyframedTransformAnimationCurve::Create()) {}

CompositorTransformAnimationCurve::~CompositorTransformAnimationCurve() =
    default;

void CompositorTransformAnimationCurve::AddKeyframe(
    const CompositorTransformKeyframe& keyframe) {
  curve_->AddKeyframe(keyframe.CloneToCC());
}

void CompositorTransformAnimationCurve::SetTimingFunction(
    const TimingFunction& timing_function) {
  curve_->SetTimingFunction(timing_function.CloneToCC());
}

void CompositorTransformAnimationCurve::SetScaledDuration(
    double scaled_duration) {
  curve_->set_scaled_duration(scaled_duration);
}

std::unique_ptr<cc::AnimationCurve>
CompositorTransformAnimationCurve::CloneToAnimationCurve() const {
  return curve_->Clone();
}

}  // namespace blink
