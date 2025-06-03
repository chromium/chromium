// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

#include <cstdint>
#include <iterator>
#include <utility>

#include "base/containers/adapters.h"
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
  kInsufficientPaints = 2,
  kNoDomModification = 4,

  // For now, this next value is equivalent to kNoDomModification, because we
  // cannot have paints without a dom mod.
  // However, kNoDomModification might evolve into something more "semantic",
  // such that you could have paints without a dom mod.
  kNoPaintOrDomModification = kInsufficientPaints | kNoDomModification,

  kMaxValue = kNoPaintOrDomModification,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:SoftNavigationOutcome)

void OnSoftNavigationContextWasExhausted(const SoftNavigationContext& context,
                                         uint64_t required_paint_area) {
  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("loading"),
      "SoftNavigationHeuristics::SoftNavigationContextWasExhausted", "context",
      context);

  // Don't bother to log if the URL was never set.  That means it was just a
  // normal interaction.
  if (context.Url().empty()) {
    return;
  }

  // TODO(crbug.com/351826232): Consider differentiating contexts that were
  // cleaned up before page was unloaded vs cleaned up because of page unload.

  if (context.WasEmitted()) {
    // We already report this outcome eagerly, as part of
    // `ReportSoftNavigationToMetrics`, so don't report again here.
    // However, if we ever report total painted area, or largest paint size, we
    // should do it here, lazily, in order to wait for the final sizes.
  } else if (!context.HasDomModification()) {
    base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                  SoftNavigationOutcome::kNoDomModification);
  } else if (!context.SatisfiesSoftNavPaintCriteria(required_paint_area)) {
    base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                  SoftNavigationOutcome::kInsufficientPaints);
  }
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

SoftNavigationHeuristics::SoftNavigationHeuristics(LocalDOMWindow* window)
    : window_(window),
      task_attribution_tracker_(
          scheduler::TaskAttributionTracker::From(window->GetIsolate())) {
  LocalFrame* frame = window->GetFrame();
  CHECK(frame && frame->View());
}

SoftNavigationHeuristics* SoftNavigationHeuristics::CreateIfNeeded(
    LocalDOMWindow* window) {
  CHECK(window);
  if (!base::FeatureList::IsEnabled(features::kSoftNavigationDetection)) {
    return nullptr;
  }
  if (!window->GetFrame()->IsMainFrame()) {
    return nullptr;
  }
  if (Document* document = window->document()) {
    // Don't measure soft navigations in devtools.
    if (document->Url().ProtocolIs("devtools")) {
      return nullptr;
    }
  }
  return MakeGarbageCollected<SoftNavigationHeuristics>(window);
}

void SoftNavigationHeuristics::Shutdown() {
  task_attribution_tracker_ = nullptr;

  const auto required_paint_area = CalculateRequiredPaintArea();
  for (const auto& context : potential_soft_navigations_) {
    OnSoftNavigationContextWasExhausted(*context.Get(), required_paint_area);
  }
  potential_soft_navigations_.clear();
}

void SoftNavigationHeuristics::SetIsTrackingSoftNavigationHeuristicsOnDocument(
    bool value) const {
  if (Document* document = window_->document()) {
    document->SetIsTrackingSoftNavigationHeuristics(value);
  }
}

SoftNavigationContext* SoftNavigationHeuristics::EnsureContextForCurrentWindow(
    SoftNavigationContext* context) const {
  // Even when we have a context, we need to confirm if this SNH instance
  // knows about it. If a context created in one window is scheduled in
  // another, we might have a different SNH instance. This seems to fail
  // with datetime/calendar modals, for example.
  // TODO(crbug.com/40871933): We don't care to support datetime modals, but
  // this behaviour might be similar for iframes, and might be worth
  // supporting.
  if (context && potential_soft_navigations_.Contains(context)) {
    return context;
  }
  return nullptr;
}

SoftNavigationContext*
SoftNavigationHeuristics::GetSoftNavigationContextForCurrentTask() const {
  if (potential_soft_navigations_.empty()) {
    return nullptr;
  }
  // The `task_attribution_tracker_` must exist if `potential_soft_navigations_`
  // is non-empty. `task_state` can have null `context` in tests.
  CHECK(task_attribution_tracker_);
  if (auto* task_state = task_attribution_tracker_->RunningTask()) {
    return EnsureContextForCurrentWindow(
        task_state->GetSoftNavigationContext());
  }
  return nullptr;
}

