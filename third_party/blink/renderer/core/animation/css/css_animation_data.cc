// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"

#include "third_party/blink/renderer/core/animation/timing.h"

namespace blink {

CSSAnimationData::CSSAnimationData() {
  name_list_.push_back(InitialName());
  timeline_list_.push_back(InitialTimeline());
  iteration_count_list_.push_back(InitialIterationCount());
  direction_list_.push_back(InitialDirection());
  fill_mode_list_.push_back(InitialFillMode());
  play_state_list_.push_back(InitialPlayState());
}

CSSAnimationData::CSSAnimationData(const CSSAnimationData& other) = default;

const AtomicString& CSSAnimationData::InitialName() {
  DEFINE_STATIC_LOCAL(const AtomicString, name, ("none"));
  return name;
}

const StyleTimeline& CSSAnimationData::InitialTimeline() {
  DEFINE_STATIC_LOCAL(const StyleTimeline, timeline, (CSSValueID::kAuto));
  return timeline;
}

bool CSSAnimationData::AnimationsMatchForStyleRecalc(
    const CSSAnimationData& other) const {
  return name_list_ == other.name_list_ &&
         timeline_list_ == other.timeline_list_ &&
         play_state_list_ == other.play_state_list_ &&
         iteration_count_list_ == other.iteration_count_list_ &&
         direction_list_ == other.direction_list_ &&
         fill_mode_list_ == other.fill_mode_list_ &&
         TimingMatchForStyleRecalc(other);
}

Timing CSSAnimationData::ConvertToTiming(size_t index) const {
  DCHECK_LT(index, name_list_.size());
  Timing timing = CSSTimingData::ConvertToTiming(index);
  timing.iteration_count = GetRepeated(iteration_count_list_, index);
  timing.direction = GetRepeated(direction_list_, index);
  timing.fill_mode = GetRepeated(fill_mode_list_, index);
  timing.AssertValid();
  return timing;
}

const StyleTimeline& CSSAnimationData::GetTimeline(size_t index) const {
  DCHECK_LT(index, name_list_.size());
  return GetRepeated(timeline_list_, index);
}

}  // namespace blink
