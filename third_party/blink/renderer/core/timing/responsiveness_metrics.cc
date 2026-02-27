// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/responsiveness_metrics.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/histogram_scope.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/pointer_event_factory.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/timing/performance_event_timing.h"
#include "third_party/blink/renderer/core/timing/performance_timeline_entry_id_generator.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
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

void EmitInteractionToNextPaintTraceEvent(base::TimeTicks creation_time,
                                          base::TimeTicks end_time,
                                          bool is_pointer_event) {
  const perfetto::Track track(base::trace_event::GetNextGlobalTraceId(),
                              perfetto::ProcessTrack::Current());
  TRACE_EVENT_BEGIN(
      "interactions", "Web Interaction", track, creation_time,
      [&](perfetto::EventContext& ctx) {
        using Interaction = perfetto::protos::pbzero::WebContentInteraction;

        auto* web_content_interaction =
            ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_web_content_interaction();
        web_content_interaction->set_type(
            is_pointer_event ? Interaction::INTERACTION_CLICK_TAP
                             : Interaction::INTERACTION_KEYBOARD);
      });

  TRACE_EVENT_END("interactions", track, end_time);
}

void EmitSlowInteractionToNextPaintTraceEvent(base::TimeTicks creation_time,
                                              base::TimeTicks end_time,
                                              uint64_t event_id) {
  auto track =
      perfetto::Track::Global(base::trace_event::GetNextGlobalTraceId());
  TRACE_EVENT_BEGIN(kSlowInteractionToNextPaintTraceEventCategory,
                    kSlowInteractionToNextPaintTraceEventName, track,
                    creation_time);
  TRACE_EVENT_END(kSlowInteractionToNextPaintTraceEventCategory, track,
                  end_time, perfetto::Flow::Global(event_id));
}

std::unique_ptr<TracedValue> UserInteractionTraceData(base::TimeDelta duration,
                                                      bool is_pointer) {
  auto traced_value = std::make_unique<TracedValue>();
  traced_value->SetInteger("maxDuration",
                           static_cast<int>(duration.InMilliseconds()));
  traced_value->SetString("interactionType",
                          is_pointer ? "tapOrClick" : "keyboard");
  return traced_value;
}

void LogResponsivenessHistogram(base::TimeDelta duration, const char* suffix) {
  base::UmaHistogramCustomTimes(
      base::StrCat({kHistogramMaxEventDuration, suffix}), duration,
      base::Milliseconds(1), base::Seconds(60), 50);
}

bool IsEventTypeForInteractionId(const AtomicString& type) {
  return type == event_type_names::kPointercancel ||
         type == event_type_names::kContextmenu ||
         type == event_type_names::kPointerdown ||
         type == event_type_names::kPointerup ||
         type == event_type_names::kClick ||
         type == event_type_names::kKeydown ||
         type == event_type_names::kKeypress ||
         type == event_type_names::kKeyup ||
         type == event_type_names::kCompositionstart ||
         type == event_type_names::kCompositionupdate ||
         type == event_type_names::kCompositionend ||
         type == event_type_names::kInput;
}

}  // namespace

ResponsivenessMetrics::ResponsivenessMetrics(
    WindowPerformance* window_performance)
    : window_performance_(window_performance) {}

ResponsivenessMetrics::~ResponsivenessMetrics() = default;

void ResponsivenessMetrics::TryAssignInteractionId(
    PerformanceEventTiming* new_entry) {
  CHECK(new_entry);
  CHECK(!new_entry->HasKnownInteractionID());

  const AtomicString& event_type = new_entry->name();
  if (!IsEventTypeForInteractionId(event_type)) {
    SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
    return;
  }

  const auto& reporting_info = *new_entry->GetEventTimingReportingInfo();

  // 1. Composition events and Input events always use the composition handler.
  if (event_type == event_type_names::kCompositionstart ||
      event_type == event_type_names::kCompositionupdate ||
      event_type == event_type_names::kCompositionend ||
      event_type == event_type_names::kInput) {
    HandleCompositionInteraction(new_entry);
    return;
  }

  // 2. Keyboard events use composition handler if active, else standard
  // keyboard handler.
  if (reporting_info.key_code.has_value()) {
    if (composition_state_ != kNonComposition) {
      HandleCompositionInteraction(new_entry);
    } else {
      HandleKeyboardInteraction(new_entry);
    }
    return;
  }

  // 3. Pointer and Click events always use the pointer handler, even if the
  // click results from a keyboard interaction, for example.
  if (reporting_info.pointer_id.has_value() ||
      event_type == event_type_names::kClick) {
    HandlePointerInteraction(new_entry);
    return;
  }

  // All known events and dispatch flows should be handled by one of the
  // pathways above.
  NOTREACHED(base::NotFatalUntil::M151);

  // This is a catch-all, just in case, because without assigning a known
  // interactionID the whole event queue gets stuck.
  SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
}

