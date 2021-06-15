// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_TRANSFORM_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_TRANSFORM_KEYFRAME_H_

#include "third_party/blink/renderer/platform/animation/compositor_keyframe.h"
#include "third_party/blink/renderer/platform/animation/compositor_transform_operations.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace blink {

class PLATFORM_EXPORT CompositorTransformKeyframe : public CompositorKeyframe {
 public:
  CompositorTransformKeyframe(double time,
                              CompositorTransformOperations value,
                              const TimingFunction&);
  CompositorTransformKeyframe(const CompositorTransformKeyframe&) = delete;
  CompositorTransformKeyframe& operator=(const CompositorTransformKeyframe&) =
      delete;
  ~CompositorTransformKeyframe() override;

  std::unique_ptr<gfx::TransformKeyframe> CloneToCC() const;

  // CompositorKeyframe implementation.
  double Time() const override;
  const gfx::TimingFunction* CcTimingFunction() const override;

 private:
  std::unique_ptr<gfx::TransformKeyframe> transform_keyframe_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_TRANSFORM_KEYFRAME_H_
