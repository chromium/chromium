// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animation_data.h"

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/animation/timing.h"

namespace blink {

// static
bool CSSAnimationData::TimelineTriggerDataChanged(
    const CSSAnimationData* old_data,
    const CSSAnimationData* new_data) {
  if (old_data && new_data) {
    return !old_data->TriggersMatchForStyleRecalc(*new_data);
  } else if (old_data || new_data) {
    // If one of the ComputedStyles didn't have CSSAnimationData and the other
    // did, the other is only meaningfully different if it declared a named
    // trigger.
    const CSSAnimationData* data = new_data ? new_data : old_data;
    return std::any_of(data->TimelineTriggerNameList().begin(),
                       data->TimelineTriggerNameList().end(),
                       [](Member<const ScopedCSSName> trigger_name) {
                         return trigger_name.Get();
                       });
  }
  return false;
}

CSSAnimationData::CSSAnimationData() : CSSTimingData(InitialDuration()) {
  name_list_.push_back(InitialName());
  timeline_list_.push_back(InitialTimeline());
  iteration_count_list_.push_back(InitialIterationCount());
  direction_list_.push_back(InitialDirection());
  fill_mode_list_.push_back(InitialFillMode());
  play_state_list_.push_back(InitialPlayState());
  range_start_list_.push_back(InitialRangeStart());
  range_end_list_.push_back(InitialRangeEnd());
  composition_list_.push_back(InitialComposition());
  timeline_trigger_name_list_.push_back(InitialTimelineTriggerName());
  timeline_trigger_source_list_.push_back(InitialTimelineTriggerSource());
  timeline_trigger_activation_range_start_list_.push_back(
      InitialTimelineTriggerActivationRangeStart());
  timeline_trigger_activation_range_end_list_.push_back(
      InitialTimelineTriggerActivationRangeEnd());
  timeline_trigger_active_range_start_list_.push_back(
      InitialTimelineTriggerActiveRangeStart());
  timeline_trigger_active_range_end_list_.push_back(
      InitialTimelineTriggerActiveRangeEnd());
  trigger_attachments_list_.push_back(InitialTriggerAttachments());
}

CSSAnimationData::CSSAnimationData(const CSSAnimationData& other) = default;

std::optional<double> CSSAnimationData::InitialDuration() {
  return std::nullopt;
}

const AtomicString& CSSAnimationData::InitialName() {
  DEFINE_STATIC_LOCAL(const AtomicString, name, (""));
  return name;
}

const StyleTimeline& CSSAnimationData::InitialTimeline() {
  DEFINE_STATIC_LOCAL(const StyleTimeline, timeline, (CSSValueID::kAuto));
  return timeline;
}

const StyleTimeline& CSSAnimationData::InitialTimelineTriggerSource() {
  DEFINE_STATIC_LOCAL(const StyleTimeline, timeline_trigger_source,
                      (CSSValueID::kAuto));
  return timeline_trigger_source;
}

bool CSSAnimationData::AnimationsMatchForStyleRecalc(
    const CSSAnimationData& other) const {
  return name_list_ == other.name_list_ &&
         timeline_list_ == other.timeline_list_ &&
         play_state_list_ == other.play_state_list_ &&
         iteration_count_list_ == other.iteration_count_list_ &&
         direction_list_ == other.direction_list_ &&
         fill_mode_list_ == other.fill_mode_list_ &&
         range_start_list_ == other.range_start_list_ &&
         range_end_list_ == other.range_end_list_ &&
         TimingMatchForStyleRecalc(other) && TriggersMatchForStyleRecalc(other);
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

const StyleTimeline& CSSAnimationData::GetTimelineTriggerSource(
    size_t index) const {
  DCHECK_LT(index, timeline_trigger_name_list_.size());
  return GetRepeated(timeline_trigger_source_list_, index);
}

const Member<const StyleTriggerAttachmentVector>
CSSAnimationData::GetTriggerAttachments(size_t index) const {
  DCHECK_LT(index, name_list_.size());
  return (index < trigger_attachments_list_.size())
             ? trigger_attachments_list_.at(index)
             : nullptr;
}

bool CSSAnimationData::TimelineTriggerNamesMatch(
    const CSSAnimationData& other) const {
  if (TimelineTriggerNameList().size() !=
      other.TimelineTriggerNameList().size()) {
    return false;
  }

  for (wtf_size_t i = 0; i < TimelineTriggerNameList().size(); i++) {
    if (!base::ValuesEquivalent(TimelineTriggerNameList().at(i),
                                other.TimelineTriggerNameList().at(i))) {
      return false;
    }
  }

  return true;
}

bool CSSAnimationData::TriggersMatchForStyleRecalc(
    const CSSAnimationData& other) const {
  return TimelineTriggerNamesMatch(other) &&
         (other.TimelineTriggerSourceList() == TimelineTriggerSourceList()) &&
         (other.TimelineTriggerActivationRangeStartList() ==
          TimelineTriggerActivationRangeStartList()) &&
         (other.TimelineTriggerActivationRangeEndList() ==
          TimelineTriggerActivationRangeEndList()) &&
         (other.TimelineTriggerActiveRangeStartList() ==
          TimelineTriggerActiveRangeStartList()) &&
         (other.TimelineTriggerActiveRangeEndList() ==
          TimelineTriggerActiveRangeEndList());
}

}  // namespace blink
