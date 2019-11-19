// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_COLOR_ANIMATION_CURVE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_COLOR_ANIMATION_CURVE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_color_keyframe.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class KeyframedColorAnimationCurve;
}

namespace blink {

class CompositorColorKeyframe;

// A keyframed color animation curve.
class PLATFORM_EXPORT CompositorColorAnimationCurve
    : public CompositorAnimationCurve {
 public:
  CompositorColorAnimationCurve();
  ~CompositorColorAnimationCurve() override;

  void AddKeyframe(const CompositorColorKeyframe&);
  void SetTimingFunction(const TimingFunction&);
  void SetScaledDuration(double);
  SkColor GetValue(double time) const;

  // CompositorAnimationCurve implementation.
  std::unique_ptr<cc::AnimationCurve> CloneToAnimationCurve() const override;

  static std::unique_ptr<CompositorColorAnimationCurve> CreateForTesting(
      std::unique_ptr<cc::KeyframedColorAnimationCurve>);

  using Keyframes = Vector<std::unique_ptr<CompositorColorKeyframe>>;
  Keyframes KeyframesForTesting() const;

 private:
  CompositorColorAnimationCurve(
      std::unique_ptr<cc::KeyframedColorAnimationCurve>);

  std::unique_ptr<cc::KeyframedColorAnimationCurve> curve_;

  DISALLOW_COPY_AND_ASSIGN(CompositorColorAnimationCurve);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_COLOR_ANIMATION_CURVE_H_
