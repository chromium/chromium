// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_SCROLL_OFFSET_ANIMATION_CURVE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_SCROLL_OFFSET_ANIMATION_CURVE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/animation/scroll_offset_animation_curve_factory.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_curve.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace cc {
class ScrollOffsetAnimationCurve;
}

namespace blink {

class PLATFORM_EXPORT CompositorScrollOffsetAnimationCurve
    : public CompositorAnimationCurve {
 public:
  using ScrollType = cc::ScrollOffsetAnimationCurveFactory::ScrollType;

  CompositorScrollOffsetAnimationCurve(FloatPoint, ScrollType);
  explicit CompositorScrollOffsetAnimationCurve(
      cc::ScrollOffsetAnimationCurve*);

  ~CompositorScrollOffsetAnimationCurve() override;

  void SetInitialValue(FloatPoint);
  FloatPoint GetValue(double time) const;
  double Duration() const;
  FloatPoint TargetValue() const;
  void ApplyAdjustment(IntSize);
  void UpdateTarget(base::TimeDelta time, FloatPoint new_target);

  // CompositorAnimationCurve implementation.
  std::unique_ptr<cc::AnimationCurve> CloneToAnimationCurve() const override;

 private:
  std::unique_ptr<cc::ScrollOffsetAnimationCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(CompositorScrollOffsetAnimationCurve);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_SCROLL_OFFSET_ANIMATION_CURVE_H_
