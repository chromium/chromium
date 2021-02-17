// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_FLOAT_ANIMATION_CURVE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_FLOAT_ANIMATION_CURVE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_keyframe.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
class KeyframedFloatAnimationCurve;
}

namespace blink {
class CompositorFloatKeyframe;
}

namespace blink {

// A keyframed float animation curve.
class PLATFORM_EXPORT CompositorFloatAnimationCurve
    : public CompositorAnimationCurve {
 public:
  CompositorFloatAnimationCurve();
  ~CompositorFloatAnimationCurve() override;

  void AddKeyframe(const CompositorFloatKeyframe&);
  void SetTimingFunction(const TimingFunction&);
  void SetScaledDuration(double);
  float GetValue(double time) const;

  // CompositorAnimationCurve implementation.
  std::unique_ptr<cc::AnimationCurve> CloneToAnimationCurve() const override;

  static std::unique_ptr<CompositorFloatAnimationCurve> CreateForTesting(
      std::unique_ptr<cc::KeyframedFloatAnimationCurve>);

  using Keyframes = Vector<std::unique_ptr<CompositorFloatKeyframe>>;
  Keyframes KeyframesForTesting() const;

  scoped_refptr<TimingFunction> GetTimingFunctionForTesting() const;

 private:
  CompositorFloatAnimationCurve(
      std::unique_ptr<cc::KeyframedFloatAnimationCurve>);

  std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(CompositorFloatAnimationCurve);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_FLOAT_ANIMATION_CURVE_H_
