// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_COLOR_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_COLOR_KEYFRAME_H_

#include "base/macros.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class TimingFunction;

class PLATFORM_EXPORT CompositorColorKeyframe : public CompositorKeyframe {
 public:
  CompositorColorKeyframe(double time, SkColor value, const TimingFunction&);
  CompositorColorKeyframe(std::unique_ptr<cc::ColorKeyframe>);
  ~CompositorColorKeyframe() override;

  // CompositorKeyframe implementation.
  double Time() const override;
  const cc::TimingFunction* CcTimingFunction() const override;

  SkColor Value() { return color_keyframe_->Value(); }
  std::unique_ptr<cc::ColorKeyframe> CloneToCC() const;

 private:
  std::unique_ptr<cc::ColorKeyframe> color_keyframe_;

  DISALLOW_COPY_AND_ASSIGN(CompositorColorKeyframe);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_COLOR_KEYFRAME_H_
