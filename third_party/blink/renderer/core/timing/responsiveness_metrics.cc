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

constexpr char kSlowInteractionToNextPaintTraceEventCategory[] = "latency";
constexpr char kSlowInteractionToNextPaintTraceEventName[] =
    "SlowInteractionToNextPaint";

const char kPageLoadInternalEventTimingClickInteractionEvents[] =
    "PageLoad.Internal.EventTiming.ClickInteractionEvents";

// These values are logged to UMA. Please keep in sync with
// "EventTimingClickInteractionEvents" in tools/metrics/histograms/enums.xml.
// LINT.IfChange
enum ClickInteractionEvents {
  kClickDetected = 0,
  kPointerClickWithPointerdownAndPointerup = 1,
  kPointerClickWithMissingPointerdownOnly = 2,
  kPointerClickWithMissingPointerupOnly = 3,
  kPointerClickWithMissingPointerdownAndPointerup = 4,
  kPointerClickPointerIdDifferFromLastPointerIdAndPointerIdExistInMap = 5,
  kPointerClickPointerIdDifferFromLastPointerIdAndOnlyLastPointerIdInMap = 6,
  kPointerClickPointerIdDifferFromLastPointerIdAndNeitherInMap = 7,
  kKeyboardClick = 8,
  kMaxValue = kKeyboardClick,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:EventTimingClickInteractionEvents)

void EmitSlowInteractionToNextPaintTraceEvent(
    const ResponsivenessMetrics::EventTimestamps& event) {
  uint64_t trace_id = base::trace_event::GetNextGlobalTraceId();
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      kSlowInteractionToNextPaintTraceEventCategory,
      kSlowInteractionToNextPaintTraceEventName, trace_id, event.creation_time);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      kSlowInteractionToNextPaintTraceEventCategory,
      kSlowInteractionToNextPaintTraceEventName, trace_id, event.end_time);
}

// Returns the longest event in `timestamps`.
ResponsivenessMetrics::EventTimestamps LongestEvent(
    const WTF::Vector<ResponsivenessMetrics::EventTimestamps>& events) {
  DCHECK(events.size());
  return *std::max_element(
      events.begin(), events.end(),
      [](const ResponsivenessMetrics::EventTimestamps& left,
         const ResponsivenessMetrics::EventTimestamps& right) {
        return left.duration() < right.duration();
      });
}