std::optional<scheduler::TaskAttributionId>
SoftNavigationHeuristics::AsyncSameDocumentNavigationStarted() {
  // `task_attribution_tracker_` will be null if
  // TaskAttributionInfrastructureDisabledForTesting is enabled.
  if (!task_attribution_tracker_) {
    return std::nullopt;
  }
  scheduler::TaskAttributionInfo* task_state =
      task_attribution_tracker_->RunningTask();
  if (!task_state) {
    return std::nullopt;
  }
  // We don't need to EnsureContextForCurrentWindow here because this function
  // is not really "part" of SNH. It's a helper for task attribution.
  SoftNavigationContext* context = task_state->GetSoftNavigationContext();
  if (!context) {
    return std::nullopt;
  }
  task_attribution_tracker_->AddSameDocumentNavigationTask(task_state);
  return task_state->Id();
}

void SoftNavigationHeuristics::SameDocumentNavigationCommitted(
    const String& url,
    SoftNavigationContext* context) {
  context = EnsureContextForCurrentWindow(context);
  if (!context) {
    TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("loading"),
                        "SoftNavigationHeuristics::"
                        "SameDocumentNavigationCommittedWithoutContext",
                        "url", url);
    base::UmaHistogramEnumeration(
        kPageLoadInternalSoftNavigationOutcome,
        SoftNavigationOutcome::kNoSoftNavContextDuringUrlChange);
    return;
  }
  context->SetUrl(url);

  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("loading"),
      "SoftNavigationHeuristics::SameDocumentNavigationCommitted", "context",
      *context);

  EmitSoftNavigationEntryIfAllConditionsMet(context);
}

bool SoftNavigationHeuristics::ModifiedDOM(Node* node) {
  // Don't bother marking dom nodes unless they are in the right frame.
  if (!GetLocalFrameIfOutermostAndNotDetached()) {
    return false;
  }
  SoftNavigationContext* context = GetSoftNavigationContextForCurrentTask();
  if (!context) {
    return false;
  }
  context->AddModifiedNode(node);

  EmitSoftNavigationEntryIfAllConditionsMet(context);
  return true;
}

bool SoftNavigationHeuristics::EmitSoftNavigationEntryIfAllConditionsMet(
    SoftNavigationContext* context) {
  // If we've already emitted this entry, we might still be tracking paints.
  // Skip the rest since we only want to emit new soft-navs.
  if (context->WasEmitted()) {
    return false;
  }

  // Are the basic criteria met (interaction, url, dom modification)?
  if (!context->SatisfiesSoftNavNonPaintCriteria()) {
    return false;
  }

  // Once we've met all criteria with a new context, we replace the active
  // soft_navigation. This helps us collect paints even after Task graph is
  // exhausted, and allows the previous context to get cleaned up.
  // TODO(crbug.com/416705860): Consider always making this the context which
  // was last to update the URL, immediately upon changing, rather than waiting
  // for other criteria (like dom modification) to be met.
  most_recent_context_to_meet_non_paint_criteria_ = context;

  // Are we done?
  uint64_t required_paint_area = CalculateRequiredPaintArea();
  if (!context->SatisfiesSoftNavPaintCriteria(required_paint_area)) {
    return false;
  }

  // We have met all criteria!  SetWasEmitted here, even though we might still
  // constrain reporting (below).  That is because we do not want to test
  // for meeting criteria ever again, once we meet it for the first time.
  context->SetWasEmitted();

  // TODO(crbug.com/40871933): We are already only marking dom nodes when we
  // have a frame, and we are already limiting paints attribution to contexts
  // that come from the same SNH/window instance.  So, this might be safe to
  // CHECK().  However, potentially it is possible to meet paint criteria, then
  // meet some other final criteria in a different frame?  Until we test that,
  // let's just guard carefully.
  LocalFrame* frame = GetLocalFrameIfOutermostAndNotDetached();
  if (!frame) {
    return false;
  }

  ++soft_navigation_count_;
  window_->GenerateNewNavigationId();
  auto* performance = DOMWindowPerformance::performance(*window_.Get());
  performance->AddSoftNavigationEntry(AtomicString(context->Url()),
                                      context->UserInteractionTimestamp());

  CommitPreviousPaintTimings(frame);
  ReportSoftNavigationToMetrics(frame, context);

  TRACE_EVENT_INSTANT("scheduler,devtools.timeline,loading",
                      "SoftNavigationHeuristics_SoftNavigationDetected",
                      "context", *context, "frame", GetFrameIdForTracing(frame),
                      "navigationId", window_->GetNavigationId());

  return true;
}

