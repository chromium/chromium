// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_animation.h"

#include "cc/animation/animation_id_provider.h"
#include "cc/animation/animation_timeline.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_delegate.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

std::unique_ptr<CompositorAnimation> CompositorAnimation::Create(
    std::optional<int> replaced_cc_animation_id) {
  auto compositor_animation = std::make_unique<CompositorAnimation>(
      cc::Animation::Create(replaced_cc_animation_id
                                ? *replaced_cc_animation_id
                                : cc::AnimationIdProvider::NextAnimationId()));
  if (replaced_cc_animation_id) {
    compositor_animation->CcAnimation()->set_is_replacement();
  }
  return compositor_animation;
}

std::unique_ptr<CompositorAnimation>
CompositorAnimation::CreateWorkletAnimation(
    cc::WorkletAnimationId worklet_animation_id,
    const String& name,
    double playback_rate,
    std::unique_ptr<cc::AnimationOptions> options,
    std::unique_ptr<cc::AnimationEffectTimings> effect_timings) {
  return std::make_unique<CompositorAnimation>(cc::WorkletAnimation::Create(
      worklet_animation_id, name.Utf8(), playback_rate, std::move(options),
      std::move(effect_timings)));
}

CompositorAnimation::CompositorAnimation(scoped_refptr<cc::Animation> animation)
    : animation_(animation), delegate_() {}

CompositorAnimation::~CompositorAnimation() {
  SetAnimationDelegate(nullptr);
  // Detach animation from timeline, otherwise it stays there (leaks) until
  // compositor shutdown.
  if (animation_->animation_timeline())
    animation_->animation_timeline()->DetachAnimation(animation_);
}

cc::Animation* CompositorAnimation::CcAnimation() const {
  return animation_.get();
}

int CompositorAnimation::CcAnimationId() const {
  CHECK(CcAnimation());
  return CcAnimation()->id();
}

void CompositorAnimation::SetAnimationDelegate(
    CompositorAnimationDelegate* delegate) {
  delegate_ = delegate;
  animation_->set_animation_delegate(delegate ? this : nullptr);
}

void CompositorAnimation::AttachElement(const CompositorElementId& id) {
  animation_->AttachElement(id);
}

void CompositorAnimation::AttachPaintWorkletElement() {
  animation_->AttachPaintWorkletElement();
}

void CompositorAnimation::DetachElement() {
  animation_->DetachElement();
}

bool CompositorAnimation::IsElementAttached() const {
  return !!animation_->element_id();
}

void CompositorAnimation::AddKeyframeModel(
    std::unique_ptr<cc::KeyframeModel> keyframe_model) {
  keyframe_model->set_needs_synchronized_start_time(true);
  animation_->AddKeyframeModel(std::move(keyframe_model));
}

void CompositorAnimation::RemoveKeyframeModel(int keyframe_model_id) {
  animation_->RemoveKeyframeModel(keyframe_model_id);
}

void CompositorAnimation::PauseKeyframeModel(int keyframe_model_id,
                                             base::TimeDelta time_offset) {
  animation_->PauseKeyframeModel(keyframe_model_id, time_offset);
}

void CompositorAnimation::AbortKeyframeModel(int keyframe_model_id) {
  animation_->AbortKeyframeModel(keyframe_model_id);
}

void CompositorAnimation::UpdatePlaybackRate(double playback_rate) {
  cc::ToWorkletAnimation(animation_.get())->UpdatePlaybackRate(playback_rate);
}

void CompositorAnimation::NotifyAnimationStarted(base::TimeTicks monotonic_time,
                                                 int target_property,
                                                 int group) {
  if (delegate_) {
    delegate_->NotifyAnimationStarted(monotonic_time - base::TimeTicks(),
                                      group);
  }
}

void CompositorAnimation::NotifyAnimationFinished(
    base::TimeTicks monotonic_time,
    int target_property,
    int group) {
  if (delegate_) {
    delegate_->NotifyAnimationFinished(monotonic_time - base::TimeTicks(),
                                       group);
  }
}

void CompositorAnimation::NotifyAnimationAborted(base::TimeTicks monotonic_time,
                                                 int target_property,
                                                 int group) {
  if (delegate_) {
    delegate_->NotifyAnimationAborted(monotonic_time - base::TimeTicks(),
                                      group);
  }
}

void CompositorAnimation::NotifyAnimationTakeover(
    base::TimeTicks monotonic_time,
    int target_property,
    base::TimeTicks animation_start_time,
    std::unique_ptr<gfx::AnimationCurve> curve) {
  if (delegate_) {
    delegate_->NotifyAnimationTakeover(
        (monotonic_time - base::TimeTicks()).InSecondsF(),
        (animation_start_time - base::TimeTicks()).InSecondsF(),
        std::move(curve));
  }
}

void CompositorAnimation::NotifyLocalTimeUpdated(
    std::optional<base::TimeDelta> local_time) {
  if (delegate_) {
    delegate_->NotifyLocalTimeUpdated(local_time);
  }
}

}  // namespace blink