void ResponsivenessMetrics::HandleKeyboardInteraction(
    PerformanceEventTiming* new_entry) {
  CHECK(new_entry);
  const AtomicString& event_type = new_entry->name();
  const int key_code =
      new_entry->GetEventTimingReportingInfo()->key_code.value();

  PerformanceTimelineEntryIdInfo interaction_id_for_keycode =
      PerformanceTimelineEntryIdInfo::kNone;

  if (event_type == event_type_names::kKeydown) {
    interaction_id_for_keycode = AssignNewKeyboardInteractionId(key_code);
    SetInteractionId(new_entry, interaction_id_for_keycode);
    return;
  }

  auto it = keycode_to_interactionid_.find(key_code);
  if (it != keycode_to_interactionid_.end()) {
    interaction_id_for_keycode = it->value;
  }

  // At this point, |keydown_interaction_id| may still be |kNone| if there was
  // no recent |keydown| for this keyCode.  We can unconditionally assign to the
  // next key events to match this interactionId (none or not-none).

  if (event_type == event_type_names::kKeyup) {
    SetInteractionId(new_entry, interaction_id_for_keycode);
    // Clear the keycode map, but not |last_keydown_interaction_id_|, because
    // a click event may follow with |kReservedNonPointerId| and the only way
    // to match is using the most recent keydown event.
    if (it != keycode_to_interactionid_.end()) {
      keycode_to_interactionid_.erase(it);
    }
    return;
  }

  if (event_type == event_type_names::kKeypress) {
    SetInteractionId(new_entry, interaction_id_for_keycode);
    return;
  }

  // All known events and dispatch flows should be handled by one of the
  // pathways above.
  NOTREACHED(base::NotFatalUntil::M151);

  SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
}

void ResponsivenessMetrics::HandleCompositionInteraction(
    PerformanceEventTiming* new_entry) {
  CHECK(new_entry);
  const AtomicString& event_type = new_entry->name();

  // If we aren't already in composition mode, event must be compositionstart.
  if (composition_state_ == kNonComposition &&
      event_type != event_type_names::kCompositionstart) {
    SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
    return;
  }

  if (event_type == event_type_names::kCompositionstart) {
    // Apparently we can get multiple compositionstart in some tests.
    // This seems to happen when you move the IME input to another part of
    // layout, so maybe compositionstart without compositionend is a type of
    // "restart" notification.
    // The state machine supports this, just don't expect to CHECK() the current
    // composition state.
    composition_state_ = kCompositionActive;
    SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
    return;
  }

  // Immediately after compositionend we treat the next input & keyup as the
  // last composition related events-- otherwise we reset to non-composition
  // mode, and move to the keyboard handler.
  if (composition_state_ == kCompositionEndOnKeyup &&
      event_type != event_type_names::kInput &&
      event_type != event_type_names::kKeyup) {
    composition_state_ = kNonComposition;
    CHECK(new_entry->GetEventTimingReportingInfo()->key_code.has_value());
    HandleKeyboardInteraction(new_entry);
    return;
  }

  if (event_type == event_type_names::kCompositionupdate) {
    CHECK_EQ(composition_state_, kCompositionActive);
    SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
    return;
  }

  if (event_type == event_type_names::kCompositionend) {
    CHECK_EQ(composition_state_, kCompositionActive);
    composition_state_ = kCompositionEndOnKeyup;
    SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
    return;
  }

  if (event_type == event_type_names::kKeydown) {
    CHECK_EQ(composition_state_, kCompositionActive);
    last_keydown_interaction_id_ = AssignNewKeyboardInteractionId(
        new_entry->GetEventTimingReportingInfo()->key_code.value());
    SetInteractionId(new_entry, *last_keydown_interaction_id_);
    return;
  }

  if (event_type == event_type_names::kKeypress) {
    CHECK_EQ(composition_state_, kCompositionActive);
    SetInteractionId(new_entry, interaction_id_generator_.GetValue());
    return;
  }

  if (event_type == event_type_names::kKeyup) {
    // This should be equivalent to `last_keydown_interaction_id_` but input
    // events currently clear this value.
    SetInteractionId(new_entry, interaction_id_generator_.GetValue());
    if (composition_state_ == kCompositionEndOnKeyup) {
      composition_state_ = kNonComposition;
    }
    last_keydown_interaction_id_ = std::nullopt;
    return;
  }

  if (event_type == event_type_names::kInput) {
    if (last_keydown_interaction_id_) {
      SetInteractionId(new_entry, *last_keydown_interaction_id_);
    } else {
      SetInteractionId(new_entry, interaction_id_generator_.IncrementId());
    }
    last_keydown_interaction_id_ = std::nullopt;
    return;
  }

  // All known events and dispatch flows should be handled by one of the
  // pathways above.
  NOTREACHED(base::NotFatalUntil::M151);

  SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
}