void SoftNavigationHeuristics::RecordPaint(LocalFrame* frame,
                                           const gfx::RectF& rect,
                                           Node* node) {
  // For now, we only care to map paints for the most recent context to meet
  // non-paint criteria.
  if (most_recent_context_to_meet_non_paint_criteria_ &&
      most_recent_context_to_meet_non_paint_criteria_->AddPaintedArea(
          node, rect, true)) {
    return;
  }
}

void SoftNavigationHeuristics::OnPaintFinished() {
  for (const auto& context : potential_soft_navigations_) {
    if (context->OnPaintFinished()) {
      EmitSoftNavigationEntryIfAllConditionsMet(context);
    }
  }
}

void SoftNavigationHeuristics::ReportSoftNavigationToMetrics(
    LocalFrame* frame,
    SoftNavigationContext* context) const {
  CHECK(EnsureContextForCurrentWindow(context));
  auto* loader = frame->Loader().GetDocumentLoader();

  if (!loader) {
    return;
  }

  CHECK(!context->UserInteractionTimestamp().is_null());
  auto soft_navigation_start_time =
      loader->GetTiming().MonotonicTimeToPseudoWallTime(
          context->UserInteractionTimestamp());

  blink::SoftNavigationMetrics metrics = {soft_navigation_count_,
                                          soft_navigation_start_time,
                                          window_->GetNavigationId().Utf8()};

  if (LocalFrameClient* frame_client = frame->Client()) {
    // This notifies UKM about this soft navigation.
    frame_client->DidObserveSoftNavigation(metrics);
  }

  // Count "successful soft nav" in histogram
  base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                SoftNavigationOutcome::kSoftNavigationDetected);
}

void SoftNavigationHeuristics::ResetPaintTimingsIfNeeded() {
  LocalFrame* frame = GetLocalFrameIfOutermostAndNotDetached();
  if (!frame) {
    return;
  }
  LocalFrameView* local_frame_view = frame->View();
  CHECK(local_frame_view);
  if (RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window_)) {
    if (Document* document = window_->document();
        document &&
        RuntimeEnabledFeatures::SoftNavigationHeuristicsExposeFPAndFCPEnabled(
            window_.Get())) {
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
void SoftNavigationHeuristics::CommitPreviousPaintTimings(LocalFrame* frame) {
  CHECK(frame && frame->IsOutermostMainFrame());
  LocalFrameView* local_frame_view = frame->View();

  CHECK(local_frame_view);

  local_frame_view->GetPaintTimingDetector().SoftNavigationDetected(
      window_.Get());
  if (RuntimeEnabledFeatures::SoftNavigationHeuristicsExposeFPAndFCPEnabled(
          window_.Get())) {
    PaintTiming::From(*window_->document()).SoftNavigationDetected();
  }
}

void SoftNavigationHeuristics::Trace(Visitor* visitor) const {
  visitor->Trace(active_interaction_context_);
  visitor->Trace(most_recent_context_to_meet_non_paint_criteria_);
  // Register a custom weak callback, which runs after processing weakness for
  // the container. This allows us to observe the collection becoming empty
  // without needing to observe individual element disposal.
  visitor->RegisterWeakCallbackMethod<
      SoftNavigationHeuristics,
      &SoftNavigationHeuristics::ProcessCustomWeakness>(this);
  visitor->Trace(window_);
}

// This is invoked when executing a callback with an active `EventScope`,
// which happens for click and keyboard input events, as well as
// user-initiated navigation and popstate events. Running such an event
// listener "activates" the `SoftNavigationContext` as a candidate soft
// navigation.
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
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("loading"),
                      "SoftNavigationHeuristics::OnCreateTaskScope", "context",
                      active_interaction_context_.Get(), "task_id",
                      task_state.Id().value());

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
  const auto required_paint_area = CalculateRequiredPaintArea();
  WTF::EraseIf(potential_soft_navigations_, [&](const auto& context) {
    if (!info.IsHeapObjectAlive(context)) {
      OnSoftNavigationContextWasExhausted(*context.Get(), required_paint_area);
      return true;
    }
    return false;
  });

  // This should never happen if we have a last_interaction_context_.
  // numbecrbug.com/416706750aints?  Perhaps once all dom nodes modified by the
  // context and/or number of paints?  Perhaps once all dom nodes modified by
  // the context have been painted at least once, we don't care about more data?
  // Perhaps after user scrolls/interacts after history change?
  if (potential_soft_navigations_.empty()) {
    CHECK(!active_interaction_context_, base::NotFatalUntil::M142);
    CHECK(!most_recent_context_to_meet_non_paint_criteria_,
          base::NotFatalUntil::M142);
    SetIsTrackingSoftNavigationHeuristicsOnDocument(false);
  }
}

