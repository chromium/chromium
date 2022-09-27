// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/responsiveness_metrics.h"
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

namespace {
// Minimum potentially generated value for UKM sampling.
constexpr int kMinValueForSampling = 1;
// Maximum potentially generated value for UKM sampling.
constexpr int kMaxValueForSampling = 100;
// UKM sampling rate. The sampling strategy is 1/N.
constexpr int kUkmSamplingRate = 10;
// Minimum potential value for the first Interaction ID.
constexpr uint32_t kMinFirstInteractionID = 100;
// Maximum potential value for the first Interaction ID.
constexpr uint32_t kMaxFirstInteractionID = 10000;
// Interaction ID increment. We increase this value by an integer greater than 1
// to discourage developers from using the value to 'count' the number of user
// interactions. This is consistent with the spec, which allows the increasing
// the user interaction value by a small number chosen by the user agent.
constexpr uint32_t kInteractionIdIncrement = 7;
// The maximum tap delay we can handle for assigning interaction id.
constexpr blink::DOMHighResTimeStamp kMaxDelayForEntries =
    blink::DOMHighResTimeStamp(500);
// The length of the timer to flush entries from the time pointerup occurs.
constexpr base::TimeDelta kFlushTimerLength = base::Seconds(1);
// The name for the histogram which records interaction timings, and the names
// of the variants for keyboard, click/tap, and drag interactions.
const char kHistogramMaxEventDuration[] =
    "Blink.Responsiveness.UserInteraction.MaxEventDuration";
const char kHistogramAllTypes[] = ".AllTypes";
const char kHistogramKeyboard[] = ".Keyboard";
const char kHistogramTapOrClick[] = ".TapOrClick";
const char kHistogramDrag[] = ".Drag";

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

WTF::String InteractionTypeToString(UserInteractionType interaction_type) {
  switch (interaction_type) {
    case UserInteractionType::kDrag:
      return "drag";
    case UserInteractionType::kKeyboard:
      return "keyboard";
    case UserInteractionType::kTapOrClick:
      return "tapOrClick";
    default:
      NOTREACHED();
      return "";
  }
}

std::unique_ptr<TracedValue> UserInteractionTraceData(
    base::TimeDelta max_duration,
    base::TimeDelta total_duration,
    UserInteractionType interaction_type) {
  auto traced_value = std::make_unique<TracedValue>();
  traced_value->SetInteger("maxDuration",
                           static_cast<int>(max_duration.InMilliseconds()));
  traced_value->SetInteger("totalDuration",
                           static_cast<int>(total_duration.InMilliseconds()));
  traced_value->SetString("interactionType",
                          InteractionTypeToString(interaction_type));
  return traced_value;
}

void LogResponsivenessHistogram(base::TimeDelta max_event_duration,
                                const char* suffix) {
  base::UmaHistogramCustomTimes(
      base::StrCat({kHistogramMaxEventDuration, suffix}), max_event_duration,
      base::Milliseconds(1), base::Seconds(60), 50);
}

}  // namespace

ResponsivenessMetrics::ResponsivenessMetrics(
    WindowPerformance* window_performance)
    : window_performance_(window_performance),
      pointer_flush_timer_(window_performance_->task_runner_,
                           this,
                           &ResponsivenessMetrics::FlushPointerTimerFired),
      current_interaction_id_for_event_timing_(
          // Follow the spec by choosing a random integer as the initial value
          // to discourage developers from using interactionId to count the
          // number of interactions. See
          // https://wicg.github.io/event-timing/#user-interaction-value.
          base::RandInt(kMinFirstInteractionID, kMaxFirstInteractionID)) {}

ResponsivenessMetrics::~ResponsivenessMetrics() = default;

