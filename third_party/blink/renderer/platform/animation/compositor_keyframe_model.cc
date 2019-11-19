// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_keyframe_model.h"

#include <memory>
#include "base/memory/ptr_util.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_color_animation_curve.h"
#include "third_party/blink/renderer/platform/animation/compositor_float_animation_curve.h"

using cc::KeyframeModel;
using cc::AnimationIdProvider;

using blink::CompositorAnimationCurve;
using blink::CompositorKeyframeModel;

namespace blink {

CompositorKeyframeModel::CompositorKeyframeModel(
    const CompositorAnimationCurve& curve,
    compositor_target_property::Type target_property,
    int keyframe_model_id,
    int group_id,
    const AtomicString& custom_property_name) {
  if (!keyframe_model_id)
    keyframe_model_id = AnimationIdProvider::NextKeyframeModelId();
  if (!group_id)
    group_id = AnimationIdProvider::NextGroupId();

  keyframe_model_ = KeyframeModel::Create(
      curve.CloneToAnimationCurve(), keyframe_model_id, group_id,
      target_property, custom_property_name.Utf8().data());
}

CompositorKeyframeModel::~CompositorKeyframeModel() = default;

int CompositorKeyframeModel::Id() const {
  return keyframe_model_->id();
}

int CompositorKeyframeModel::Group() const {
  return keyframe_model_->group();
}

compositor_target_property::Type CompositorKeyframeModel::TargetProperty()
    const {
  return static_cast<compositor_target_property::Type>(
      keyframe_model_->target_property_id());
}

void CompositorKeyframeModel::SetElementId(CompositorElementId element_id) {
  keyframe_model_->set_element_id(element_id);
}

double CompositorKeyframeModel::Iterations() const {
  return keyframe_model_->iterations();
}

void CompositorKeyframeModel::SetIterations(double n) {
  keyframe_model_->set_iterations(n);
}

double CompositorKeyframeModel::IterationStart() const {
  return keyframe_model_->iteration_start();
}

void CompositorKeyframeModel::SetIterationStart(double iteration_start) {
  keyframe_model_->set_iteration_start(iteration_start);
}

double CompositorKeyframeModel::StartTime() const {
  return (keyframe_model_->start_time() - base::TimeTicks()).InSecondsF();
}

void CompositorKeyframeModel::SetStartTime(double monotonic_time) {
  keyframe_model_->set_start_time(base::TimeTicks::FromInternalValue(
      monotonic_time * base::Time::kMicrosecondsPerSecond));
}

void CompositorKeyframeModel::SetStartTime(base::TimeTicks monotonic_time) {
  keyframe_model_->set_start_time(monotonic_time);
}

double CompositorKeyframeModel::TimeOffset() const {
  return keyframe_model_->time_offset().InSecondsF();
}

void CompositorKeyframeModel::SetTimeOffset(double monotonic_time) {
  keyframe_model_->set_time_offset(
      base::TimeDelta::FromSecondsD(monotonic_time));
}

blink::CompositorKeyframeModel::Direction
CompositorKeyframeModel::GetDirection() const {
  return keyframe_model_->direction();
}

void CompositorKeyframeModel::SetDirection(Direction direction) {
  keyframe_model_->set_direction(direction);
}

double CompositorKeyframeModel::PlaybackRate() const {
  return keyframe_model_->playback_rate();
}

void CompositorKeyframeModel::SetPlaybackRate(double playback_rate) {
  keyframe_model_->set_playback_rate(playback_rate);
}

blink::CompositorKeyframeModel::FillMode CompositorKeyframeModel::GetFillMode()
    const {
  return keyframe_model_->fill_mode();
}

void CompositorKeyframeModel::SetFillMode(FillMode fill_mode) {
  keyframe_model_->set_fill_mode(fill_mode);
}

std::unique_ptr<cc::KeyframeModel>
CompositorKeyframeModel::ReleaseCcKeyframeModel() {
  keyframe_model_->set_needs_synchronized_start_time(true);
  return std::move(keyframe_model_);
}

std::unique_ptr<CompositorFloatAnimationCurve>
CompositorKeyframeModel::FloatCurveForTesting() const {
  const cc::AnimationCurve* curve = keyframe_model_->curve();
  DCHECK_EQ(cc::AnimationCurve::FLOAT, curve->Type());

  auto keyframed_curve = base::WrapUnique(
      static_cast<cc::KeyframedFloatAnimationCurve*>(curve->Clone().release()));
  return CompositorFloatAnimationCurve::CreateForTesting(
      std::move(keyframed_curve));
}

std::unique_ptr<CompositorColorAnimationCurve>
CompositorKeyframeModel::ColorCurveForTesting() const {
  const cc::AnimationCurve* curve = keyframe_model_->curve();
  DCHECK_EQ(cc::AnimationCurve::COLOR, curve->Type());

  auto keyframed_curve = base::WrapUnique(
      static_cast<cc::KeyframedColorAnimationCurve*>(curve->Clone().release()));
  return CompositorColorAnimationCurve::CreateForTesting(
      std::move(keyframed_curve));
}

}  // namespace blink
