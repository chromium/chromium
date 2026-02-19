// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/responsiveness_metrics.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/histogram_scope.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/pointer_event_factory.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event.h"

namespace blink {

namespace {
// Minimum potentially generated value for UKM sampling.
constexpr int kMinValueForSampling = 1;
// Maximum potentially generated value for UKM sampling.
constexpr int kMaxValueForSampling = 100;
// UKM sampling rate. The sampling strategy is 1/N.
constexpr int kUkmSamplingRate = 10;
// The length of the timer to flush entries from the time pointerup occurs.
constexpr base::TimeDelta kFlushTimerLength = base::Seconds(1);
// The name for the histogram which records interaction timings, and the names
// of the variants for keyboard or click/tap interactions.
const char kHistogramMaxEventDuration[] =
    "Blink.Responsiveness.UserInteraction.MaxEventDuration";
const char kHistogramAllTypes[] = ".AllTypes";
const char kHistogramKeyboard[] = ".Keyboard";
const char kHistogramTapOrClick[] = ".TapOrClick";

constexpr char kSlowInteractionToNextPaintTraceEventCategory[] = "latency";
constexpr char kSlowInteractionToNextPaintTraceEventName[] =
    "SlowInteractionToNextPaint";

void EmitSlowInteractionToNextPaintTraceEvent(
    const ResponsivenessMetrics::EventTimestamps& event,
    uint64_t event_id) {
  auto track =
      perfetto::Track::Global(base::trace_event::GetNextGlobalTraceId());
  TRACE_EVENT_BEGIN(kSlowInteractionToNextPaintTraceEventCategory,
                    kSlowInteractionToNextPaintTraceEventName, track,
                    event.creation_time);
  TRACE_EVENT_END(kSlowInteractionToNextPaintTraceEventCategory, track,
                  event.end_time, perfetto::Flow::Global(event_id));
}

// Returns the longest event in `timestamps`.
ResponsivenessMetrics::EventTimestamps LongestEvent(
    const Vector<ResponsivenessMetrics::EventTimestamps>& events) {
  DCHECK(events.size());
  return *std::max_element(
      events.begin(), events.end(),
      [](const ResponsivenessMetrics::EventTimestamps& left,
         const ResponsivenessMetrics::EventTimestamps& right) {
        return left.duration() < right.duration();
      });
}

std::unique_ptr<TracedValue> UserInteractionTraceData(
    base::TimeDelta max_duration,
    bool is_pointer) {
  auto traced_value = std::make_unique<TracedValue>();
  traced_value->SetInteger("maxDuration",
                           static_cast<int>(max_duration.InMilliseconds()));
  traced_value->SetString("interactionType",
                          is_pointer ? "tapOrClick" : "keyboard");
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
      composition_end_flush_timer_(
          window_performance_->task_runner_,
          this,
          &ResponsivenessMetrics::FlushCompositionEndTimerFired) {}

ResponsivenessMetrics::~ResponsivenessMetrics() = default;

void ResponsivenessMetrics::RecordUserInteractionUKM(
    LocalDOMWindow* window,
    UserInteractionType interaction_type,
    const Vector<EventTimestamps>& timestamps,
    uint32_t interaction_offset) {
  if (!window) {
    return;
  }

  for (EventTimestamps timestamp : timestamps) {
    if (timestamp.creation_time == base::TimeTicks()) {
      return;
    }
  }

  EventTimestamps longest_event = LongestEvent(timestamps);
  base::TimeTicks max_event_start = longest_event.creation_time;
  base::TimeTicks max_event_end = longest_event.end_time;
  base::TimeTicks max_event_queued_main_thread =
      longest_event.queued_to_main_thread_time;
  base::TimeTicks max_event_commit_finish = longest_event.commit_finish_time;
  base::TimeDelta max_event_duration = longest_event.duration();
  // We found some negative values in the data. Before figuring out the root
  // cause, we need this check to avoid sending nonsensical data.
  if (max_event_duration.InMilliseconds() >= 0) {
    window->GetFrame()->Client()->DidObserveUserInteraction(
        max_event_start, max_event_queued_main_thread, max_event_commit_finish,
        max_event_end, interaction_offset);
  }

  bool is_pointer_event = interaction_type == UserInteractionType::kTapOrClick;

  TRACE_EVENT2("devtools.timeline", "Responsiveness.Renderer.UserInteraction",
               "data",
               UserInteractionTraceData(max_event_duration, is_pointer_event),
               "frame", GetFrameIdForTracing(window->GetFrame()));

  EmitInteractionToNextPaintTraceEvent(longest_event, is_pointer_event);

  // Emit a trace event when "interaction to next paint" is considered "slow"
  // according to RAIL guidelines (web.dev/rail).
  uint64_t event_id = base::trace_event::GetNextGlobalTraceId();
  constexpr base::TimeDelta kSlowInteractionToNextPaintThreshold =
      base::Milliseconds(100);
  if (longest_event.duration() > kSlowInteractionToNextPaintThreshold) {
    EmitSlowInteractionToNextPaintTraceEvent(longest_event, event_id);
  }

  base::trace_event::HistogramScope scoped_event(event_id);
  LogResponsivenessHistogram(max_event_duration, kHistogramAllTypes);
  if (is_pointer_event) {
    LogResponsivenessHistogram(max_event_duration, kHistogramTapOrClick);
  } else {
    LogResponsivenessHistogram(max_event_duration, kHistogramKeyboard);
  }

  ukm::UkmRecorder* ukm_recorder = window->UkmRecorder();
  ukm::SourceId source_id = window->UkmSourceID();
  if (source_id != ukm::kInvalidSourceId &&
      (!sampling_ ||
       base::RandIntInclusive(kMinValueForSampling, kMaxValueForSampling) <=
           kUkmSamplingRate)) {
    ukm::builders::Responsiveness_UserInteraction(source_id)
        .SetInteractionType(static_cast<int64_t>(interaction_type))
        .SetMaxEventDuration(max_event_duration.InMilliseconds())
        .Record(ukm_recorder);
  }
}

void ResponsivenessMetrics::RecordTapOrClickUKM(
    LocalDOMWindow* window,
    PointerEntryAndInfo& pointer_info) {
  DCHECK(pointer_info.GetEntry());
  // Early return if all we got was a pointerdown.
  if (pointer_info.GetEntry()->name() == event_type_names::kPointerdown &&
      pointer_info.GetTimeStamps().size() == 1u) {
    return;
  }
  auto interaction_id = pointer_info.GetEntry()->GetInteractionIdInfo();
  if (!interaction_id ||
      interaction_id->id == PerformanceTimelineEntryIdInfo::kNoId) {
    return;
  }
  if (pointer_info.GetEntry()
          ->GetEventTimingReportingInfo()
          ->prevent_counting_as_interaction) {
    return;
  }
  RecordUserInteractionUKM(window, UserInteractionType::kTapOrClick,
                           pointer_info.GetTimeStamps(),
                           interaction_id->offset);
}

// Event timing pointer events processing
//
// See also ./Pointer_interaction_state_machine.md
// (https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/timing/Pointer_interaction_state_machine.md)
// to help understand the logic below that how event timing group up pointer
// events as interactions.
bool ResponsivenessMetrics::SetPointerIdAndRecordLatency(
    PerformanceEventTiming* entry,
    EventTimestamps event_timestamps) {
  const AtomicString& event_type = entry->name();
  auto pointer_id = entry->GetEventTimingReportingInfo()->pointer_id.value();
  auto* pending_pointer_down = pointer_id_entry_map_.Contains(pointer_id)
                                   ? pointer_id_entry_map_.at(pointer_id)
                                   : nullptr;
  CHECK(!pending_pointer_down || pending_pointer_down->GetEntry()->name() ==
                                     event_type_names::kPointerdown);
  LocalDOMWindow* window = window_performance_->DomWindow();
  if (event_type == event_type_names::kPointercancel) {
    if (pending_pointer_down) {
      // Set interaction id to 0 for buffered Pointerdown.
      pending_pointer_down->GetEntry()->SetInteractionIdInfo(
          PerformanceTimelineEntryIdInfo::kNone);
      FlushPointerdownAndNotifyObservers(pending_pointer_down);
      // The pointer id of the pointerdown is no longer needed.
      pointer_id_entry_map_.erase(
          entry->GetEventTimingReportingInfo()->pointer_id.value());
    }
    // Set interaction id to 0 for Pointercancel.
    entry->SetInteractionIdInfo(PerformanceTimelineEntryIdInfo::kNone);
  } else if (event_type == event_type_names::kContextmenu) {
    // A pointerdown followed by contextmenu is assigned an interactionId right
    // away.  We leave the pointerdown in the map of pointer interactions so
    // subsequent pointerup can still be matched.
    if (pending_pointer_down) {
      if (!pending_pointer_down->GetEntry()->HasKnownInteractionID()) {
        pending_pointer_down->GetEntry()->SetInteractionIdInfo(
            interaction_id_generator_.IncrementId());
      }
      FlushPointerdownAndNotifyObservers(pending_pointer_down);
    }
    // Set interaction id to 0 for Contextmenu.
    entry->SetInteractionIdInfo(PerformanceTimelineEntryIdInfo::kNone);
  } else if (event_type == event_type_names::kPointerdown) {
    if (pending_pointer_down) {
      // Set interaction id to 0 for buffered Pointerdown if it's not already
      // set.
      if (!pending_pointer_down->GetEntry()->HasKnownInteractionID()) {
        pending_pointer_down->GetEntry()->SetInteractionIdInfo(
            PerformanceTimelineEntryIdInfo::kNone);
      } else {
        // Flush the existing entry with non 0 interaction id. We are starting
        // a new interaction.
        RecordTapOrClickUKM(window, *pending_pointer_down);
      }

      FlushPointerdownAndNotifyObservers(pending_pointer_down);
      pointer_id_entry_map_.erase(pointer_id);
      pending_pointer_down = nullptr;
    }

    FlushAllPointerdownWithMeasuredPointerup();

    pointer_id_entry_map_.Set(
        pointer_id, PointerEntryAndInfo::Create(entry, event_timestamps));

    return false;
  } else if (event_type == event_type_names::kPointerup) {
    FlushAllPointerdownWithMeasuredPointerup();

    // Check if this is an orphan pointerup.  If we didn't see a pointerdown we
    // will not have pending_pointer_down. If we do have a
    // pending_pointer_down, it might be from a previous interaction if it
    // already has multiple timestamps reported.
    // Early exit if it's an orphan pointerup, not treating it as an
    // interaction. crbug.com/40935137.
    if (!pending_pointer_down ||
        pending_pointer_down->GetTimeStamps().size() > 1) {
      entry->SetInteractionIdInfo(PerformanceTimelineEntryIdInfo::kNone);
      return true;
    }

    // Generate a new interaction id.
    if (!pending_pointer_down->GetEntry()->HasKnownInteractionID()) {
      pending_pointer_down->GetEntry()->SetInteractionIdInfo(
          interaction_id_generator_.IncrementId());
    }

    entry->SetInteractionIdInfo(
        pending_pointer_down->GetEntry()->GetInteractionIdInfo());

    if (entry->GetEventTimingReportingInfo()->prevent_counting_as_interaction) {
      pending_pointer_down->GetEntry()
          ->GetEventTimingReportingInfo()
          ->prevent_counting_as_interaction = true;
    }

    FlushPointerdownAndNotifyObservers(pending_pointer_down);
    pending_pointer_down->GetTimeStamps().push_back(event_timestamps);
    // Start the timer to flush the entry just created later, if needed.
    pointer_flush_timer_.StartOneShot(kFlushTimerLength, FROM_HERE);

  } else if (event_type == event_type_names::kClick) {
    // Try handle keyboard event simulated click.
    if (TryHandleKeyboardEventSimulatedClick(entry, pointer_id)) {
      return true;
    }

    // We now trust |pointer_id| for all click sources, including pointer,
    // keyboard, and other (accessibility) events.
    if (pending_pointer_down) {
      // There is a previous pointerdown or pointerup entry. Use its
      // interactionId.
      // There are cases where we only see pointerdown and click, for instance
      // with contextmenu.
      if (!pending_pointer_down->GetEntry()->HasKnownInteractionID()) {
        pending_pointer_down->GetEntry()->SetInteractionIdInfo(
            interaction_id_generator_.IncrementId());
      }
      // Click event would always have its interaction id set.
      entry->SetInteractionIdInfo(
          pending_pointer_down->GetEntry()->GetInteractionIdInfo());
      pending_pointer_down->GetTimeStamps().push_back(event_timestamps);

      RecordTapOrClickUKM(window, *pending_pointer_down);

      // The pointer id of the pointerdown is no longer needed.
      pointer_id_entry_map_.erase(pointer_id);

    } else {
      // There is no previous pointerdown or pointerup entry. This can happen
      // when the user clicks using a non-pointer device. Generate a new
      // interactionId. No need to add to the map since this is the last event
      // in the interaction.
      // Click event would always have its interaction id set.
      entry->SetInteractionIdInfo(interaction_id_generator_.IncrementId());
      RecordTapOrClickUKM(
          window, *PointerEntryAndInfo::Create(entry, event_timestamps));
    }
    // Any existing pointerup in the map cannot fire a click.
    FlushAllPointerdownWithMeasuredPointerup();
  }
  return true;
}

void ResponsivenessMetrics::RecordKeyboardUKM(
    LocalDOMWindow* window,
    const Vector<EventTimestamps>& event_timestamps,
    uint32_t interaction_offset) {
  RecordUserInteractionUKM(window, UserInteractionType::kKeyboard,
                           event_timestamps, interaction_offset);
}

// Event timing keyboard events processing
//
// See also ./Key_interaction_state_machine.md
// (https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/timing/Key_interaction_state_machine.md)
// to help understand the logic below that how event timing group up keyboard
// events as interactions.
void ResponsivenessMetrics::SetKeyIdAndRecordLatency(
    PerformanceEventTiming* entry,
    EventTimestamps event_timestamps) {
  auto event_type = entry->name();
  if (event_type == event_type_names::kKeydown) {
    CHECK(entry->GetEventTimingReportingInfo()->key_code.has_value());
    auto key_code = entry->GetEventTimingReportingInfo()->key_code.value();
    if (composition_state_ == kNonComposition) {
      if (IsHoldingKey(key_code)) {
        FlushSequenceBasedKeyboardEvents();
      }
      interaction_id_generator_.IncrementId();
    } else if (composition_state_ == kCompositionContinueOngoingInteraction) {
      // Continue interaction; Do not update Interaction Id
    } else if (composition_state_ == kCompositionStartNewInteractionOnKeydown) {
      FlushSequenceBasedKeyboardEvents();
      interaction_id_generator_.IncrementId();
      composition_state_ = kCompositionContinueOngoingInteraction;
    } else if (composition_state_ == kEndCompositionOnKeydown) {
      FlushSequenceBasedKeyboardEvents();
      interaction_id_generator_.IncrementId();
      composition_state_ = kNonComposition;
    }

    // Keydown always has a interaction id set.
    entry->SetInteractionIdInfo(interaction_id_generator_.GetValue());
    sequence_based_keyboard_interaction_info_.SetInteractionIdInfo(
        interaction_id_generator_.GetValue());
    sequence_based_keyboard_interaction_info_.AddTimestamps(event_timestamps);

    if (composition_state_ == kNonComposition) {
      InteractionInfo keydown_entry;
      keydown_entry.SetInteractionIdInfo(interaction_id_generator_.GetValue());
      keydown_entry.AddTimestamps(event_timestamps);
      key_code_to_interaction_info_map_.Set(key_code, std::move(keydown_entry));
    }
    last_keydown_keycode_info_ =
        KeycodeInfo(key_code, interaction_id_generator_.GetValue());
  } else if (event_type == event_type_names::kKeyup) {
    if (composition_state_ == kNonComposition) {
      CHECK(entry->GetEventTimingReportingInfo()->key_code.has_value());
      auto key_code = entry->GetEventTimingReportingInfo()->key_code.value();
      if (!key_code_to_interaction_info_map_.Contains(key_code)) {
        entry->SetInteractionIdInfo(PerformanceTimelineEntryIdInfo::kNone);
        return;
      }

      // Match the keydown entry with the keyup entry using keycode.
      auto& key_entry = key_code_to_interaction_info_map_.find(key_code)->value;

      // Keyup always has a interaction id set.
      entry->SetInteractionIdInfo(key_entry.GetInteractionIdInfo());
      key_entry.AddTimestamps(event_timestamps);
      RecordKeyboardUKM(window_performance_->DomWindow(),
                        key_entry.GetTimeStamps(),
                        key_entry.GetInteractionIdInfo().offset);
      // Remove keycode from the map and reset other values
      key_code_to_interaction_info_map_.erase(key_code);
      sequence_based_keyboard_interaction_info_.Clear();
    } else {
      // Keyup always has a interaction id set.
      entry->SetInteractionIdInfo(interaction_id_generator_.GetValue());
      sequence_based_keyboard_interaction_info_.SetInteractionIdInfo(
          interaction_id_generator_.GetValue());
      sequence_based_keyboard_interaction_info_.AddTimestamps(event_timestamps);
    }
  } else if (event_type == event_type_names::kKeypress) {
    if (composition_state_ == kNonComposition &&
        last_keydown_keycode_info_.has_value() &&
        key_code_to_interaction_info_map_.find(
            last_keydown_keycode_info_.value().keycode) !=
            key_code_to_interaction_info_map_.end()) {
      // Set a interaction id generated by previous keydown entry
      entry->SetInteractionIdInfo(interaction_id_generator_.GetValue());
      key_code_to_interaction_info_map_
          .find(last_keydown_keycode_info_.value().keycode)
          ->value.AddTimestamps(event_timestamps);
    } else {
      // This happens when keypress cannot be matched to a keydown (or we are in
      // composition mode). For now, don't assign an interactionId here, but
      // consider investigating for use cases.
      entry->SetInteractionIdInfo(PerformanceTimelineEntryIdInfo::kNone);
    }
  } else if (event_type == event_type_names::kCompositionstart) {
    entry->SetInteractionIdInfo(PerformanceTimelineEntryIdInfo::kNone);
    composition_state_ = kCompositionContinueOngoingInteraction;
    key_code_to_interaction_info_map_.clear();
  } else if (event_type == event_type_names::kCompositionend) {
    entry->SetInteractionIdInfo(PerformanceTimelineEntryIdInfo::kNone);
    composition_state_ = kEndCompositionOnKeydown;
    composition_end_flush_timer_.StartOneShot(kFlushTimerLength, FROM_HERE);
  } else if (event_type == event_type_names::kCompositionupdate) {
    entry->SetInteractionIdInfo(PerformanceTimelineEntryIdInfo::kNone);
    if (!last_keydown_keycode_info_.has_value()) {
      composition_state_ = kCompositionStartNewInteractionOnInput;
    } else {
      composition_state_ = kCompositionStartNewInteractionOnKeydown;
    }
  } else if (event_type == event_type_names::kInput) {
    // Expose interactionId for Input events only under composition
    if (composition_state_ == kNonComposition) {
      entry->SetInteractionIdInfo(PerformanceTimelineEntryIdInfo::kNone);
      return;
    }
    // Update Interaction Id when input is selected using IME suggestion without
    // pressing a key. In this case Input event starts and finishes interaction
    if (composition_state_ == kCompositionStartNewInteractionOnInput) {
      FlushSequenceBasedKeyboardEvents();
      entry->SetInteractionIdInfo(interaction_id_generator_.IncrementId());
      sequence_based_keyboard_interaction_info_.SetInteractionIdInfo(
          interaction_id_generator_.GetValue());
      sequence_based_keyboard_interaction_info_.AddTimestamps(event_timestamps);
      FlushSequenceBasedKeyboardEvents();
      composition_state_ = kCompositionStartNewInteractionOnKeydown;
    } else {
      // TODO(crbug.com/1252856): fix counts in ChromeOS due to duplicate
      // events.
      entry->SetInteractionIdInfo(interaction_id_generator_.GetValue());
      sequence_based_keyboard_interaction_info_.SetInteractionIdInfo(
          interaction_id_generator_.GetValue());
      sequence_based_keyboard_interaction_info_.AddTimestamps(event_timestamps);
    }
    last_keydown_keycode_info_.reset();
  }
}

void ResponsivenessMetrics::FlushKeydown() {
  for (auto& entry : key_code_to_interaction_info_map_) {
    // Keydowns triggered contextmenu, though missing pairing keyups due to a
    // known issue - https://github.com/w3c/pointerevents/issues/408, should
    // still be counted as a valid interaction and get reported to UKM.
    RecordKeyboardUKM(window_performance_->DomWindow(),
                      entry.value.GetTimeStamps(),
                      entry.value.GetInteractionIdInfo().offset);
  }
  key_code_to_interaction_info_map_.clear();
}

void ResponsivenessMetrics::FlushAllEventsAtPageHidden() {
  // Flush events that are waiting to be set an interaction id.
  FlushAllPointerdown();

  FlushKeydown();

  FlushSequenceBasedKeyboardEvents();
}

uint32_t ResponsivenessMetrics::GetInteractionCount() const {
  return interaction_id_generator_.GetValue().offset;
}

void ResponsivenessMetrics::SetCurrentInteractionEventQueuedTimestamp(
    base::TimeTicks queued_time) {
  current_interaction_event_queued_timestamp_ = queued_time;
}

base::TimeTicks ResponsivenessMetrics::CurrentInteractionEventQueuedTimestamp()
    const {
  return current_interaction_event_queued_timestamp_;
}

void ResponsivenessMetrics::FlushCompositionEndTimerFired(TimerBase*) {
  FlushSequenceBasedKeyboardEvents();
}

void ResponsivenessMetrics::FlushSequenceBasedKeyboardEvents() {
  LocalDOMWindow* window = window_performance_->DomWindow();
  if (!window) {
    return;
  }

  if (composition_end_flush_timer_.IsActive()) {
    composition_end_flush_timer_.Stop();
  }

  if (!sequence_based_keyboard_interaction_info_.Empty()) {
    RecordKeyboardUKM(
        window, sequence_based_keyboard_interaction_info_.GetTimeStamps(),
        sequence_based_keyboard_interaction_info_.GetInteractionIdInfo()
            .offset);
    sequence_based_keyboard_interaction_info_.Clear();
  }
}

// Determines if the key is is being held (pressed) for a sustained period of
// time. It is used when keyup does not appear in the end of a interaction (e.g
// Windows). In such cases the interaction is reported using
// sequence_based_keyboard_interaction_info_.
bool ResponsivenessMetrics::IsHoldingKey(std::optional<int> key_code) {
  return last_keydown_keycode_info_.has_value() &&
         last_keydown_keycode_info_->keycode == key_code;
}

void ResponsivenessMetrics::FlushPointerTimerFired(TimerBase*) {
  FlushAllPointerdownWithMeasuredPointerup();
}

void ResponsivenessMetrics::FlushAllPointerdownWithMeasuredPointerup() {
  LocalDOMWindow* window = window_performance_->DomWindow();
  if (!window) {
    return;
  }
  if (pointer_flush_timer_.IsActive()) {
    pointer_flush_timer_.Stop();
  }

  Vector<PointerId> pointer_ids_to_remove;
  for (const auto& item : pointer_id_entry_map_) {
    PerformanceEventTiming* entry = item.value->GetEntry();
    // Report [pointerdown,pointerup]s that are currently waiting for a click.
    if (item.value->GetTimeStamps().size() > 1u) {
      CHECK(entry->name() == event_type_names::kPointerdown);
      CHECK(entry->HasKnownInteractionID());
      RecordTapOrClickUKM(window, *item.value);
      pointer_ids_to_remove.push_back(item.key);
    }
  }

  // map clean up
  pointer_id_entry_map_.RemoveAll(pointer_ids_to_remove);
}

void ResponsivenessMetrics::FlushAllPointerdown() {
  LocalDOMWindow* window = window_performance_->DomWindow();
  if (!window) {
    return;
  }
  if (pointer_flush_timer_.IsActive()) {
    pointer_flush_timer_.Stop();
  }

  for (const auto& item : pointer_id_entry_map_) {
    PerformanceEventTiming* entry = item.value->GetEntry();
    if (!entry->HasKnownInteractionID()) {
      entry->SetInteractionIdInfo(interaction_id_generator_.IncrementId());
    }

    // Pointerdown without pointerup nor click need to notify performance
    // observer since they haven't.
    if (item.value->GetTimeStamps().size() == 1u) {
      FlushPointerdownAndNotifyObservers(item.value.Get());
    }
    RecordTapOrClickUKM(window, *item.value);
  }

  // map clean up
  pointer_id_entry_map_.clear();
}

void ResponsivenessMetrics::FlushPointerdownAndNotifyObservers(
    PointerEntryAndInfo* pointer_info) const {
  // A pointerdown may be "flushed" to performance timeline when any number of
  // stop criteria are met, but we need to keep it in the map of pointer
  // interactions in order to have an interaction id for "all parts" of the
  // interaction.  So, we guard against subsequent reports.
  if (pointer_info->WasEntryEmitted()) {
    return;
  }
  PerformanceEventTiming* entry = pointer_info->GetEntry();
  // We only delay dispatching entries when they are pointerdown.
  CHECK(entry->name() == event_type_names::kPointerdown);
  CHECK(entry->HasKnownInteractionID());
  window_performance_->NotifyAndAddEventTimingBuffer(entry);
  pointer_info->SetEntryEmitted();
}

// Flush UKM timestamps of composition events for testing.
void ResponsivenessMetrics::FlushAllEventsForTesting() {
  FlushSequenceBasedKeyboardEvents();
}

void ResponsivenessMetrics::PointerEntryAndInfo::Trace(Visitor* visitor) const {
  visitor->Trace(entry_);
}

void ResponsivenessMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(window_performance_);
  visitor->Trace(pointer_id_entry_map_);
  visitor->Trace(pointer_flush_timer_);
  visitor->Trace(composition_end_flush_timer_);
}

void ResponsivenessMetrics::EmitInteractionToNextPaintTraceEvent(
    const ResponsivenessMetrics::EventTimestamps& event,
    bool is_pointer_event) {
  const perfetto::Track track(base::trace_event::GetNextGlobalTraceId(),
                              perfetto::ProcessTrack::Current());
  TRACE_EVENT_BEGIN(
      "interactions", "Web Interaction", track, event.creation_time,
      [&](perfetto::EventContext& ctx) {
        using Interaction = perfetto::protos::pbzero::WebContentInteraction;

        auto* web_content_interaction =
            ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_web_content_interaction();
        web_content_interaction->set_type(
            is_pointer_event ? Interaction::INTERACTION_CLICK_TAP
                             : Interaction::INTERACTION_KEYBOARD);
      });

  TRACE_EVENT_END("interactions", track, event.end_time);
}

// For keyboard-simulated clicks (pointer_id == kReservedNonPointerId), assign
// the interaction ID from the last keydown event. We assume a keydown event
// always precedes a simulated click.

// TODO(crbug.com/328902994): Rename this function to
// `TryHandleNonPointerClickEvent` and explicitly handle non-keyboard
// non-pointer interactions, by assigning a new interactionId here.
bool ResponsivenessMetrics::TryHandleKeyboardEventSimulatedClick(
    PerformanceEventTiming* entry,
    const std::optional<PointerId>& pointer_id) {
  // We assume simulated clicks with pointer_id -1 should be dispatched by
  // keyboard events and expect the presence of a keydown event.
  if (pointer_id != PointerEventFactory::kReservedNonPointerId) {
    return false;
  }

  if (!last_keydown_keycode_info_.has_value()) {
    // Count the occurrence of a simulated click with no active keyboard
    // interaction. See crbug.com/40824503.

    blink::UseCounter::Count(
        window_performance_->GetExecutionContext(),
        WebFeature::kEventTimingSimulatedClickWithNoKeyboardInteraction);
    return false;
  }

  entry->SetInteractionIdInfo(last_keydown_keycode_info_->interaction_id);

  return true;
}
}  // namespace blink
