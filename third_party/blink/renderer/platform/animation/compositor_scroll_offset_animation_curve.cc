// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_scroll_offset_animation_curve.h"

#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scroll_offset_animation_curve_factory.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/geometry/size.h"

using blink::CompositorScrollOffsetAnimationCurve;

namespace blink {

CompositorScrollOffsetAnimationCurve::CompositorScrollOffsetAnimationCurve(
    gfx::PointF target_value,
    ScrollType scroll_type)
    : curve_(cc::ScrollOffsetAnimationCurveFactory::CreateAnimation(
          target_value,
          scroll_type)) {}

CompositorScrollOffsetAnimationCurve::CompositorScrollOffsetAnimationCurve(
    cc::ScrollOffsetAnimationCurve* curve)
    : curve_(curve->CloneToScrollOffsetAnimationCurve()) {}

CompositorScrollOffsetAnimationCurve::~CompositorScrollOffsetAnimationCurve() =
    default;

void CompositorScrollOffsetAnimationCurve::SetInitialValue(
    gfx::PointF initial_value) {
  curve_->SetInitialValue(initial_value);
}

gfx::PointF CompositorScrollOffsetAnimationCurve::GetValue(double time) const {
  return curve_->GetValue(base::Seconds(time));
}

void CompositorScrollOffsetAnimationCurve::ApplyAdjustment(
    gfx::Vector2d adjustment) {
  curve_->ApplyAdjustment(adjustment);
}

base::TimeDelta CompositorScrollOffsetAnimationCurve::Duration() const {
  return curve_->Duration();
}

gfx::PointF CompositorScrollOffsetAnimationCurve::TargetValue() const {
  return curve_->target_value();
}

void CompositorScrollOffsetAnimationCurve::UpdateTarget(
    base::TimeDelta time,
    gfx::PointF new_target) {
  curve_->UpdateTarget(time, new_target);
}

std::unique_ptr<gfx::AnimationCurve>
CompositorScrollOffsetAnimationCurve::CloneToAnimationCurve() const {
  return curve_->Clone();
}

}  // namespace blink