void ResponsivenessMetrics::HandlePointerInteraction(
    PerformanceEventTiming* new_entry) {
  CHECK(new_entry);
  const AtomicString& event_type = new_entry->name();

  CHECK(new_entry->GetEventTimingReportingInfo()->pointer_id.has_value());
  PointerId pointer_id =
      new_entry->GetEventTimingReportingInfo()->pointer_id.value();

  PerformanceEventTiming* pending_pointerdown_entry = nullptr;
  std::optional<PerformanceTimelineEntryIdInfo> interaction_id_for_pointerid;

  // Per spec, only contextmenu and click events triggered by keyboard can have
  // |kReservedNonPointerId| pointerid value-- and so they cannot be in the map.
  // But we cannot do lookup in the map for -1, since that is a reserved value.
  if (pointer_id != PointerEventFactory::kReservedNonPointerId) {
    if (auto it = pending_pointerdown_entries_.find(pointer_id);
        it != pending_pointerdown_entries_.end()) {
      // Found a pending pointerdown.  We cannot have a known interaction id for
      // this pointer id in our map.
      CHECK(!pointerid_to_interactionid_.Contains(pointer_id));
      pending_pointerdown_entry = it->value.Get();
    } else if (auto id_it = pointerid_to_interactionid_.find(pointer_id);
               id_it != pointerid_to_interactionid_.end()) {
      // We no longer have a pending pointerdown, but we do have a known
      // interaction id for this pointer id in our map.
      interaction_id_for_pointerid = id_it->value;
    }
  }

  if (event_type == event_type_names::kPointerdown) {
    if (pending_pointerdown_entry) {
      // "Cancel" any pending pointerdown for this same pointer id.
      SetInteractionId(pending_pointerdown_entry,
                       PerformanceTimelineEntryIdInfo::kNone);
    }
    CHECK(pointer_id != PointerEventFactory::kReservedNonPointerId);
    pending_pointerdown_entries_.Set(pointer_id, new_entry);
    pointerid_to_interactionid_.erase(pointer_id);
    return;
  }

  if (event_type == event_type_names::kPointerup) {
    if (pending_pointerdown_entry) {
      interaction_id_for_pointerid = AssignNewPointerInteractionId(pointer_id);
      SetInteractionId(pending_pointerdown_entry,
                       *interaction_id_for_pointerid);
      pending_pointerdown_entries_.erase(pointer_id);
    }
    // Pointerup without Pointerdown gets 0 id.
    SetInteractionId(new_entry, interaction_id_for_pointerid.value_or(
                                    PerformanceTimelineEntryIdInfo::kNone));
    return;
  }

  if (event_type == event_type_names::kContextmenu) {
    if (pending_pointerdown_entry) {
      interaction_id_for_pointerid = AssignNewPointerInteractionId(pointer_id);
      SetInteractionId(pending_pointerdown_entry,
                       *interaction_id_for_pointerid);
      pending_pointerdown_entries_.erase(pointer_id);
    }
    SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
    return;
  }

  if (event_type == event_type_names::kClick) {
    // Uncommon: Non-pointer click events (keyboard or accessibility devices).
    if (pointer_id == PointerEventFactory::kReservedNonPointerId &&
        last_keydown_interaction_id_) {
      SetInteractionId(new_entry, *last_keydown_interaction_id_);
      last_keydown_interaction_id_ = std::nullopt;
      return;
    }
    // Common: Pointer click following pointer events.
    if (interaction_id_for_pointerid) {
      SetInteractionId(new_entry, *interaction_id_for_pointerid);
      pointerid_to_interactionid_.erase(pointer_id);
      return;
    }
    // Very Rare: Pointerdown -> Click without pointerup.
    if (pending_pointerdown_entry) {
      interaction_id_for_pointerid = AssignNewPointerInteractionId(pointer_id);
      SetInteractionId(pending_pointerdown_entry,
                       *interaction_id_for_pointerid);
      SetInteractionId(new_entry, *interaction_id_for_pointerid);
      pending_pointerdown_entries_.erase(pointer_id);
      return;
    }
    // Uncommon: Click event all on its own. Still gets a new id.
    SetInteractionId(new_entry, interaction_id_generator_.IncrementId());
    return;
  }

  if (event_type == event_type_names::kPointercancel) {
    if (pending_pointerdown_entry) {
      SetInteractionId(pending_pointerdown_entry,
                       PerformanceTimelineEntryIdInfo::kNone);
      pending_pointerdown_entries_.erase(pointer_id);
    }
    // We erase this here, but a pointerup will follow.  It will get id of 0,
    // so its fine, but we could also just leave this in the map.
    pointerid_to_interactionid_.erase(pointer_id);
    SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
    return;
  }

  // All known events and dispatch flows should be handled by one of the
  // pathways above.
  NOTREACHED(base::NotFatalUntil::M151);

  SetInteractionId(new_entry, PerformanceTimelineEntryIdInfo::kNone);
}

