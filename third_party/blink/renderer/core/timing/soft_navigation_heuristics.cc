// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

namespace {

const size_t SOFT_NAVIGATION_PAINT_AREA_PRECENTAGE = 2;
const size_t HUNDRED_PERCENT = 100;

const char kPageLoadInternalSoftNavigationOutcome[] =
    "PageLoad.Internal.SoftNavigationOutcome";

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SoftNavigationOutcome" in tools/metrics/histograms/enums.xml. Note also that
// these form a bitmask; future conditions should continue this pattern.
// LINT.IfChange
enum SoftNavigationOutcome {
  kSoftNavigationDetected = 0,

  kNoSoftNavContextDuringUrlChange = 1,
  kNoPaint = 2,
  kNoDomModification = 4,

  kNoPaintOrDomModification = kNoPaint | kNoDomModification,

  kMaxValue = kNoPaintOrDomModification,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:SoftNavigationOutcome)

void LogAndTraceDetectedSoftNavigation(LocalFrame* frame,
                                       LocalDOMWindow* window,
                                       const SoftNavigationContext& context) {
  CHECK(frame && frame->IsMainFrame());
  CHECK(window);
  if (!RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window)) {
    return;
  }
  auto* console_message = MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kInfo,
      String("A soft navigation has been detected: ") + context.Url());
  window->AddConsoleMessage(console_message);

  TRACE_EVENT_INSTANT("scheduler,devtools.timeline,loading",
                      "SoftNavigationHeuristics_SoftNavigationDetected",
                      context.UserInteractionTimestamp(), "frame",
                      GetFrameIdForTracing(frame), "url", context.Url(),
                      "navigationId", window->GetNavigationId());
}

constexpr bool IsInteractionStart(
    SoftNavigationHeuristics::EventScope::Type type) {
  return (type == SoftNavigationHeuristics::EventScope::Type::kClick ||
          type == SoftNavigationHeuristics::EventScope::Type::kKeydown ||
          type == SoftNavigationHeuristics::EventScope::Type::kNavigate);
}

constexpr bool IsInteractionEnd(
    SoftNavigationHeuristics::EventScope::Type type) {
  return (type == SoftNavigationHeuristics::EventScope::Type::kClick ||
          type == SoftNavigationHeuristics::EventScope::Type::kKeyup ||
          type == SoftNavigationHeuristics::EventScope::Type::kNavigate);
}

std::optional<SoftNavigationHeuristics::EventScope::Type>
EventScopeTypeFromEvent(const Event& event) {
  if (!event.isTrusted()) {
    return std::nullopt;
  }
  if (event.IsMouseEvent() && event.type() == event_type_names::kClick) {
    return SoftNavigationHeuristics::EventScope::Type::kClick;
  }
  if (event.type() == event_type_names::kNavigate) {
    return SoftNavigationHeuristics::EventScope::Type::kNavigate;
  }
  if (event.IsKeyboardEvent()) {
    Node* target_node = event.target() ? event.target()->ToNode() : nullptr;
    if (target_node && target_node->IsHTMLElement() &&
        DynamicTo<HTMLElement>(target_node)->IsHTMLBodyElement()) {
      if (event.type() == event_type_names::kKeydown) {
        return SoftNavigationHeuristics::EventScope::Type::kKeydown;
      } else if (event.type() == event_type_names::kKeypress) {
        return SoftNavigationHeuristics::EventScope::Type::kKeypress;
      } else if (event.type() == event_type_names::kKeyup) {
        return SoftNavigationHeuristics::EventScope::Type::kKeyup;
      }
    }
  }
  return std::nullopt;
}

}  // namespace

// static
const char SoftNavigationHeuristics::kSupplementName[] =
    "SoftNavigationHeuristics";

