// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_scroll_offset_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"

#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/scroll_offset_animation_curve_factory.h"
#include "ui/gfx/animation/keyframe/timing_function.h"

using blink::CompositorScrollOffsetAnimationCurve;

namespace blink {

CompositorScrollOffsetAnimationCurve::CompositorScrollOffsetAnimationCurve(
    FloatPoint target_value,
    ScrollType scroll_type)
    : curve_(cc::ScrollOffsetAnimationCurveFactory::CreateAnimation(
          gfx::Vector2dF(target_value.X(), target_value.Y()),
          scroll_type)) {}

CompositorScrollOffsetAnimationCurve::CompositorScrollOffsetAnimationCurve(
    cc::ScrollOffsetAnimationCurve* curve)
    : curve_(curve->CloneToScrollOffsetAnimationCurve()) {}

CompositorScrollOffsetAnimationCurve::~CompositorScrollOffsetAnimationCurve() =
    default;

void CompositorScrollOffsetAnimationCurve::SetInitialValue(
    FloatPoint initial_value) {
  curve_->SetInitialValue(gfx::Vector2dF(initial_value.X(), initial_value.Y()));
}

FloatPoint CompositorScrollOffsetAnimationCurve::GetValue(double time) const {
  gfx::Vector2dF value = curve_->GetValue(base::Seconds(time));
  return FloatPoint(value.x(), value.y());
}

void CompositorScrollOffsetAnimationCurve::ApplyAdjustment(IntSize adjustment) {
  curve_->ApplyAdjustment(
      gfx::Vector2dF(adjustment.Width(), adjustment.Height()));
}

base::TimeDelta CompositorScrollOffsetAnimationCurve::Duration() const {
  return curve_->Duration();
}

FloatPoint CompositorScrollOffsetAnimationCurve::TargetValue() const {
  gfx::Vector2dF target = curve_->target_value();
  return FloatPoint(target.x(), target.y());
}

void CompositorScrollOffsetAnimationCurve::UpdateTarget(base::TimeDelta time,
                                                        FloatPoint new_target) {
  curve_->UpdateTarget(time, gfx::Vector2dF(new_target.X(), new_target.Y()));
}

std::unique_ptr<gfx::AnimationCurve>
CompositorScrollOffsetAnimationCurve::CloneToAnimationCurve() const {
  return curve_->Clone();
}

}  // namespace blink