void ResponsivenessMetrics::RecordUserInteractionUKM(
    LocalDOMWindow* window,
    UserInteractionType interaction_type,
    const WTF::Vector<EventTimestamps>& timestamps) {
  if (!window)
    return;

  for (EventTimestamps timestamp : timestamps) {
    if (timestamp.start_time == base::TimeTicks()) {
      return;
    }
  }

  base::TimeDelta max_event_duration = MaxEventDuration(timestamps);
  base::TimeDelta total_event_duration = TotalEventDuration(timestamps);
  // We found some negative values in the data. Before figuring out the root
  // cause, we need this check to avoid sending nonsensical data.
  if (max_event_duration.InMilliseconds() >= 0) {
    window->GetFrame()->Client()->DidObserveUserInteraction(max_event_duration,
                                                            interaction_type);
  }
  TRACE_EVENT2("devtools.timeline", "Responsiveness.Renderer.UserInteraction",
               "data",
               UserInteractionTraceData(max_event_duration,
                                        total_event_duration, interaction_type),
               "frame", ToTraceValue(window->GetFrame()));

  LogResponsivenessHistogram(max_event_duration, kHistogramAllTypes);
  switch (interaction_type) {
    case UserInteractionType::kKeyboard:
      LogResponsivenessHistogram(max_event_duration, kHistogramKeyboard);
      break;
    case UserInteractionType::kTapOrClick:
      LogResponsivenessHistogram(max_event_duration, kHistogramTapOrClick);
      break;
    case UserInteractionType::kDrag:
      LogResponsivenessHistogram(max_event_duration, kHistogramDrag);
      break;
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

void ResponsivenessMetrics::NotifyPotentialDrag(PointerId pointer_id) {
  if (pointer_id_entry_map_.Contains(pointer_id))
    pointer_id_entry_map_.at(pointer_id)->SetIsDrag();
}

void ResponsivenessMetrics::RecordDragTapOrClickUKM(
    LocalDOMWindow* window,
    PointerEntryAndInfo& pointer_info) {
  DCHECK(pointer_info.GetEntry());
  // Early return if all we got was a pointerdown.
  if (pointer_info.GetEntry()->name() == event_type_names::kPointerdown &&
      pointer_info.GetTimeStamps().size() == 1u) {
    return;
  }
  RecordUserInteractionUKM(window,
                           pointer_info.IsDrag()
                               ? UserInteractionType::kDrag
                               : UserInteractionType::kTapOrClick,
                           pointer_info.GetTimeStamps());
}

// Event timing pointer events processing
//
// See also ./Pointer_interaction_state_machine.md to help understand the logic
// below that how event timing group up pointer events as interactions.
bool ResponsivenessMetrics::SetPointerIdAndRecordLatency(
    PerformanceEventTiming* entry,
    PointerId pointer_id,
    EventTimestamps event_timestamps) {
  const AtomicString& event_type = entry->name();
  auto* pointer_info = pointer_id_entry_map_.Contains(pointer_id)
                           ? pointer_id_entry_map_.at(pointer_id)
                           : nullptr;
  LocalDOMWindow* window = window_performance_->DomWindow();
  if (event_type == event_type_names::kPointercancel && pointer_info) {
    NotifyPointerdown(pointer_info->GetEntry());
    // The pointer id of the pointerdown is no longer needed.
    pointer_id_entry_map_.erase(pointer_id);
    last_pointer_id_ = absl::nullopt;
  } else if (event_type == event_type_names::kPointerdown) {
    if (pointer_info) {
      // Flush the existing entry. We are starting a new interaction.
      RecordDragTapOrClickUKM(window, *pointer_info);
      NotifyPointerdown(pointer_info->GetEntry());
      pointer_id_entry_map_.erase(pointer_id);
    }
    // Any existing entry in the map cannot fire a click.
    FlushPointerMap();
    if (pointer_flush_timer_.IsActive()) {
      pointer_flush_timer_.Stop();
    }
    pointer_id_entry_map_.Set(
        pointer_id, PointerEntryAndInfo::Create(entry, event_timestamps));

    // Waiting to see if we get a pointercancel or pointerup.
    last_pointer_id_ = pointer_id;
    return false;
  } else if (event_type == event_type_names::kPointerup) {
    // Generate a new interaction id.
    UpdateInteractionId();
    entry->SetInteractionId(GetCurrentInteractionId());
    if (pointer_info &&
        pointer_info->GetEntry()->name() == event_type_names::kPointerdown) {
      // Set interaction id and notify the pointer down entry.
      PerformanceEventTiming* pointer_down_entry = pointer_info->GetEntry();
      pointer_down_entry->SetInteractionId(GetCurrentInteractionId());
      NotifyPointerdown(pointer_down_entry);
      pointer_info->GetTimeStamps().push_back(event_timestamps);
    } else {
      // There is no matching pointerdown: Set the map using pointerup, in
      // case a click event shows up.
      pointer_id_entry_map_.Set(
          pointer_id, PointerEntryAndInfo::Create(entry, event_timestamps));
    }
    // Start the timer to flush the entry just created later, if needed.
    if (!pointer_flush_timer_.IsActive()) {
      pointer_flush_timer_.StartOneShot(kFlushTimerLength, FROM_HERE);
    }
    last_pointer_id_ = pointer_id;
  } else if (event_type == event_type_names::kClick) {
    // We do not rely on the |pointer_id| for clicks because they may be
    // inaccurate. Instead, we rely on the last pointer id seen.
    pointer_info = nullptr;
    if (last_pointer_id_.has_value() &&
        pointer_id_entry_map_.Contains(*last_pointer_id_)) {
      pointer_info = pointer_id_entry_map_.at(*last_pointer_id_);
    }
    if (pointer_info) {
      // There is a previous pointerdown or pointerup entry. Use its
      // interactionId.
      PerformanceEventTiming* previous_entry = pointer_info->GetEntry();
      // There are cases where we only see pointerdown and click, for instance
      // with contextmenu.
      if (previous_entry->interactionId() == 0u) {
        UpdateInteractionId();
        previous_entry->SetInteractionId(GetCurrentInteractionId());
      }
      entry->SetInteractionId(previous_entry->interactionId());
      pointer_info->GetTimeStamps().push_back(event_timestamps);
      RecordDragTapOrClickUKM(window, *pointer_info);
      // The pointer id of the pointerdown is no longer needed.
      pointer_id_entry_map_.erase(pointer_id);
    } else {
      // There is no previous pointerdown or pointerup entry. This can happen
      // when the user clicks using a non-pointer device. Generate a new
      // interactionId. No need to add to the map since this is the last event
      // in the interaction.
      UpdateInteractionId();
      entry->SetInteractionId(GetCurrentInteractionId());
      RecordDragTapOrClickUKM(
          window, *PointerEntryAndInfo::Create(entry, event_timestamps));
    }
    last_pointer_id_ = absl::nullopt;
  }
  return true;
}

void ResponsivenessMetrics::RecordKeyboardUKM(
    LocalDOMWindow* window,
    const WTF::Vector<EventTimestamps>& event_timestamps) {
  RecordUserInteractionUKM(window, UserInteractionType::kKeyboard,
                           event_timestamps);
}

bool ResponsivenessMetrics::SetKeyIdAndRecordLatency(
    PerformanceEventTiming* entry,
    absl::optional<int> key_code,
    EventTimestamps event_timestamps) {
  last_pointer_id_ = absl::nullopt;
  auto event_type = entry->name();
  if (event_type == event_type_names::kKeydown) {
    DCHECK(key_code.has_value());
    // During compositions, we ignore keydowns/keyups and look at input events.
    if (composition_started_)
      return true;

    if (key_code_entry_map_.Contains(*key_code)) {
      auto* previous_entry = key_code_entry_map_.at(*key_code);
      // Ignore repeat IME keydowns. See
      // https://w3c.github.io/uievents/#determine-keydown-keyup-keyCode.
      // Reasoning: we cannot ignore all IME keydowns because on Android in some
      // languages the events received are 'keydown', 'input', 'keyup', and
      // since we are not composing then the 'input' event is ignored, so we
      // must consider the key events with 229 keyCode as the user interaction.
      // Besides this, we cannot consider repeat 229 keydowns because we may get
      // those on ChromeOS when we should ignore them. This may be related to
      // crbug.com/1252856.
      if (*key_code != 229) {
        // Generate a new interaction id for |previous_entry|. This case could
        // be caused by keeping a key pressed for a while.
        UpdateInteractionId();
        previous_entry->GetEntry()->SetInteractionId(GetCurrentInteractionId());
        RecordKeyboardUKM(window_performance_->DomWindow(),
                          {previous_entry->GetTimeStamps()});
      }
      window_performance_->NotifyAndAddEventTimingBuffer(
          previous_entry->GetEntry());
    }
    key_code_entry_map_.Set(
        *key_code, KeyboardEntryAndTimestamps::Create(entry, event_timestamps));
    // Similar to pointerdown, we need to wait a bit before knowing the
    // interactionId of keydowns.
    return false;
  } else if (event_type == event_type_names::kKeyup) {
    DCHECK(key_code.has_value());
    if (composition_started_ || !key_code_entry_map_.Contains(*key_code))
      return true;

    auto* previous_entry = key_code_entry_map_.at(*key_code);
    // Generate a new interaction id for the keydown-keyup pair.
    UpdateInteractionId();
    previous_entry->GetEntry()->SetInteractionId(GetCurrentInteractionId());
    window_performance_->NotifyAndAddEventTimingBuffer(
        previous_entry->GetEntry());
    entry->SetInteractionId(GetCurrentInteractionId());
    RecordKeyboardUKM(window_performance_->DomWindow(),
                      {previous_entry->GetTimeStamps(), event_timestamps});
    key_code_entry_map_.erase(*key_code);
  } else if (event_type == event_type_names::kCompositionstart) {
    composition_started_ = true;
    for (auto key_entry : key_code_entry_map_.Values()) {
      window_performance_->NotifyAndAddEventTimingBuffer(key_entry->GetEntry());
    }
    key_code_entry_map_.clear();
  } else if (event_type == event_type_names::kCompositionend) {
    composition_started_ = false;
  } else if (event_type == event_type_names::kInput) {
    if (!composition_started_) {
      return true;
    }
    // We are in the case of a text input event while compositing with
    // non-trivial data, so we want to increase interactionId.
    // TODO(crbug.com/1252856): fix counts in ChromeOS due to duplicate events.
    UpdateInteractionId();
    entry->SetInteractionId(GetCurrentInteractionId());
    RecordKeyboardUKM(window_performance_->DomWindow(), {event_timestamps});
  }
  return true;
}

void ResponsivenessMetrics::MaybeFlushKeyboardEntries(
    DOMHighResTimeStamp current_time) {
  // We cannot delete from a HashMap while iterating.
  Vector<int> key_codes_to_remove;
  for (const auto& entry : key_code_entry_map_) {
    PerformanceEventTiming* key_down = entry.value->GetEntry();
    if (current_time - key_down->processingEnd() > kMaxDelayForEntries) {
      window_performance_->NotifyAndAddEventTimingBuffer(key_down);
      key_codes_to_remove.push_back(entry.key);
    }
  }
  key_code_entry_map_.RemoveAll(key_codes_to_remove);
}

void ResponsivenessMetrics::UpdateInteractionId() {
  current_interaction_id_for_event_timing_ += kInteractionIdIncrement;
}

uint32_t ResponsivenessMetrics::GetCurrentInteractionId() const {
  return current_interaction_id_for_event_timing_;
}

void ResponsivenessMetrics::FlushPointerTimerFired(TimerBase*) {
  FlushPointerMap();
}

void ResponsivenessMetrics::FlushPointerMap() {
  LocalDOMWindow* window = window_performance_->DomWindow();
  if (!window)
    return;
  Vector<PointerId> pointer_ids_to_remove;
  for (const auto& item : pointer_id_entry_map_) {
    PerformanceEventTiming* entry = item.value->GetEntry();
    // Report entries that are currently waiting for a click. This could be the
    // case when the entry's name() is pointerup or when we have more than one
    // event for this |item|, which means we have pointerdown and pointerup.
    if (entry->name() == event_type_names::kPointerup ||
        item.value->GetTimeStamps().size() > 1u) {
      NotifyPointerdown(entry);
      RecordDragTapOrClickUKM(window, *item.value);
      pointer_ids_to_remove.push_back(item.key);
    }
  }
  pointer_id_entry_map_.RemoveAll(pointer_ids_to_remove);
}

void ResponsivenessMetrics::NotifyPointerdown(
    PerformanceEventTiming* entry) const {
  // We only delay dispatching entries when they are pointerdown.
  if (entry->name() != event_type_names::kPointerdown)
    return;
  window_performance_->NotifyAndAddEventTimingBuffer(entry);
}

void ResponsivenessMetrics::KeyboardEntryAndTimestamps::Trace(
    Visitor* visitor) const {
  visitor->Trace(entry_);
}

void ResponsivenessMetrics::PointerEntryAndInfo::Trace(Visitor* visitor) const {
  visitor->Trace(entry_);
}

void ResponsivenessMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(window_performance_);
  visitor->Trace(pointer_id_entry_map_);
  visitor->Trace(key_code_entry_map_);
  visitor->Trace(pointer_flush_timer_);
}

}  // namespace blink