base::TimeDelta TotalEventDuration(
    // timestamps is sorted by the start_time of EventTimestamps.
    const WTF::Vector<ResponsivenessMetrics::EventTimestamps>& timestamps) {
  DCHECK(timestamps.size());
  // TODO(crbug.com/1229668): Once the event timestamp bug is fixed, add a
  // DCHECK(IsSorted) here.
  base::TimeDelta total_duration =
      timestamps[0].end_time - timestamps[0].creation_time;
  base::TimeTicks current_end_time = timestamps[0].end_time;
  for (WTF::wtf_size_t i = 1; i < timestamps.size(); ++i) {
    total_duration += timestamps[i].end_time - timestamps[i].creation_time;
    if (timestamps[i].creation_time < current_end_time) {
      total_duration -= std::min(current_end_time, timestamps[i].end_time) -
                        timestamps[i].creation_time;
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
      NOTREACHED_IN_MIGRATION();
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
      contextmenu_flush_timer_(
          window_performance_->task_runner_,
          this,
          &ResponsivenessMetrics::ContextmenuFlushTimerFired),
      composition_end_flush_timer_(
          window_performance_->task_runner_,
          this,
          &ResponsivenessMetrics::FlushCompositionEndTimerFired),
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
    const WTF::Vector<EventTimestamps>& timestamps,
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
  base::TimeDelta total_event_duration = TotalEventDuration(timestamps);
  // We found some negative values in the data. Before figuring out the root
  // cause, we need this check to avoid sending nonsensical data.
  if (max_event_duration.InMilliseconds() >= 0) {
    window->GetFrame()->Client()->DidObserveUserInteraction(
        max_event_start, max_event_queued_main_thread, max_event_commit_finish,
        max_event_end, interaction_type, interaction_offset);
  }
  TRACE_EVENT2("devtools.timeline", "Responsiveness.Renderer.UserInteraction",
               "data",
               UserInteractionTraceData(max_event_duration,
                                        total_event_duration, interaction_type),
               "frame", GetFrameIdForTracing(window->GetFrame()));

  EmitInteractionToNextPaintTraceEvent(longest_event, interaction_type,
                                       total_event_duration);
  // Emit a trace event when "interaction to next paint" is considered "slow"
  // according to RAIL guidelines (web.dev/rail).
  constexpr base::TimeDelta kSlowInteractionToNextPaintThreshold =
      base::Milliseconds(100);
  if (longest_event.duration() > kSlowInteractionToNextPaintThreshold) {
    EmitSlowInteractionToNextPaintTraceEvent(longest_event);
  }

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
  if (pointer_id_entry_map_.Contains(pointer_id)) {
    pointer_id_entry_map_.at(pointer_id)->SetIsDrag();
  }
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
  if (pointer_info.GetEntry()->interactionId() == 0u) {
    return;
  }
  RecordUserInteractionUKM(window,
                           pointer_info.IsDrag()
                               ? UserInteractionType::kDrag
                               : UserInteractionType::kTapOrClick,
                           pointer_info.GetTimeStamps(),
                           pointer_info.GetEntry()->interactionOffset());
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
  auto* pointer_info = pointer_id_entry_map_.Contains(pointer_id)
                           ? pointer_id_entry_map_.at(pointer_id)
                           : nullptr;
  LocalDOMWindow* window = window_performance_->DomWindow();
  if (event_type == event_type_names::kPointercancel && pointer_info) {
    NotifyPointerdown(pointer_info->GetEntry());
    // The pointer id of the pointerdown is no longer needed.
    pointer_id_entry_map_.erase(
        entry->GetEventTimingReportingInfo()->pointer_id.value());
    last_pointer_id_ = std::nullopt;
  } else if (event_type == event_type_names::kContextmenu) {
    // Start a timer to flush event timing entries when times up. On receiving a
    // new pointerup or pointerdown, the timer will be canceled and entries will
    // be flushed immediately.
    contextmenu_flush_timer_.StartOneShot(kFlushTimerLength, FROM_HERE);
  } else if (event_type == event_type_names::kPointerdown) {
    // If we were waiting for matching pointerup/keyup after a contextmenu, they
    // won't show up at this point.
    if (contextmenu_flush_timer_.IsActive()) {
      contextmenu_flush_timer_.Stop();
      FlushPointerdownAndPointerup();
      FlushKeydown();
    } else {
      if (pointer_info) {
        // Flush the existing entry. We are starting a new interaction.
        RecordDragTapOrClickUKM(window, *pointer_info);
        NotifyPointerdown(pointer_info->GetEntry());
        pointer_id_entry_map_.erase(pointer_id);
      }
      // Any existing pointerup in the map cannot fire a click.
      FlushPointerup();
    }

    if (entry->GetEventTimingReportingInfo()->prevent_counting_as_interaction) {
      entry->SetInteractionId(0);
    }
    pointer_id_entry_map_.Set(
        pointer_id, PointerEntryAndInfo::Create(entry, event_timestamps));

    // Waiting to see if we get a pointercancel or pointerup.
    last_pointer_id_ = pointer_id;
    return false;
  } else if (event_type == event_type_names::kPointerup) {
    if (contextmenu_flush_timer_.IsActive()) {
      contextmenu_flush_timer_.Stop();
    }

    // Any existing pointerup in the map cannot fire a click.
    FlushPointerup();

    is_last_pointerup_orphan_ = false;
    last_pointer_id_ = pointer_id;

    // Platforms like Android would create ever-increasing pointer_id for
    // interactions, whereas platforms like linux could reuse the same id for
    // different interactions. So resetting pointer_info here if it's flushed.
    if (!pointer_id_entry_map_.Contains(pointer_id)) {
      is_last_pointerup_orphan_ = true;

      // Reset if pointer_info got flushed.
      pointer_info = nullptr;

      UseCounter::Count(window_performance_->GetExecutionContext(),
                        WebFeature::kEventTimingOrphanPointerup);

      if (base::FeatureList::IsEnabled(
              features::kEventTimingHandleOrphanPointerup)) {
        // Early exit if it's an orphan pointerup, not treating it as an
        // interaction. crbug.com/40935137
        return true;
      }
    }

    // Generate a new interaction id.
    // Do not generate any interaction id for the events when the scroll is
    // active.
    if (!RuntimeEnabledFeaturesBase::
            EventTimingTapStopScrollNoInteractionIdEnabled() ||
        (pointer_info && !pointer_info->GetEntry()->HasKnownInteractionID())) {
      UpdateInteractionId();
      entry->SetInteractionIdAndOffset(GetCurrentInteractionId(),
                                       GetInteractionCount());
    }

    if (pointer_info &&
        pointer_info->GetEntry()->name() == event_type_names::kPointerdown) {
      // Set interaction id and notify the pointer down entry.
      PerformanceEventTiming* pointer_down_entry = pointer_info->GetEntry();
      if (entry->HasKnownInteractionID()) {
        pointer_down_entry->SetInteractionIdAndOffset(
            entry->interactionId(), entry->interactionOffset());
      }
      NotifyPointerdown(pointer_down_entry);
      pointer_info->GetTimeStamps().push_back(event_timestamps);
    } else {
      // There is no matching pointerdown: Set the map using pointerup, in
      // case a click event shows up.
      pointer_id_entry_map_.Set(
          pointer_id, PointerEntryAndInfo::Create(entry, event_timestamps));
    }
    // Start the timer to flush the entry just created later, if needed.
    pointer_flush_timer_.StartOneShot(kFlushTimerLength, FROM_HERE);
  } else if (event_type == event_type_names::kClick) {
    base::UmaHistogramEnumeration(
        kPageLoadInternalEventTimingClickInteractionEvents,
        ClickInteractionEvents::kClickDetected);
    if (pointer_id == PointerEventFactory::kReservedNonPointerId) {
      base::UmaHistogramEnumeration(
          kPageLoadInternalEventTimingClickInteractionEvents,
          ClickInteractionEvents::kKeyboardClick);
    } else if (is_last_pointerup_orphan_) {
      UseCounter::Count(window_performance_->GetExecutionContext(),
                        WebFeature::kEventTimingOrphanPointerupWithClick);
      base::UmaHistogramEnumeration(
          kPageLoadInternalEventTimingClickInteractionEvents,
          ClickInteractionEvents::kPointerClickWithMissingPointerdownOnly);
      is_last_pointerup_orphan_ = false;
    }

    if (last_pointer_id_.has_value() && pointer_id != *last_pointer_id_ &&
        // Exclude keyboard clicks.
        pointer_id != PointerEventFactory::kReservedNonPointerId) {
      if (pointer_id_entry_map_.Contains(pointer_id)) {
        base::UmaHistogramEnumeration(
            kPageLoadInternalEventTimingClickInteractionEvents,
            ClickInteractionEvents::
                kPointerClickPointerIdDifferFromLastPointerIdAndPointerIdExistInMap);
      } else {
        if (pointer_id_entry_map_.Contains(*last_pointer_id_)) {
          base::UmaHistogramEnumeration(
              kPageLoadInternalEventTimingClickInteractionEvents,
              ClickInteractionEvents::
                  kPointerClickPointerIdDifferFromLastPointerIdAndOnlyLastPointerIdInMap);
        } else {
          base::UmaHistogramEnumeration(
              kPageLoadInternalEventTimingClickInteractionEvents,
              ClickInteractionEvents::
                  kPointerClickPointerIdDifferFromLastPointerIdAndNeitherInMap);
        }
      }
    }

    if (RuntimeEnabledFeaturesBase::
            EventTimingHandleKeyboardEventSimulatedClickEnabled()) {
      // Try handle keyboard event simulated click.
      if (TryHandleKeyboardEventSimulatedClick(entry, pointer_id)) {
        return true;
      }
    }

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

      if (previous_entry->name() == event_type_names::kPointerdown) {
        if (pointer_info->GetTimeStamps().size() > 1u) {
          base::UmaHistogramEnumeration(
              kPageLoadInternalEventTimingClickInteractionEvents,
              ClickInteractionEvents::kPointerClickWithPointerdownAndPointerup);
        } else {
          base::UmaHistogramEnumeration(
              kPageLoadInternalEventTimingClickInteractionEvents,
              ClickInteractionEvents::kPointerClickWithMissingPointerupOnly);
        }
      }

      // There are cases where we only see pointerdown and click, for instance
      // with contextmenu.
      if (!previous_entry->HasKnownInteractionID()) {
        UpdateInteractionId();
        previous_entry->SetInteractionIdAndOffset(GetCurrentInteractionId(),
                                                  GetInteractionCount());
      }
      entry->SetInteractionIdAndOffset(previous_entry->interactionId(),
                                       previous_entry->interactionOffset());
      pointer_info->GetTimeStamps().push_back(event_timestamps);
      RecordDragTapOrClickUKM(window, *pointer_info);
      // The pointer id of the pointerdown is no longer needed.
      pointer_id_entry_map_.erase(*last_pointer_id_);
    } else {
      // There is no previous pointerdown or pointerup entry. This can happen
      // when the user clicks using a non-pointer device. Generate a new
      // interactionId. No need to add to the map since this is the last event
      // in the interaction.
      UpdateInteractionId();
      entry->SetInteractionIdAndOffset(GetCurrentInteractionId(),
                                       GetInteractionCount());
      RecordDragTapOrClickUKM(
          window, *PointerEntryAndInfo::Create(entry, event_timestamps));

      // Exclude keyboard clicks.
      if (pointer_id != PointerEventFactory::kReservedNonPointerId) {
        // Note this also count if the click's corresponding pointerdown/up has
        // been over 1 secs thus flushed.
        base::UmaHistogramEnumeration(
            kPageLoadInternalEventTimingClickInteractionEvents,
            ClickInteractionEvents::
                kPointerClickWithMissingPointerdownAndPointerup);
      }
    }
    // Any existing pointerup in the map cannot fire a click.
    FlushPointerup();
    last_pointer_id_ = std::nullopt;
  }
  return true;
}

void ResponsivenessMetrics::RecordKeyboardUKM(
    LocalDOMWindow* window,
    const WTF::Vector<EventTimestamps>& event_timestamps,
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
  last_pointer_id_ = std::nullopt;
  auto event_type = entry->name();
  if (event_type == event_type_names::kKeydown) {
    // If we were waiting for matching pointerup/keyup after a contextmenu, they
    // won't show up at this point.
    if (contextmenu_flush_timer_.IsActive()) {
      contextmenu_flush_timer_.Stop();
      FlushPointerdownAndPointerup();
      FlushKeydown();
    }

    CHECK(entry->GetEventTimingReportingInfo()->key_code.has_value());
    auto key_code = entry->GetEventTimingReportingInfo()->key_code.value();
    if (composition_state_ == kNonComposition) {
      if (IsHoldingKey(key_code)) {
        FlushSequenceBasedKeyboardEvents();
      }
      UpdateInteractionId();
    } else if (composition_state_ == kCompositionContinueOngoingInteraction) {
      // Continue interaction; Do not update Interaction Id
    } else if (composition_state_ == kCompositionStartNewInteractionOnKeydown) {
      FlushSequenceBasedKeyboardEvents();
      UpdateInteractionId();
      composition_state_ = kCompositionContinueOngoingInteraction;
    } else if (composition_state_ == kEndCompositionOnKeydown) {
      FlushSequenceBasedKeyboardEvents();
      UpdateInteractionId();
      composition_state_ = kNonComposition;
    }

    entry->SetInteractionIdAndOffset(GetCurrentInteractionId(),
                                     GetInteractionCount());
    sequence_based_keyboard_interaction_info_.SetInteractionIdAndOffset(
        GetCurrentInteractionId(), GetInteractionCount());
    sequence_based_keyboard_interaction_info_.AddTimestamps(event_timestamps);

    if (composition_state_ == kNonComposition) {
      InteractionInfo keydown_entry(GetCurrentInteractionId(),
                                    GetInteractionCount(), event_timestamps);
      key_code_to_interaction_info_map_.Set(key_code, std::move(keydown_entry));
    }
    last_keydown_keycode_info_ =
        KeycodeInfo(key_code, GetCurrentInteractionId(), GetInteractionCount());
  } else if (event_type == event_type_names::kKeyup) {
    if (composition_state_ == kNonComposition) {
      CHECK(entry->GetEventTimingReportingInfo()->key_code.has_value());
      auto key_code = entry->GetEventTimingReportingInfo()->key_code.value();
      if (!key_code_to_interaction_info_map_.Contains(key_code)) {
        return;
      }

      // Match the keydown entry with the keyup entry using keycode.
      auto& key_entry = key_code_to_interaction_info_map_.find(key_code)->value;
      entry->SetInteractionIdAndOffset(key_entry.GetInteractionId(),
                                       key_entry.GetInteractionOffset());
      key_entry.AddTimestamps(event_timestamps);
      RecordKeyboardUKM(window_performance_->DomWindow(),
                        key_entry.GetTimeStamps(),
                        key_entry.GetInteractionOffset());
      // Remove keycode from the map and reset other values
      key_code_to_interaction_info_map_.erase(key_code);
      sequence_based_keyboard_interaction_info_.Clear();
    } else {
      entry->SetInteractionIdAndOffset(GetCurrentInteractionId(),
                                       GetInteractionCount());
      sequence_based_keyboard_interaction_info_.SetInteractionIdAndOffset(
          GetCurrentInteractionId(), GetInteractionCount());
      sequence_based_keyboard_interaction_info_.AddTimestamps(event_timestamps);
    }
  } else if (event_type == event_type_names::kKeypress) {
    if (composition_state_ == kNonComposition &&
        last_keydown_keycode_info_.has_value() &&
        key_code_to_interaction_info_map_.find(
            last_keydown_keycode_info_.value().keycode) !=
            key_code_to_interaction_info_map_.end()) {
      // Set a interaction id generated by previous keydown entry
      entry->SetInteractionIdAndOffset(GetCurrentInteractionId(),
                                       GetInteractionCount());
      key_code_to_interaction_info_map_
          .find(last_keydown_keycode_info_.value().keycode)
          ->value.AddTimestamps(event_timestamps);
    }
  } else if (event_type == event_type_names::kCompositionstart) {
    composition_state_ = kCompositionContinueOngoingInteraction;
    key_code_to_interaction_info_map_.clear();
  } else if (event_type == event_type_names::kCompositionend) {
    composition_state_ = kEndCompositionOnKeydown;
    composition_end_flush_timer_.StartOneShot(kFlushTimerLength, FROM_HERE);
  } else if (event_type == event_type_names::kCompositionupdate) {
    if (!last_keydown_keycode_info_.has_value()) {
      composition_state_ = kCompositionStartNewInteractionOnInput;
    } else {
      composition_state_ = kCompositionStartNewInteractionOnKeydown;
    }
  } else if (event_type == event_type_names::kInput) {
    // Expose interactionId for Input events only under composition
    if (composition_state_ == kNonComposition) {
      return;
    }
    // Update Interaction Id when input is selected using IME suggestion without
    // pressing a key. In this case Input event starts and finishes interaction
    if (composition_state_ == kCompositionStartNewInteractionOnInput) {
      FlushSequenceBasedKeyboardEvents();
      UpdateInteractionId();
      entry->SetInteractionIdAndOffset(GetCurrentInteractionId(),
                                       GetInteractionCount());
      sequence_based_keyboard_interaction_info_.SetInteractionIdAndOffset(
          GetCurrentInteractionId(), GetInteractionCount());
      sequence_based_keyboard_interaction_info_.AddTimestamps(event_timestamps);
      FlushSequenceBasedKeyboardEvents();
      composition_state_ = kCompositionStartNewInteractionOnKeydown;
    } else {
      // TODO(crbug.com/1252856): fix counts in ChromeOS due to duplicate
      // events.
      entry->SetInteractionIdAndOffset(GetCurrentInteractionId(),
                                       GetInteractionCount());
      sequence_based_keyboard_interaction_info_.SetInteractionIdAndOffset(
          GetCurrentInteractionId(), GetInteractionCount());
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
                      entry.value.GetInteractionOffset());
  }
  key_code_to_interaction_info_map_.clear();
}

void ResponsivenessMetrics::FlushAllEventsAtPageHidden() {
  // Flush events that are waiting to be set an interaction id.
  FlushPointerdownAndPointerup();

  FlushKeydown();

  FlushSequenceBasedKeyboardEvents();
}

uint32_t ResponsivenessMetrics::GetInteractionCount() const {
  return interaction_count_;
}

void ResponsivenessMetrics::UpdateInteractionId() {
  current_interaction_id_for_event_timing_ += kInteractionIdIncrement;
  interaction_count_++;
}

uint32_t ResponsivenessMetrics::GetCurrentInteractionId() const {
  return current_interaction_id_for_event_timing_;
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
        sequence_based_keyboard_interaction_info_.GetInteractionOffset());
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
  FlushPointerup();
}

