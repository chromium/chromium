// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_color_animation_curve.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"

namespace blink {

CompositorColorAnimationCurve::CompositorColorAnimationCurve()
    : curve_(cc::KeyframedColorAnimationCurve::Create()) {}

CompositorColorAnimationCurve::CompositorColorAnimationCurve(
    std::unique_ptr<cc::KeyframedColorAnimationCurve> curve)
    : curve_(std::move(curve)) {}

CompositorColorAnimationCurve::~CompositorColorAnimationCurve() = default;

std::unique_ptr<CompositorColorAnimationCurve>
CompositorColorAnimationCurve::CreateForTesting(
    std::unique_ptr<cc::KeyframedColorAnimationCurve> curve) {
  return base::WrapUnique(new CompositorColorAnimationCurve(std::move(curve)));
}

CompositorColorAnimationCurve::Keyframes
CompositorColorAnimationCurve::KeyframesForTesting() const {
  Keyframes keyframes;
  for (const auto& cc_keyframe : curve_->keyframes_for_testing()) {
    keyframes.push_back(
        base::WrapUnique(new CompositorColorKeyframe(cc_keyframe->Clone())));
  }
  return keyframes;
}

void CompositorColorAnimationCurve::AddKeyframe(
    const CompositorColorKeyframe& keyframe) {
  curve_->AddKeyframe(keyframe.CloneToCC());
}

void CompositorColorAnimationCurve::SetTimingFunction(
    const TimingFunction& timing_function) {
  curve_->SetTimingFunction(timing_function.CloneToCC());
}

void CompositorColorAnimationCurve::SetScaledDuration(double scaled_duration) {
  curve_->set_scaled_duration(scaled_duration);
}

SkColor CompositorColorAnimationCurve::GetValue(double time) const {
  return curve_->GetValue(base::TimeDelta::FromSecondsD(time));
}

std::unique_ptr<cc::AnimationCurve>
CompositorColorAnimationCurve::CloneToAnimationCurve() const {
  return curve_->Clone();
}

}  // namespace blink