SoftNavigationHeuristics::SoftNavigationHeuristics(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {
  LocalFrame* frame = window.GetFrame();
  CHECK(frame && frame->View());

  viewport_area_ = frame->View()->GetLayoutSize().Area64();
}

SoftNavigationHeuristics* SoftNavigationHeuristics::From(
    LocalDOMWindow& window) {
  if (!base::FeatureList::IsEnabled(features::kSoftNavigationDetection)) {
    return nullptr;
  }
  if (!window.GetFrame()->IsMainFrame()) {
    return nullptr;
  }
  SoftNavigationHeuristics* heuristics =
      Supplement<LocalDOMWindow>::From<SoftNavigationHeuristics>(window);
  if (!heuristics) {
    if (Document* document = window.document()) {
      // Don't measure soft navigations in devtools.
      if (document->Url().ProtocolIs("devtools")) {
        return nullptr;
      }
    }
    heuristics = MakeGarbageCollected<SoftNavigationHeuristics>(window);
    ProvideTo(window, heuristics);
  }
  return heuristics;
}

void SoftNavigationHeuristics::Dispose() {
  for (const auto& context : potential_soft_navigations_) {
    RecordUmaForNonSoftNavigationInteraction(*context.Get());
  }
}

void SoftNavigationHeuristics::RecordUmaForNonSoftNavigationInteraction(
    const SoftNavigationContext& context) const {
  // For all interactions which included a (soft nav context attributable) URL
  // modification, yet were not declared soft navs, log the criteria which were
  // not met.
  if (context.Url().empty()) {
    return;
  }

  if (!paint_conditions_met_ && !context.HasMainModification()) {
    base::UmaHistogramEnumeration(
        kPageLoadInternalSoftNavigationOutcome,
        SoftNavigationOutcome::kNoPaintOrDomModification);
  } else if (!paint_conditions_met_) {
    base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                  SoftNavigationOutcome::kNoPaint);
  } else if (!context.HasMainModification()) {
    base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                  SoftNavigationOutcome::kNoDomModification);
  }
}

void SoftNavigationHeuristics::SetIsTrackingSoftNavigationHeuristicsOnDocument(
    bool value) const {
  LocalDOMWindow* window = GetSupplementable();
  if (!window) {
    return;
  }
  if (Document* document = window->document()) {
    document->SetIsTrackingSoftNavigationHeuristics(value);
  }
}

void SoftNavigationHeuristics::ResetHeuristic() {
  // Reset previously seen indicators and task IDs.
  potential_soft_navigations_.clear();
  last_detected_soft_navigation_ = nullptr;
  active_interaction_context_ = nullptr;
  SetIsTrackingSoftNavigationHeuristicsOnDocument(false);
  did_commit_previous_paints_ = false;
  paint_conditions_met_ = false;
  softnav_painted_area_ = 0;
}

SoftNavigationContext*
SoftNavigationHeuristics::GetSoftNavigationContextForCurrentTask() {
  if (potential_soft_navigations_.empty()) {
    return nullptr;
  }
  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetSupplementable()->GetIsolate());
  // The `tracker` must exist if `potential_soft_navigations_` is non-empty.
  CHECK(tracker);
  auto* task_state = tracker->RunningTask();
  if (!task_state) {
    return nullptr;
  }
  SoftNavigationContext* context =
      task_state ? task_state->GetSoftNavigationContext() : nullptr;
  // `task_state` can have null `context` in tests. `context` can be non-null
  // but not in `potential_soft_navigations_` if the heuristic was reset, e.g.
  // if `context` was already considered a soft navigation. In that case, return
  // null.
  if (!context || !potential_soft_navigations_.Contains(context)) {
    return nullptr;
  }
  return context;
}

std::optional<scheduler::TaskAttributionId>
SoftNavigationHeuristics::AsyncSameDocumentNavigationStarted() {
  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetSupplementable()->GetIsolate());
  // `tracker` will be null if TaskAttributionInfrastructureDisabledForTesting
  // is enabled.
  if (!tracker) {
    return std::nullopt;
  }
  scheduler::TaskAttributionInfo* task_state = tracker->RunningTask();
  SoftNavigationContext* context =
      task_state ? task_state->GetSoftNavigationContext() : nullptr;
  TRACE_EVENT1("scheduler",
               "SoftNavigationHeuristics::AsyncSameDocumentNavigationStarted",
               "has_context", !!context);
  if (context) {
    tracker->AddSameDocumentNavigationTask(task_state);
  }
  return context ? std::optional<scheduler::TaskAttributionId>(task_state->Id())
                 : std::nullopt;
}

