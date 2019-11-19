// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_KEYFRAME_MODEL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_KEYFRAME_MODEL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/animation/keyframe_model.h"
#include "third_party/blink/renderer/platform/animation/compositor_target_property.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace cc {
class KeyframeModel;
}

namespace blink {

class CompositorAnimationCurve;
class CompositorFloatAnimationCurve;
class CompositorColorAnimationCurve;

// A compositor driven animation.
class PLATFORM_EXPORT CompositorKeyframeModel {
  USING_FAST_MALLOC(CompositorKeyframeModel);

 public:
  using Direction = cc::KeyframeModel::Direction;
  using FillMode = cc::KeyframeModel::FillMode;

  // The |custom_property_name| has a default value of an empty string,
  // indicating that the animated property is a native property. When it is an
  // animated custom property, it should be the property name.
  CompositorKeyframeModel(const CompositorAnimationCurve&,
                          compositor_target_property::Type,
                          int keyframe_model_id,
                          int group_id,
                          const AtomicString& custom_property_name = "");
  ~CompositorKeyframeModel();

  // An id must be unique.
  int Id() const;
  int Group() const;

  compositor_target_property::Type TargetProperty() const;

  void SetElementId(CompositorElementId element_id);

  // This is the number of times that the animation will play. If this
  // value is zero the animation will not play. If it is negative, then
  // the animation will loop indefinitely.
  double Iterations() const;
  void SetIterations(double);

  double StartTime() const;
  void SetStartTime(double monotonic_time);
  void SetStartTime(base::TimeTicks);

  double TimeOffset() const;
  void SetTimeOffset(double monotonic_time);

  Direction GetDirection() const;
  void SetDirection(Direction);

  double PlaybackRate() const;
  void SetPlaybackRate(double);

  FillMode GetFillMode() const;
  void SetFillMode(FillMode);

  double IterationStart() const;
  void SetIterationStart(double);

  std::unique_ptr<cc::KeyframeModel> ReleaseCcKeyframeModel();

  std::unique_ptr<CompositorFloatAnimationCurve> FloatCurveForTesting() const;
  std::unique_ptr<CompositorColorAnimationCurve> ColorCurveForTesting() const;

  const std::string& GetCustomPropertyNameForTesting() const {
    return keyframe_model_->custom_property_name();
  }

 private:
  std::unique_ptr<cc::KeyframeModel> keyframe_model_;

  DISALLOW_COPY_AND_ASSIGN(CompositorKeyframeModel);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_KEYFRAME_MODEL_H_
