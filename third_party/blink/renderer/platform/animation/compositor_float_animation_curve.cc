// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/animation/keyframe/timing_function.h"

namespace blink {

CompositorFloatAnimationCurve::CompositorFloatAnimationCurve()
    : curve_(gfx::KeyframedFloatAnimationCurve::Create()) {}

CompositorFloatAnimationCurve::CompositorFloatAnimationCurve(
    std::unique_ptr<gfx::KeyframedFloatAnimationCurve> curve)
    : curve_(std::move(curve)) {}

CompositorFloatAnimationCurve::~CompositorFloatAnimationCurve() = default;

std::unique_ptr<CompositorFloatAnimationCurve>
CompositorFloatAnimationCurve::CreateForTesting(
    std::unique_ptr<gfx::KeyframedFloatAnimationCurve> curve) {
  return base::WrapUnique(new CompositorFloatAnimationCurve(std::move(curve)));
}

CompositorFloatAnimationCurve::Keyframes
CompositorFloatAnimationCurve::KeyframesForTesting() const {
  Keyframes keyframes;
  for (const auto& cc_keyframe : curve_->keyframes_for_testing()) {
    keyframes.push_back(
        base::WrapUnique(new CompositorFloatKeyframe(cc_keyframe->Clone())));
  }
  return keyframes;
}

scoped_refptr<TimingFunction>
CompositorFloatAnimationCurve::GetTimingFunctionForTesting() const {
  return CreateCompositorTimingFunctionFromCC(
      curve_->timing_function_for_testing());
}

void CompositorFloatAnimationCurve::AddKeyframe(
    const CompositorFloatKeyframe& keyframe) {
  curve_->AddKeyframe(keyframe.CloneToCC());
}

void CompositorFloatAnimationCurve::SetTimingFunction(
    const TimingFunction& timing_function) {
  curve_->SetTimingFunction(timing_function.CloneToCC());
}

void CompositorFloatAnimationCurve::SetScaledDuration(double scaled_duration) {
  curve_->set_scaled_duration(scaled_duration);
}

float CompositorFloatAnimationCurve::GetValue(double time) const {
  return curve_->GetValue(base::Seconds(time));
}

std::unique_ptr<gfx::AnimationCurve>
CompositorFloatAnimationCurve::CloneToAnimationCurve() const {
  return curve_->Clone();
}

}  // namespace blink