void SoftNavigationHeuristics::SameDocumentNavigationCommitted(
    const String& url,
    SoftNavigationContext* context) {
  TRACE_EVENT2("scheduler",
               "SoftNavigationHeuristics::SameDocumentNavigationCommitted",
               "url", url, "has_context", !!context);
  if (context) {
    if (potential_soft_navigations_.Contains(context)) {
      context->SetUrl(url);
      EmitSoftNavigationEntryIfAllConditionsMet(context);
    }
  } else {
    base::UmaHistogramEnumeration(
        kPageLoadInternalSoftNavigationOutcome,
        SoftNavigationOutcome::kNoSoftNavContextDuringUrlChange);
  }
}

bool SoftNavigationHeuristics::ModifiedDOM() {
  SoftNavigationContext* context = GetSoftNavigationContextForCurrentTask();
  if (context) {
    context->MarkMainModification();
    EmitSoftNavigationEntryIfAllConditionsMet(context);
  }
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::ModifiedDOM",
               "has_context", !!context);
  return !!context;
}

void SoftNavigationHeuristics::EmitSoftNavigationEntryIfAllConditionsMet(
    SoftNavigationContext* context) {
  // If there's an `EventScope` on the stack, hold off checking to avoid
  // clearing state while it's in use.
  if (has_active_event_scope_) {
    return;
  }

  LocalFrame* frame = GetLocalFrameIfNotDetached();
  // TODO(crbug.com/1510706): See if we need to add `paint_conditions_met_` back
  // into this condition.
  if (!context || !context->IsSoftNavigation() ||
      context->UserInteractionTimestamp().is_null() || !frame ||
      !frame->IsOutermostMainFrame()) {
    return;
  }
  last_detected_soft_navigation_ = context;

  LocalDOMWindow* window = GetSupplementable();
  ++soft_navigation_count_;
  window->GenerateNewNavigationId();
  auto* performance = DOMWindowPerformance::performance(*window);
  performance->AddSoftNavigationEntry(AtomicString(context->Url()),
                                      context->UserInteractionTimestamp());

  CommitPreviousPaints(frame);

  LogAndTraceDetectedSoftNavigation(frame, window, *context);
  ReportSoftNavigationToMetrics(frame, context);
  ResetHeuristic();
}

