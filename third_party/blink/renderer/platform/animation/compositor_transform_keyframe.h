// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_TRANSFORM_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_TRANSFORM_KEYFRAME_H_

#include "base/macros.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_keyframe.h"
#include "third_party/blink/renderer/platform/animation/compositor_transform_operations.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT CompositorTransformKeyframe : public CompositorKeyframe {
 public:
  CompositorTransformKeyframe(double time,
                              CompositorTransformOperations value,
                              const TimingFunction&);
  ~CompositorTransformKeyframe() override;

  std::unique_ptr<cc::TransformKeyframe> CloneToCC() const;

  // CompositorKeyframe implementation.
  double Time() const override;
  const cc::TimingFunction* CcTimingFunction() const override;

 private:
  std::unique_ptr<cc::TransformKeyframe> transform_keyframe_;

  DISALLOW_COPY_AND_ASSIGN(CompositorTransformKeyframe);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_TRANSFORM_KEYFRAME_H_