void ResponsivenessMetrics::FlushPointerup() {
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
    // Report pointerups that are currently waiting for a click. This could be
    // the case when the entry's name() is pointerup or when we have more than
    // one event for this |item|, which means we have pointerdown and pointerup.
    if (entry->name() == event_type_names::kPointerup ||
        item.value->GetTimeStamps().size() > 1u) {
      RecordDragTapOrClickUKM(window, *item.value);
      pointer_ids_to_remove.push_back(item.key);
    }
  }

  // map clean up
  pointer_id_entry_map_.RemoveAll(pointer_ids_to_remove);
}

void ResponsivenessMetrics::ContextmenuFlushTimerFired(TimerBase*) {
  // Pointerdown could be followed by a contextmenu without pointerup, in this
  // case we need to treat contextmenu as if pointerup and flush the previous
  // pointerdown with a valid interactionId. (crbug.com/1413096)
  FlushPointerdownAndPointerup();
  // Windows keyboard could have a contextmenu key and trigger keydown
  // followed by contextmenu when pressed. (crbug.com/1428603)
  FlushKeydown();
}

void ResponsivenessMetrics::FlushPointerdownAndPointerup() {
  LocalDOMWindow* window = window_performance_->DomWindow();
  if (!window) {
    return;
  }
  if (pointer_flush_timer_.IsActive()) {
    pointer_flush_timer_.Stop();
  }

  for (const auto& item : pointer_id_entry_map_) {
    PerformanceEventTiming* entry = item.value->GetEntry();
    if (entry->name() == event_type_names::kPointerdown &&
        item.value->GetTimeStamps().size() == 1u) {
      UpdateInteractionId();
      entry->SetInteractionIdAndOffset(GetCurrentInteractionId(),
                                       GetInteractionCount());
      // Pointerdown without pointerup nor click need to notify performance
      // observer since they haven't.
      NotifyPointerdown(entry);
    }
    RecordDragTapOrClickUKM(window, *item.value);
  }

  // map clean up
  pointer_id_entry_map_.clear();
}