// This is called from Text/ImagePaintTimingDetector when a paint is recorded
// there.
void SoftNavigationHeuristics::RecordPaint(
    LocalFrame* frame,
    uint64_t painted_area,
    bool is_modified_by_soft_navigation) {
  if (!initial_interaction_encountered_ && is_modified_by_soft_navigation) {
    // TODO(crbug.com/41496928): Paints can be reported for Nodes which had
    // is_modified... flag set but a different instance of a
    // SoftNavigationHeuristics class.  This happens when Nodes are re-parented
    // into a new document, e.g. into an open() window.
    // Instead of just ignoring the worst case of this issue as we do here, we
    // should support this use case.  Either by clearing the flag on nodes, or,
    // by staring an interaction/navigation id on Node, rathan than boolean.
    return;
  }
  if (!initial_interaction_encountered_) {
    // We haven't seen an interaction yet, so we are still measuring initial
    // paint area.
    CHECK(!is_modified_by_soft_navigation);
    initial_painted_area_ += painted_area;
    return;
  }

  if (potential_soft_navigations_.empty()) {
    // We aren't measuring a soft-nav so we can just exit.
    return;
  }

  if (!is_modified_by_soft_navigation) {
    return;
  }

  softnav_painted_area_ += painted_area;

  uint64_t required_paint_area =
      std::min(initial_painted_area_, viewport_area_);

  if (required_paint_area == 0) {
    return;
  }

  float softnav_painted_area_ratio =
      (float)softnav_painted_area_ / (float)required_paint_area;

  uint64_t required_paint_area_scaled =
      required_paint_area * SOFT_NAVIGATION_PAINT_AREA_PRECENTAGE;
  uint64_t softnav_painted_area_scaled =
      softnav_painted_area_ * HUNDRED_PERCENT;
  bool is_above_threshold =
      (softnav_painted_area_scaled > required_paint_area_scaled);

  TRACE_EVENT_INSTANT(
      "loading", "SoftNavigationHeuristics_RecordPaint", "softnav_painted_area",
      softnav_painted_area_, "softnav_painted_area_ratio",
      softnav_painted_area_ratio, "url",
      (last_detected_soft_navigation_ ? last_detected_soft_navigation_->Url()
                                      : ""),
      "is_above_threshold", is_above_threshold);

  // TODO(crbug.com/1510706): GC between DOM modification and paint could cause
  // `last_detected_soft_navigation_` to be cleared, preventing the entry from
  // being emitted if `paint_conditions_met_` wasn't set but will be in the
  // subsequent paint. This problem existed in task attribution v1 as well since
  // the heuristic is reset when `potential_soft_navigations_` becomes empty.
  if (is_above_threshold) {
    paint_conditions_met_ = true;
    EmitSoftNavigationEntryIfAllConditionsMet(
        last_detected_soft_navigation_.Get());
  }
}

void SoftNavigationHeuristics::ReportSoftNavigationToMetrics(
    LocalFrame* frame,
    SoftNavigationContext* context) const {
  auto* loader = frame->Loader().GetDocumentLoader();

  if (!loader) {
    return;
  }

  CHECK(!context->UserInteractionTimestamp().is_null());
  auto soft_navigation_start_time =
      loader->GetTiming().MonotonicTimeToPseudoWallTime(
          context->UserInteractionTimestamp());

  LocalDOMWindow* window = GetSupplementable();

  blink::SoftNavigationMetrics metrics = {soft_navigation_count_,
                                          soft_navigation_start_time,
                                          window->GetNavigationId().Utf8()};

  if (LocalFrameClient* frame_client = frame->Client()) {
    // This notifies UKM about this soft navigation.
    frame_client->DidObserveSoftNavigation(metrics);
  }

  // Count "successful soft nav" in histogram
  base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                SoftNavigationOutcome::kSoftNavigationDetected);
}

void SoftNavigationHeuristics::ResetPaintsIfNeeded() {
  LocalFrame* frame = GetLocalFrameIfNotDetached();
  if (!frame || !frame->IsOutermostMainFrame()) {
    return;
  }
  LocalFrameView* local_frame_view = frame->View();
  CHECK(local_frame_view);
  LocalDOMWindow* window = GetSupplementable();
  if (RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window)) {
    if (Document* document = window->document();
        document &&
        RuntimeEnabledFeatures::SoftNavigationHeuristicsExposeFPAndFCPEnabled(
            window)) {
      PaintTiming::From(*document).ResetFirstPaintAndFCP();
    }
    local_frame_view->GetPaintTimingDetector().RestartRecordingLCP();
  }

  local_frame_view->GetPaintTimingDetector().RestartRecordingLCPToUkm();
}

// Once all the soft navigation conditions are met (verified in
// `EmitSoftNavigationEntryIfAllConditionsMet()`), the previous paints are
// committed, to make sure accumulated FP, FCP and LCP entries are properly
// fired.
void SoftNavigationHeuristics::CommitPreviousPaints(LocalFrame* frame) {
  CHECK(frame && frame->IsOutermostMainFrame());
  LocalDOMWindow* window = GetSupplementable();
  if (!did_commit_previous_paints_) {
    LocalFrameView* local_frame_view = frame->View();

    CHECK(local_frame_view);

    local_frame_view->GetPaintTimingDetector().SoftNavigationDetected(window);
    if (RuntimeEnabledFeatures::SoftNavigationHeuristicsExposeFPAndFCPEnabled(
            window)) {
      PaintTiming::From(*window->document()).SoftNavigationDetected();
    }

    did_commit_previous_paints_ = true;
  }
}

