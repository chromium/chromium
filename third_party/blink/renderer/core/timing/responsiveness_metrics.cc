// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/responsiveness_metrics.h"

#include "base/rand_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace blink {

namespace {
// Minimum potentially generated value for UKM sampling.
constexpr int kMinValueForSampling = 1;
// Maximum potentially generated value for UKM sampling.
constexpr int kMaxValueForSampling = 100;
// UKM sampling rate. The sampling strategy is 1/N.
constexpr int kUkmSamplingRate = 10;

base::TimeDelta MaxEventDuration(
    WTF::Vector<ResponsivenessMetrics::EventTimestamps> timestamps) {
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
    WTF::Vector<ResponsivenessMetrics::EventTimestamps> timestamps) {
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
    base::TimeDelta max_event_duration,
    base::TimeDelta total_event_duration) {
  if (!window)
    return;

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

void ResponsivenessMetrics::RecordPerInteractionLatency(
    LocalDOMWindow* window,
    const AtomicString& event_type,
    absl::optional<int> key_code,
    absl::optional<PointerId> pointer_id,
    EventTimestamps event_timestamps) {
  // Keyboard interactions.
  if (key_code.has_value()) {
    RecordKeyboardInteractions(window, event_type, key_code.value(),
                               event_timestamps);
  }
  // Tap(Click) or Drag.
  if (pointer_id.has_value()) {
    RecordTapOrClickOrDrag(window, event_type, event_timestamps);
  }
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
                             MaxEventDuration(timestamps),
                             TotalEventDuration(timestamps));
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
                             MaxEventDuration(timestamps),
                             TotalEventDuration(timestamps));
    ResetPendingPointers();
  }
}

void ResponsivenessMetrics::RecordKeyboardInteractions(
    LocalDOMWindow* window,
    const AtomicString& event_type,
    int key_code,
    EventTimestamps event_timestamps) {
  if (event_type == event_type_names::kKeydown) {
    if (key_down_timestamps_map_.find(key_code) !=
        key_down_timestamps_map_.end()) {
      // Found a previous key_down with the same keycode, which means a key is
      // being held down. We regard the duration of the keydown as an
      // interaction level latency.
      EventTimestamps key_down_timestamps =
          key_down_timestamps_map_.at(key_code);
      base::TimeDelta event_duration =
          key_down_timestamps.end_time - key_down_timestamps.start_time;
      RecordUserInteractionUKM(window, UserInteractionType::kKeyboard,
                               event_duration, event_duration);
    }
    key_down_timestamps_map_[key_code] = event_timestamps;
  } else if (event_type == event_type_names::kKeyup) {
    if (key_down_timestamps_map_.find(key_code) !=
        key_down_timestamps_map_.end()) {
      // Found a previous key_down with the same keycode as keyup.
      // We calculate the interaction latency based on the durations of keydown
      // and keyup.
      EventTimestamps key_down_timestamps =
          key_down_timestamps_map_.at(key_code);
      WTF::Vector<EventTimestamps> timestamps;
      // Insertion order matters for latency computation.
      timestamps.push_back(key_down_timestamps);
      timestamps.push_back(event_timestamps);
      RecordUserInteractionUKM(window, UserInteractionType::kKeyboard,
                               MaxEventDuration(timestamps),
                               TotalEventDuration(timestamps));
      // Remove the stale keydown.
      key_down_timestamps_map_.erase(key_code);
    } else {
      // Can't find a corresponding keydown. We regard the duration of the keyup
      // as an interaction latency.
      base::TimeDelta event_duration =
          event_timestamps.end_time - event_timestamps.start_time;
      RecordUserInteractionUKM(window, UserInteractionType::kKeyboard,
                               event_duration, event_duration);
    }
  }
}

}  // namespace blink
