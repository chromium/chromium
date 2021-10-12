// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/responsiveness_metrics.h"

#include "base/rand_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"

namespace blink {

namespace {
// Minimum potentially generated value for UKM sampling.
constexpr int kMinValueForSampling = 1;
// Maximum potentially generated value for UKM sampling.
constexpr int kMaxValueForSampling = 100;
// UKM sampling rate. The sampling strategy is 1/N.
constexpr int kUkmSamplingRate = 10;

base::TimeDelta MaxEventDuration(
    const WTF::Vector<ResponsivenessMetrics::EventTimestamps>& timestamps) {
  DCHECK(timestamps.size());
  base::TimeDelta max_duration =
      timestamps[0].end_time - timestamps[0].start_time;
  for (WTF::wtf_size_t i = 1; i < timestamps.size(); ++i) {
    max_duration = std::max(max_duration,
                            timestamps[i].end_time - timestamps[i].start_time);
  }
  return max_duration;
}

base::TimeDelta TotalEventDuration(
    // timestamps is sorted by the start_time of EventTimestamps.
    const WTF::Vector<ResponsivenessMetrics::EventTimestamps>& timestamps) {
  DCHECK(timestamps.size());
  // TODO(crbug.com/1229668): Once the event timestamp bug is fixed, add a
  // DCHECK(IsSorted) here.
  base::TimeDelta total_duration =
      timestamps[0].end_time - timestamps[0].start_time;
  base::TimeTicks current_end_time = timestamps[0].end_time;
  for (WTF::wtf_size_t i = 1; i < timestamps.size(); ++i) {
    total_duration += timestamps[i].end_time - timestamps[i].start_time;
    if (timestamps[i].start_time < current_end_time) {
      total_duration -= std::min(current_end_time, timestamps[i].end_time) -
                        timestamps[i].start_time;
    }
    current_end_time = std::max(current_end_time, timestamps[i].end_time);
  }
  return total_duration;
}

}  // namespace

ResponsivenessMetrics::ResponsivenessMetrics() = default;
ResponsivenessMetrics::~ResponsivenessMetrics() = default;

void ResponsivenessMetrics::RecordUserInteractionUKM(
    LocalDOMWindow* window,
    UserInteractionType interaction_type,
    const WTF::Vector<ResponsivenessMetrics::EventTimestamps>& timestamps) {
  if (!window)
    return;

  for (ResponsivenessMetrics::EventTimestamps timestamp : timestamps) {
    if (timestamp.start_time == base::TimeTicks()) {
      return;
    }
  }

  base::TimeDelta max_event_duration = MaxEventDuration(timestamps);
  base::TimeDelta total_event_duration = TotalEventDuration(timestamps);
  // We found some negative values in the data. Before figuring out the root
  // cause, we need this check to avoid sending nonsensical data.
  if (max_event_duration.InMilliseconds() >= 0 &&
      total_event_duration.InMilliseconds() >= 0) {
    window->GetFrame()->Client()->DidObserveUserInteraction(
        max_event_duration, total_event_duration, interaction_type);
  }

  ukm::UkmRecorder* ukm_recorder = window->UkmRecorder();
  ukm::SourceId source_id = window->UkmSourceID();
  if (source_id != ukm::kInvalidSourceId &&
      (!sampling_ || base::RandInt(kMinValueForSampling,
                                   kMaxValueForSampling) <= kUkmSamplingRate)) {
    ukm::builders::Responsiveness_UserInteraction(source_id)
        .SetInteractionType(static_cast<int>(interaction_type))
        .SetMaxEventDuration(max_event_duration.InMilliseconds())
        .SetTotalEventDuration(total_event_duration.InMilliseconds())
        .Record(ukm_recorder);
  }
}

void ResponsivenessMetrics::NotifyPotentialDrag() {
  is_drag_ = pending_pointer_down_timestamps_.has_value() &&
             !pending_pointer_up_timestamps_.has_value();
}

void ResponsivenessMetrics::FlushPendingInteraction(LocalDOMWindow* window) {
  // For tap delay, the click can be dropped. We will measure the latency
  // without any click data.
  if (pending_pointer_down_timestamps_.has_value() &&
      pending_pointer_up_timestamps_.has_value()) {
    WTF::Vector<EventTimestamps> timestamps;
    // Insertion order matters for latency computation.
    timestamps.push_back(pending_pointer_down_timestamps_.value());
    timestamps.push_back(pending_pointer_up_timestamps_.value());
    RecordUserInteractionUKM(window,
                             is_drag_ ? UserInteractionType::kDrag
                                      : UserInteractionType::kTapOrClick,
                             timestamps);
  }
  ResetPendingPointers();
}

void ResponsivenessMetrics::ResetPendingPointers() {
  is_drag_ = false;
  pending_pointer_down_timestamps_.reset();
  pending_pointer_up_timestamps_.reset();
}

// For multi-finger touch, we record the innermost pair of pointerdown and
// pointerup.
// TODO(hbsong): Record one interaction per pointer id.
void ResponsivenessMetrics::RecordTapOrClickOrDrag(
    LocalDOMWindow* window,
    const AtomicString& event_type,
    EventTimestamps event_timestamps) {
  if (event_type == event_type_names::kPointercancel) {
    pending_pointer_down_timestamps_.reset();
  } else if (event_type == event_type_names::kPointerdown) {
    FlushPendingInteraction(window);
    pending_pointer_down_timestamps_ = event_timestamps;
  } else if (event_type == event_type_names::kPointerup &&
             pending_pointer_down_timestamps_.has_value() &&
             !pending_pointer_up_timestamps_.has_value()) {
    pending_pointer_up_timestamps_ = event_timestamps;
  } else if (event_type == event_type_names::kClick) {
    WTF::Vector<EventTimestamps> timestamps;
    // Insertion order matters for latency computation.
    if (pending_pointer_down_timestamps_.has_value()) {
      timestamps.push_back(pending_pointer_down_timestamps_.value());
    }
    if (pending_pointer_up_timestamps_.has_value()) {
      timestamps.push_back(pending_pointer_up_timestamps_.value());
    }
    timestamps.push_back(event_timestamps);
    RecordUserInteractionUKM(window,
                             is_drag_ ? UserInteractionType::kDrag
                                      : UserInteractionType::kTapOrClick,
                             timestamps);
    ResetPendingPointers();
  }
}

void ResponsivenessMetrics::RecordKeyboardInteractions(
    LocalDOMWindow* window,
    const WTF::Vector<EventTimestamps>& event_timestamps) {
  RecordUserInteractionUKM(window, UserInteractionType::kKeyboard,
                           event_timestamps);
}

}  // namespace blink