void SoftNavigationHeuristics::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
  visitor->Trace(last_detected_soft_navigation_);
  visitor->Trace(active_interaction_context_);
  // Register a custom weak callback, which runs after processing weakness for
  // the container. This allows us to observe the collection becoming empty
  // without needing to observe individual element disposal.
  visitor->RegisterWeakCallbackMethod<
      SoftNavigationHeuristics,
      &SoftNavigationHeuristics::ProcessCustomWeakness>(this);
}

void SoftNavigationHeuristics::OnCreateTaskScope(
    scheduler::TaskAttributionInfo& task_state) {
  CHECK(active_interaction_context_);
  // A task scope can be created without a `SoftNavigationContext` or one that
  // differs from the one associated with the current `EventScope` if, for
  // example, a previously created and awaited promise is resolved in an event
  // handler.
  if (task_state.GetSoftNavigationContext() !=
      active_interaction_context_.Get()) {
    return;
  }

  // TODO(crbug.com/40942324): Replace task_id with either an id for the
  // `SoftNavigationContext` or a serialized version of the object.
  TRACE_EVENT1("scheduler", "SoftNavigationHeuristics::OnCreateTaskScope",
               "task_id", task_state.Id().value());
  // This is invoked when executing a callback with an active `EventScope`,
  // which happens for click and keyboard input events, as well as
  // user-initiated navigation and popstate events. Running such an event
  // listener "activates" the `SoftNavigationContext` as a candidate soft
  // navigation.
  initial_interaction_encountered_ = true;
  SetIsTrackingSoftNavigationHeuristicsOnDocument(true);
}

void SoftNavigationHeuristics::ProcessCustomWeakness(
    const LivenessBroker& info) {
  if (potential_soft_navigations_.empty()) {
    return;
  }
  // When all the soft navigation tasks were garbage collected, that means that
  // all their descendant tasks are done, and there's no need to continue
  // searching for soft navigation signals, at least not until the next user
  // interaction.
  //
  // Note: This is not allowed to do Oilpan allocations. If that's needed, this
  // can schedule a task or microtask to reset the heuristic.
  Vector<UntracedMember<SoftNavigationContext>> dead_contexts;
  for (const auto& context : potential_soft_navigations_) {
    if (!info.IsHeapObjectAlive(context)) {
      RecordUmaForNonSoftNavigationInteraction(*context.Get());
      dead_contexts.push_back(context);
    }
  }
  potential_soft_navigations_.RemoveAll(dead_contexts);
  if (potential_soft_navigations_.empty()) {
    CHECK(!active_interaction_context_);
    ResetHeuristic();
  }
}

LocalFrame* SoftNavigationHeuristics::GetLocalFrameIfNotDetached() const {
  LocalDOMWindow* window = GetSupplementable();
  return window->IsCurrentlyDisplayedInFrame() ? window->GetFrame() : nullptr;
}