void ResponsivenessMetrics::SetInteractionId(
    PerformanceEventTiming* entry,
    PerformanceTimelineEntryIdInfo id) {
  CHECK(entry);
  entry->SetInteractionIdInfo(id);
}

PerformanceTimelineEntryIdInfo
ResponsivenessMetrics::AssignNewKeyboardInteractionId(int key_code) {
  PerformanceTimelineEntryIdInfo id = interaction_id_generator_.IncrementId();
  keycode_to_interactionid_.Set(key_code, id);
  last_keydown_interaction_id_ = id;
  return id;
}

PerformanceTimelineEntryIdInfo
ResponsivenessMetrics::AssignNewPointerInteractionId(PointerId pointer_id) {
  PerformanceTimelineEntryIdInfo id = interaction_id_generator_.IncrementId();
  pointerid_to_interactionid_.Set(pointer_id, id);
  return id;
}

void ResponsivenessMetrics::CommitAllPendingPointerdowns() {
  for (auto& entry : pending_pointerdown_entries_.Values()) {
    PointerId pointer_id =
        entry->GetEventTimingReportingInfo()->pointer_id.value();
    SetInteractionId(entry, AssignNewPointerInteractionId(pointer_id));
  }
  // This leaves all the known pointerid -> interactionid in the map of ids.
  // Arguably, if we are forcing / flushing all of these, we could also drop
  // all those values instead.
  pending_pointerdown_entries_.clear();
}

void ResponsivenessMetrics::ReportToMetrics(PerformanceEventTiming* entry) {
  CHECK(entry);
  CHECK(entry->IsReadyForReporting());

  LocalDOMWindow* window = window_performance_->DomWindow();
  if (!window) {
    return;
  }

  // Skip reporting EventTiming entries that are not interactions or are
  // explicitly ignored for reporting purposes.
  if (entry->GetInteractionIdInfo()->id ==
          PerformanceTimelineEntryIdInfo::kNoId ||
      entry->GetEventTimingReportingInfo()->prevent_counting_as_interaction) {
    return;
  }

  std::optional<PointerId> pointer_id =
      entry->GetEventTimingReportingInfo()->pointer_id;
  UserInteractionType interaction_type =
      (pointer_id.has_value() &&
       *pointer_id != PointerEventFactory::kReservedNonPointerId)
          ? UserInteractionType::kTapOrClick
          : UserInteractionType::kKeyboard;

  RecordUserInteractionUKM(window, interaction_type, *entry);
  RecordUserInteractionTracing(window, interaction_type, *entry);

  // For Histogram convenience, we only report "unique" interaction durations.
  // I.e. when keydown and keypress, or pointerup and click, report in the
  // same animation frame, we don't duplicate reports.  The first event for each
  // interaction id should always be the longest.  If they have the same end
  // time, they perfectly overlap in time and don't need to be repeated.
  uint64_t frame_index = entry->GetEventTimingReportingInfo()->frame_index;
  if (!last_recorded_frame_index_.has_value() ||
      frame_index != *last_recorded_frame_index_) {
    last_recorded_frame_index_ = frame_index;
    reported_interactions_in_frame_.clear();
  }

  ReportedInteractionKey key{entry->GetInteractionIdInfo()->id,
                             entry->GetEndTime()};

  if (!reported_interactions_in_frame_.Contains(key)) {
    reported_interactions_in_frame_.emplace_back(key);
    RecordUserInteractionHistograms(interaction_type, *entry);
  }
}