LocalFrame* SoftNavigationHeuristics::GetLocalFrameIfOutermostAndNotDetached()
    const {
  if (!window_->IsCurrentlyDisplayedInFrame()) {
    return nullptr;
  }

  LocalFrame* frame = window_->GetFrame();
  if (!frame->IsOutermostMainFrame()) {
    return nullptr;
  }

  return frame;
}

SoftNavigationHeuristics::EventScope SoftNavigationHeuristics::CreateEventScope(
    EventScope::Type type,
    ScriptState* script_state) {
  // TODO(crbug.com/417164510): It appears that we can create many contexts for
  // a single interaction, because we can get many ::CreateEventScope (non
  // nested) even for a single interaction.
  // We might want to move the EventScope wrapper higher up in the event
  // dispatch code, so we don't re-create it so often.

  if (!has_active_event_scope_) {
    // Create a new `SoftNavigationContext`, which represents a candidate soft
    // navigation interaction. This context is propagated to all descendant
    // tasks created within this or any nested `EventScope`.
    //
    // For non-"new interactions", we want to reuse the context from the initial
    // "new interaction" (i.e. keydown), but will create a new one if that has
    // been cleared, which can happen in tests.
    if (IsInteractionStart(type) || !active_interaction_context_) {
      active_interaction_context_ = MakeGarbageCollected<SoftNavigationContext>(
          RuntimeEnabledFeatures::
              SoftNavigationDetectionAdvancedPaintAttributionEnabled(window_));
      potential_soft_navigations_.push_back(active_interaction_context_);
      TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("loading"),
                          "SoftNavigationHeuristics::CreateNewContext",
                          "context", *active_interaction_context_);
    }

    // Ensure that paints would be reset, so that paint recording would continue
    // despite the user interaction.
    ResetPaintTimingsIfNeeded();
  }
  CHECK(active_interaction_context_.Get());

  auto* tracker =
      scheduler::TaskAttributionTracker::From(window_->GetIsolate());
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
  auto* script_state = ToScriptStateForMainWorld(window_.Get());
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

uint64_t SoftNavigationHeuristics::CalculateRequiredPaintArea() const {
  static constexpr uint64_t kMinRequiredArea = 1;

  LocalFrame* frame = window_->GetFrame();
  CHECK(frame);
  LocalFrameView* local_frame_view = frame->View();
  if (!local_frame_view) {
    return kMinRequiredArea;
  }

  constexpr int kSoftNavigationPaintAreaPercentageInPoints = 1;  // 0.01%
  uint64_t viewport_area = local_frame_view->GetLayoutSize().Area64();
  uint64_t required_paint_area =
      (viewport_area * kSoftNavigationPaintAreaPercentageInPoints) / 10000;

  if (required_paint_area > kMinRequiredArea) {
    return required_paint_area;
  }
  return kMinRequiredArea;
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
