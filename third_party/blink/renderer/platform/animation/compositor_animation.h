// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "cc/animation/animation_delegate.h"
#include "cc/animation/scroll_timeline.h"
#include "cc/animation/single_keyframe_effect_animation.h"
#include "cc/animation/worklet_animation.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace cc {
class AnimationCurve;
}

namespace blink {

using CompositorScrollTimeline = cc::ScrollTimeline;

class CompositorAnimationDelegate;
class CompositorKeyframeModel;

// A compositor representation for Animation.
class PLATFORM_EXPORT CompositorAnimation : public cc::AnimationDelegate {
 public:
  static std::unique_ptr<CompositorAnimation> Create();
  static std::unique_ptr<CompositorAnimation> CreateWorkletAnimation(
      cc::WorkletAnimationId,
      const String& name,
      double playback_rate,
      std::unique_ptr<CompositorScrollTimeline>,
      std::unique_ptr<cc::AnimationOptions>,
      std::unique_ptr<cc::AnimationEffectTimings> effect_timings);

  explicit CompositorAnimation(
      scoped_refptr<cc::SingleKeyframeEffectAnimation>);
  ~CompositorAnimation() override;

  cc::SingleKeyframeEffectAnimation* CcAnimation() const;

  // An animation delegate is notified when animations are started and stopped.
  // The CompositorAnimation does not take ownership of the delegate, and
  // it is the responsibility of the client to reset the layer's delegate before
  // deleting the delegate.
  void SetAnimationDelegate(CompositorAnimationDelegate*);

  void AttachElement(const CompositorElementId&);
  void DetachElement();
  bool IsElementAttached() const;

  void AddKeyframeModel(std::unique_ptr<CompositorKeyframeModel>);
  void RemoveKeyframeModel(int keyframe_model_id);
  void PauseKeyframeModel(int keyframe_model_id, double time_offset);
  void AbortKeyframeModel(int keyframe_model_id);

  void UpdateScrollTimeline(base::Optional<cc::ElementId>,
                            base::Optional<double> start_scroll_offset,
                            base::Optional<double> end_scroll_offset);
  void UpdatePlaybackRate(double playback_rate);

 private:
  // cc::AnimationDelegate implementation.
  void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override;
  void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                               int target_property,
                               int group) override;
  void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                              int target_property,
                              int group) override;
  void NotifyAnimationTakeover(base::TimeTicks monotonic_time,
                               int target_property,
                               base::TimeTicks animation_start_time,
                               std::unique_ptr<cc::AnimationCurve>) override;
  void NotifyLocalTimeUpdated(
      base::Optional<base::TimeDelta> local_time) override;

  scoped_refptr<cc::SingleKeyframeEffectAnimation> animation_;
  CompositorAnimationDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CompositorAnimation);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_ANIMATION_COMPOSITOR_ANIMATION_H_
