// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_COLOR_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_COLOR_KEYFRAME_H_

#include "third_party/blink/renderer/platform/animation/compositor_keyframe.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace blink {

class TimingFunction;

class PLATFORM_EXPORT CompositorColorKeyframe : public CompositorKeyframe {
 public:
  CompositorColorKeyframe(double time, SkColor value, const TimingFunction&);
  CompositorColorKeyframe(std::unique_ptr<gfx::ColorKeyframe>);
  CompositorColorKeyframe(const CompositorColorKeyframe&) = delete;
  CompositorColorKeyframe& operator=(const CompositorColorKeyframe&) = delete;
  ~CompositorColorKeyframe() override;

  // CompositorKeyframe implementation.
  double Time() const override;
  const gfx::TimingFunction* CcTimingFunction() const override;

  SkColor Value() { return color_keyframe_->Value(); }
  std::unique_ptr<gfx::ColorKeyframe> CloneToCC() const;

 private:
  std::unique_ptr<gfx::ColorKeyframe> color_keyframe_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_COLOR_KEYFRAME_H_