void ResponsivenessMetrics::NotifyPointerdown(
    PerformanceEventTiming* entry) const {
  // We only delay dispatching entries when they are pointerdown.
  if (entry->name() != event_type_names::kPointerdown) {
    return;
  }

  window_performance_->NotifyAndAddEventTimingBuffer(entry);
}

// Flush UKM timestamps of composition events for testing.
void ResponsivenessMetrics::FlushAllEventsForTesting() {
  FlushSequenceBasedKeyboardEvents();
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
  visitor->Trace(pointer_flush_timer_);
  visitor->Trace(contextmenu_flush_timer_);
  visitor->Trace(composition_end_flush_timer_);
}

perfetto::protos::pbzero::WebContentInteraction::Type
ResponsivenessMetrics::UserInteractionTypeToProto(
    UserInteractionType interaction_type) const {
  using Interaction = perfetto::protos::pbzero::WebContentInteraction;
  switch (interaction_type) {
    case UserInteractionType::kDrag:
      return Interaction::INTERACTION_DRAG;
    case UserInteractionType::kKeyboard:
      return Interaction::INTERACTION_KEYBOARD;
    case UserInteractionType::kTapOrClick:
      return Interaction::INTERACTION_CLICK_TAP;
  }

  return Interaction::INTERACTION_UNSPECIFIED;
}