SoftNavigationHeuristics::EventScope SoftNavigationHeuristics::CreateEventScope(
    EventScope::Type type,
    ScriptState* script_state) {
  if (!has_active_event_scope_) {
    // Create a new `SoftNavigationContext`, which represents a candidate soft
    // navigation interaction. This context is propagated to all descendant
    // tasks created within this or any nested `EventScope`.
    //
    // For non-"new interactions", we want to reuse the context from the initial
    // "new interaction" (i.e. keydown), but will create a new one if that has
    // been cleared, which can happen in tests.
    if (IsInteractionStart(type) || !active_interaction_context_) {
      active_interaction_context_ =
          MakeGarbageCollected<SoftNavigationContext>();
      potential_soft_navigations_.insert(active_interaction_context_.Get());
    }

    // Ensure that paints would be reset, so that paint recording would continue
    // despite the user interaction.
    ResetPaintsIfNeeded();
  }
  CHECK(active_interaction_context_.Get());

  auto* tracker = scheduler::TaskAttributionTracker::From(
      GetSupplementable()->GetIsolate());
  bool is_nested = std::exchange(has_active_event_scope_, true);
  // `tracker` will be null if TaskAttributionInfrastructureDisabledForTesting
  // is enabled.
  if (!tracker) {
    return SoftNavigationHeuristics::EventScope(this,
                                                /*observer_scope=*/std::nullopt,
                                                /*task_scope=*/std::nullopt,
                                                type, is_nested);
  }
  return SoftNavigationHeuristics::EventScope(
      this, tracker->RegisterObserver(this),
      tracker->CreateTaskScope(script_state, active_interaction_context_.Get()),
      type, is_nested);
}

std::optional<SoftNavigationHeuristics::EventScope>
SoftNavigationHeuristics::MaybeCreateEventScopeForEvent(const Event& event) {
  std::optional<EventScope::Type> type = EventScopeTypeFromEvent(event);
  if (!type) {
    return std::nullopt;
  }
  auto* script_state = ToScriptStateForMainWorld(GetSupplementable());
  if (!script_state) {
    return std::nullopt;
  }
  return CreateEventScope(*type, script_state);
}

void SoftNavigationHeuristics::OnSoftNavigationEventScopeDestroyed(
    const EventScope& event_scope) {
  // Set the start time to the end of event processing. In case of nested event
  // scopes, we want this to be the end of the nested `navigate()` event
  // handler.
  CHECK(active_interaction_context_);
  if (active_interaction_context_->UserInteractionTimestamp().is_null()) {
    active_interaction_context_->SetUserInteractionTimestamp(
        base::TimeTicks::Now());
  }

  has_active_event_scope_ = event_scope.is_nested_;
  if (has_active_event_scope_) {
    return;
  }

  EmitSoftNavigationEntryIfAllConditionsMet(active_interaction_context_.Get());
  // For keyboard events, we can't clear `active_interaction_context_` until
  // keyup because keypress and keyup need to reuse the keydown context.
  if (IsInteractionEnd(event_scope.type_)) {
    active_interaction_context_ = nullptr;
  }

  // TODO(crbug.com/1502640): We should also reset the heuristic a few seconds
  // after a click event handler is done, to reduce potential cycles.
}

// SoftNavigationHeuristics::EventScope implementation
// ///////////////////////////////////////////
SoftNavigationHeuristics::EventScope::EventScope(
    SoftNavigationHeuristics* heuristics,
    std::optional<ObserverScope> observer_scope,
    std::optional<TaskScope> task_scope,
    Type type,
    bool is_nested)
    : heuristics_(heuristics),
      observer_scope_(std::move(observer_scope)),
      task_scope_(std::move(task_scope)),
      type_(type),
      is_nested_(is_nested) {
  CHECK(heuristics_);
}

SoftNavigationHeuristics::EventScope::EventScope(EventScope&& other)
    : heuristics_(std::exchange(other.heuristics_, nullptr)),
      observer_scope_(std::move(other.observer_scope_)),
      task_scope_(std::move(other.task_scope_)),
      type_(other.type_),
      is_nested_(other.is_nested_) {}

SoftNavigationHeuristics::EventScope&
SoftNavigationHeuristics::EventScope::operator=(EventScope&& other) {
  heuristics_ = std::exchange(other.heuristics_, nullptr);
  observer_scope_ = std::move(other.observer_scope_);
  task_scope_ = std::move(other.task_scope_);
  type_ = other.type_;
  is_nested_ = other.is_nested_;
  return *this;
}

SoftNavigationHeuristics::EventScope::~EventScope() {
  if (!heuristics_) {
    return;
  }
  heuristics_->OnSoftNavigationEventScopeDestroyed(*this);
}

}  // namespace blink