void ResponsivenessMetrics::RecordUserInteractionUKM(
    LocalDOMWindow* window,
    UserInteractionType interaction_type,
    const PerformanceEventTiming& entry) {
  const auto* reporting_info = entry.GetEventTimingReportingInfo();
  base::TimeTicks event_start = reporting_info->creation_time;
  base::TimeTicks event_end = entry.GetEndTime();
  base::TimeTicks event_queued_main_thread =
      reporting_info->enqueued_to_main_thread_time;
  base::TimeTicks event_commit_finish = reporting_info->commit_finish_time;
  base::TimeDelta duration = event_end - event_start;

  uint32_t interaction_offset = entry.GetInteractionIdInfo()->offset;

  if (!event_start.is_null() && duration.InMilliseconds() >= 0) {
    if (window->GetFrame()) {
      window->GetFrame()->Client()->DidObserveUserInteraction(
          event_start, event_queued_main_thread, event_commit_finish, event_end,
          interaction_offset);
    }
  }

  ukm::UkmRecorder* ukm_recorder = window->UkmRecorder();
  ukm::SourceId source_id = window->UkmSourceID();
  if (source_id != ukm::kInvalidSourceId &&
      (!sampling_ || base::RandInt(kMinValueForSampling,
                                   kMaxValueForSampling) <= kUkmSamplingRate)) {
    ukm::builders::Responsiveness_UserInteraction(source_id)
        .SetInteractionType(static_cast<int64_t>(interaction_type))
        .SetMaxEventDuration(duration.InMilliseconds())
        .Record(ukm_recorder);
  }
}

void ResponsivenessMetrics::RecordUserInteractionHistograms(
    UserInteractionType interaction_type,
    const PerformanceEventTiming& entry) {
  base::TimeDelta duration = entry.GetExactDuration();
  uint64_t event_id = base::trace_event::GetNextGlobalTraceId();
  base::trace_event::HistogramScope scoped_event(event_id);
  LogResponsivenessHistogram(duration, kHistogramAllTypes);
  if (interaction_type == UserInteractionType::kTapOrClick) {
    LogResponsivenessHistogram(duration, kHistogramTapOrClick);
  } else {
    LogResponsivenessHistogram(duration, kHistogramKeyboard);
  }
}

void ResponsivenessMetrics::RecordUserInteractionTracing(
    LocalDOMWindow* window,
    UserInteractionType interaction_type,
    const PerformanceEventTiming& entry) {
  base::TimeDelta duration = entry.GetExactDuration();

  bool is_pointer_event = interaction_type == UserInteractionType::kTapOrClick;

  TRACE_EVENT("devtools.timeline", "Responsiveness.Renderer.UserInteraction",
              "data", UserInteractionTraceData(duration, is_pointer_event),
              "frame", GetFrameIdForTracing(window->GetFrame()));

  EmitInteractionToNextPaintTraceEvent(
      entry.GetEventTimingReportingInfo()->creation_time, entry.GetEndTime(),
      is_pointer_event);

  uint64_t event_id = base::trace_event::GetNextGlobalTraceId();
  constexpr base::TimeDelta kSlowInteractionToNextPaintThreshold =
      base::Milliseconds(100);
  if (duration > kSlowInteractionToNextPaintThreshold) {
    EmitSlowInteractionToNextPaintTraceEvent(
        entry.GetEventTimingReportingInfo()->creation_time, entry.GetEndTime(),
        event_id);
  }
}

void ResponsivenessMetrics::FlushAllEvents() {
  CommitAllPendingPointerdowns();
  keycode_to_interactionid_.clear();
  pointerid_to_interactionid_.clear();
  last_keydown_interaction_id_ = std::nullopt;
  reported_interactions_in_frame_.clear();
  last_recorded_frame_index_ = std::nullopt;
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

void ResponsivenessMetrics::Trace(Visitor* visitor) const {
  visitor->Trace(window_performance_);
  visitor->Trace(pending_pointerdown_entries_);
}

}  // namespace blink