void ResponsivenessMetrics::EmitInteractionToNextPaintTraceEvent(
    const ResponsivenessMetrics::EventTimestamps& event,
    UserInteractionType interaction_type,
    base::TimeDelta total_event_duration) {
  const perfetto::Track track(base::trace_event::GetNextGlobalTraceId(),
                              perfetto::ProcessTrack::Current());
  TRACE_EVENT_BEGIN(
      "interactions", "Web Interaction", track, event.creation_time,
      [&](perfetto::EventContext& ctx) {
        auto* web_content_interaction =
            ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_web_content_interaction();
        web_content_interaction->set_type(
            UserInteractionTypeToProto(interaction_type));
        web_content_interaction->set_total_duration_ms(
            total_event_duration.InMilliseconds());
      });

  TRACE_EVENT_END("interactions", track, event.end_time);
}

// TODO(crbug.com/355605691): Report simulated clicks to UKM. We assume click
// is dispatched/simulated during handling of the keydown or keyup. Hence it is
// contained in either the duration of keydown or keyup. The total duration and
// max duration won't be affected. This assumption may not be true in all
// keyboard click scenarios and regardless whether it is true, we should report
// them.
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

  entry->SetInteractionIdAndOffset(
      last_keydown_keycode_info_->interactionId,
      last_keydown_keycode_info_->interactionOffset);

  return true;
}
}  // namespace blink
