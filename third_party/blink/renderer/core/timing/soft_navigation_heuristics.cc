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
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_paint_attribution_tracker.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

namespace {

const char kPageLoadInternalSoftNavigationOutcome[] =
    "PageLoad.Internal.SoftNavigationOutcome";

const char kPageLoadInternalSoftNavigationEmittedTotalPaintArea[] =
    "PageLoad.Internal.SoftNavigation.Emitted.TotalPaintArea";
const char kPageLoadInternalSoftNavigationEmittedTotalPaintAreaPoints[] =
    "PageLoad.Internal.SoftNavigation.Emitted.TotalPaintAreaPoints";

const char kPageLoadInternalSoftNavigationNotEmittedUrlEmptyTotalPaintArea[] =
    "PageLoad.Internal.SoftNavigation.NotEmittedUrlEmpty.TotalPaintArea";
const char
    kPageLoadInternalSoftNavigationNotEmittedUrlEmptyTotalPaintAreaPoints[] =
        "PageLoad.Internal.SoftNavigation.NotEmittedUrlEmpty."
        "TotalPaintAreaPoints";
const char
    kPageLoadInternalSoftNavigationNotEmittedInsufficientPaintTotalPaintArea[] =
        "PageLoad.Internal.SoftNavigation.NotEmittedInsufficientPaint."
        "TotalPaintArea";
const char
    kPageLoadInternalSoftNavigationNotEmittedInsufficientPaintTotalPaintAreaPercentage
        [] = "PageLoad.Internal.SoftNavigation.NotEmittedInsufficientPaint."
             "TotalPaintAreaPoints";
// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "SoftNavigationOutcome" in tools/metrics/histograms/enums.xml. Note also that
// these form a bitmask; future conditions should continue this pattern.
// LINT.IfChange
enum SoftNavigationOutcome {
  kSoftNavigationDetected = 0,

  kNoSoftNavContextDuringUrlChange = 1 << 0,
  kInsufficientPaints = 1 << 1,
  kNoDomModification = 1 << 2,
  kNoSoftNavContextDuringUrlChangeButMergingIntoPreviousContext = 1 << 3,

  // For now, this next value is equivalent to kNoDomModification, because we
  // cannot have paints without a dom mod.
  // However, kNoDomModification might evolve into something more "semantic",
  // such that you could have paints without a dom mod.
  kNoPaintOrDomModification = kInsufficientPaints | kNoDomModification,

  kMaxValue = kNoSoftNavContextDuringUrlChangeButMergingIntoPreviousContext,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml:SoftNavigationOutcome)

void OnSoftNavigationContextWasExhausted(const SoftNavigationContext& context,
                                         uint64_t viewport_area,
                                         uint64_t required_paint_area) {
  TRACE_EVENT_INSTANT(
      "loading", "SoftNavigationHeuristics::SoftNavigationContextWasExhausted",
      perfetto::Track::FromPointer(&context), "context", context);

  TRACE_EVENT_END("loading", perfetto::Track::FromPointer(&context));

  // Don't bother to log if the URL was never set.  That means it was just a
  // normal interaction.
  if (!context.HasUrl()) {
    uint64_t total_paint_area = context.PaintedArea();
    base::UmaHistogramCounts1M(
        kPageLoadInternalSoftNavigationNotEmittedUrlEmptyTotalPaintArea,
        total_paint_area);

    // viewport_area is guaranteed to be >= 1.
    uint64_t points_val = (total_paint_area * 10000ULL) / viewport_area;
    base::UmaHistogramCounts100000(
        kPageLoadInternalSoftNavigationNotEmittedUrlEmptyTotalPaintAreaPoints,
        base::saturated_cast<int>(points_val));
    return;
  }

  // TODO(crbug.com/351826232): Consider differentiating contexts that were
  // cleaned up before page was unloaded vs cleaned up because of page unload.

  if (context.HasNavigationId()) {
    // We already report this outcome eagerly, as part of
    // `ReportSoftNavigationToMetrics`, so don't report again here.
    // However, we can report the final paint area metrics here.
    uint64_t total_paint_area = context.PaintedArea();
    base::UmaHistogramCounts1M(
        kPageLoadInternalSoftNavigationEmittedTotalPaintArea, total_paint_area);

    // viewport_area is guaranteed to be >= 1.
    uint64_t points_val = (total_paint_area * 10000ULL) / viewport_area;
    base::UmaHistogramCounts100000(
        kPageLoadInternalSoftNavigationEmittedTotalPaintAreaPoints,
        base::saturated_cast<int>(points_val));
  } else if (!context.HasDomModification()) {
    base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                  SoftNavigationOutcome::kNoDomModification);
  } else if (!context.SatisfiesSoftNavPaintCriteria(required_paint_area)) {
    base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                  SoftNavigationOutcome::kInsufficientPaints);
    uint64_t total_paint_area = context.PaintedArea();
    base::UmaHistogramCounts1M(
        kPageLoadInternalSoftNavigationNotEmittedInsufficientPaintTotalPaintArea,
        total_paint_area);

    // viewport_area is guaranteed to be >= 1.
    uint64_t points_val = (total_paint_area * 10000ULL) / viewport_area;
    base::UmaHistogramCounts100000(
        kPageLoadInternalSoftNavigationNotEmittedInsufficientPaintTotalPaintAreaPercentage,
        base::saturated_cast<int>(points_val));
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

features::SoftNavigationHeuristicsMode GetPaintAttributionMode(
    const FeatureContext* context) {
  // If the feature flag for SoftNavigationHeuristics is enabled, prefer the
  // feature param to determine whether to enable advanced paint attribution.
  // This allows users to select the mode via about://flags.
  if (base::FeatureList::IsEnabled(features::kSoftNavigationHeuristics)) {
    return features::kSoftNavigationHeuristicsModeParam.Get();
  }
  // Without the feature flag enabled, query the runtime enabled feature
  // directly. This allows the finch experiments to control the features; it
  // also enables the feature for tests.
  //
  // But since the paint attribution modes are mutually exclusive and have
  // different flags, we need to pick an order. Since the pre-paint-based
  // attribution mode needs to be enabled intentionally from the command line or
  // about:flags (it has no REF status), pick that first.
  if (RuntimeEnabledFeatures::
          SoftNavigationDetectionPrePaintBasedAttributionEnabled(context)) {
    return features::SoftNavigationHeuristicsMode::kPrePaintBasedAttribution;
  }
  if (RuntimeEnabledFeatures::
          SoftNavigationDetectionAdvancedPaintAttributionEnabled(context)) {
    return features::SoftNavigationHeuristicsMode::kAdvancedPaintAttribution;
  }
  return features::SoftNavigationHeuristicsMode::kBasic;
}

SoftNavigationHeuristics* GetHeuristicsForNodeIfShouldTrack(const Node& node) {
  const Document& document = node.GetDocument();
  if (!document.IsTrackingSoftNavigationHeuristics()) {
    return nullptr;
  }
  if (!node.isConnected()) {
    return nullptr;
  }
  LocalDOMWindow* window = document.domWindow();
  if (!window) {
    return nullptr;
  }
  return window->GetSoftNavigationHeuristics();
}

}  // namespace

SoftNavigationHeuristics::SoftNavigationHeuristics(LocalDOMWindow* window)
    : window_(window),
      paint_attribution_mode_(GetPaintAttributionMode(window)),
      task_attribution_tracker_(
          scheduler::TaskAttributionTracker::From(window->GetIsolate())) {
  LocalFrame* frame = window->GetFrame();
  CHECK(frame && frame->View());
  if (IsPrePaintBasedAttributionEnabled()) {
    paint_attribution_tracker_ =
        MakeGarbageCollected<SoftNavigationPaintAttributionTracker>();
  }
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

  const auto viewport_area = CalculateViewportArea();
  const auto required_paint_area = CalculateRequiredPaintArea();
  for (const auto& context : potential_soft_navigations_) {
    OnSoftNavigationContextWasExhausted(*context.Get(), viewport_area,
                                        required_paint_area);
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
  if (auto* task_state = task_attribution_tracker_->CurrentTaskState()) {
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
      task_attribution_tracker_->CurrentTaskState();
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
  if (!context && !context_for_current_url_) {
    // If we don't have a context for this task, and we haven't had a context
    // for a recent URL change, then this URL change is not a soft-navigation.
    TRACE_EVENT_INSTANT("loading",
                        "SoftNavigationHeuristics::"
                        "SameDocumentNavigationCommittedWithoutContext",
                        "url", url);
    base::UmaHistogramEnumeration(
        kPageLoadInternalSoftNavigationOutcome,
        SoftNavigationOutcome::kNoSoftNavContextDuringUrlChange);
  } else if (!context) {
    // All URL changes which follow an attributed URL change are assumed to be
    // client-side-redirects and will not disable paint attribution or change
    // the emitting of existing contexts.
    // TODO(crbug.com/353043684, crbug.com/40943017): Perhaps there should be
    // limits to how long we will keep the current context as active.
    context_for_current_url_->AddUrl(url);

    TRACE_EVENT_INSTANT("loading",
                        "SoftNavigationHeuristics::"
                        "SameDocumentNavigationCommittedWithoutContextButMerg"
                        "edIntoPreviousContext",
                        perfetto::Track::FromPointer(context), "context",
                        *context_for_current_url_, "url", url);
    base::UmaHistogramEnumeration(
        kPageLoadInternalSoftNavigationOutcome,
        SoftNavigationOutcome::
            kNoSoftNavContextDuringUrlChangeButMergingIntoPreviousContext);
  } else {
    context->AddUrl(url);
    // TODO(crbug.com/416705860): If we replace a previous context that is for a
    // previous URL change, maybe we should check if it was emitted?  If not,
    // we will no longer be attributing paints to it and so it will never meet
    // criteria again (unless it changes URL again).  We might want to clean up
    // and exhaust this context immediately.
    context_for_current_url_ = context;

    TRACE_EVENT_INSTANT(
        "loading", "SoftNavigationHeuristics::SameDocumentNavigationCommitted",
        perfetto::Track::FromPointer(context), "context", *context);

    EmitSoftNavigationEntryIfAllConditionsMet(context);
  }
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

  if (IsPrePaintBasedAttributionEnabled()) {
    CHECK(paint_attribution_tracker_);
    paint_attribution_tracker_->MarkNodeAsDirectlyModified(node, context);
  } else {
    context->AddModifiedNode(node);
  }

  EmitSoftNavigationEntryIfAllConditionsMet(context);
  return true;
}

// TODO(crbug.com/424448145): re-architect how we pick our FCP point, when we
// "slice" navigationID, and when we actually Emit soft-navigation entry.  Then,
// rename and re-organize these functions.
void SoftNavigationHeuristics::EmitSoftNavigationEntryIfAllConditionsMet(
    SoftNavigationContext* context) {
  // We don't want to Emit for any context except the current URL.
  // If we collect painted area for contexts other than this one, we still don't
  // want to reach "Emit" criteria.
  if (context != context_for_current_url_) {
    return;
  }

  // If we've already emitted this entry, we might still be tracking paints.
  // Skip the rest since we only want to emit new soft-navs.
  if (context->HasNavigationId()) {
    return;
  }

  // Are the basic criteria met (interaction, url, dom modification)?
  if (!context->SatisfiesSoftNavNonPaintCriteria()) {
    return;
  }

  // Are we done?
  uint64_t required_paint_area = CalculateRequiredPaintArea();
  if (!context->SatisfiesSoftNavPaintCriteria(required_paint_area)) {
    return;
  }

  // We have met all Soft-Nav criteria!

  // At this point, this navigation should be "committed" to the performance
  // timeline. Thus, we increment the navigation id here, in the animation frame
  // Paint where the criteria are first met. However, the navigation will not be
  // ready for reporting until it also has an FCP measurement.
  // We must *not* wait on this presentation time callback, because all other
  // new performance entries created need to use this new navigation id, in
  // order to match with the eventual soft-nav entry.
  //
  // TODO(crbug.com/424448145): Ideally, we should carefully ensure that this
  // happens exactly where we want our timeOrigin, and also ensure that all
  // performance entries are created at the time of the measurement they are
  // reporting, rather than some time later, which risks assigning the wrong
  // navigationId-- but this might be impossible.  Instead, we might need to
  // re-write history when we get a new navigationId with a timeOrigin in the
  // past.
  ++soft_navigation_count_;
  window_->GenerateNewNavigationId();

  context->SetNavigationId(window_->GetNavigationId());

  context_for_first_contentful_paint_ = context;
}

SoftNavigationContext*
SoftNavigationHeuristics::MaybeGetSoftNavigationContextForTiming(Node* node) {
  if (!context_for_current_url_ ||
      !context_for_current_url_->IsRecordingLargestContentfulPaint()) {
    return nullptr;
  }
  bool attributable = IsPrePaintBasedAttributionEnabled()
                          ? paint_attribution_tracker_->IsAttributable(
                                node, context_for_current_url_)
                          : context_for_current_url_->IsNeededForTiming(node);
  return attributable ? context_for_current_url_ : nullptr;
}

void SoftNavigationHeuristics::OnPaintFinished() {
  for (const auto& context : potential_soft_navigations_) {
    if (context->OnPaintFinished()) {
      EmitSoftNavigationEntryIfAllConditionsMet(context);
    }
  }
}

void SoftNavigationHeuristics::OnInputOrScroll() {
  for (const auto& context : potential_soft_navigations_) {
    // TODO(crbug.com/425402677): Is this is a good time to emit metrics to UKM,
    // and potentially force exhausting the context / remove it from
    // `potential_soft_navigations_`?
    context->OnInputOrScroll();
  }
}

OptionalPaintTimingCallback
SoftNavigationHeuristics::TakePaintTimingCallback() {
  if (!context_for_first_contentful_paint_) {
    return {};
  }
  // If we need paint timing, we must have a context that needs FCP.
  CHECK(!context_for_first_contentful_paint_->HasFirstContentfulPaint());

  // TODO(crbug.com/40871933): We are already only marking dom nodes when we
  // have a frame, and we are already limiting paints attribution to contexts
  // that come from the same SNH/window instance.  So, this might be safe to
  // CHECK().  However, potentially it is possible to meet paint criteria,
  // then meet some other final criteria in a different frame?  Until we test
  // that, let's just guard carefully.
  LocalFrame* frame = GetLocalFrameIfOutermostAndNotDetached();
  if (!frame) {
    return {};
  }
  String frameIdForTracing = GetFrameIdForTracing(frame);

  auto callback = WTF::BindOnce(
      [](SoftNavigationHeuristics* self, Member<SoftNavigationContext> context,
         String frameIdForTracing,
         const base::TimeTicks& presentation_timestamp,
         const DOMPaintTimingInfo& paint_timing_info) {
        if (!self) {
          return;
        }
        CHECK(context);
        context->SetFirstContentfulPaint(presentation_timestamp,
                                         paint_timing_info);

        auto* performance =
            DOMWindowPerformance::performance(*self->window_.Get());

        performance->AddSoftNavigationEntry(AtomicString(context->InitialUrl()),
                                            context->UserInteractionTimestamp(),
                                            paint_timing_info);
        self->ReportSoftNavigationToMetrics(context);

        TRACE_EVENT_INSTANT("scheduler,devtools.timeline,loading",
                            "SoftNavigationHeuristics::EmitSoftNavigationEntry",
                            perfetto::Track::FromPointer(context),
                            context->FirstContentfulPaint(), "context",
                            *context, "frame", frameIdForTracing);
      },
      WrapWeakPersistent(this),
      WrapPersistent(context_for_first_contentful_paint_.Get()),
      frameIdForTracing);

  context_for_first_contentful_paint_ = nullptr;
  return std::move(callback);
}

void SoftNavigationHeuristics::UpdateSoftLcpCandidate() {
  // This is called from PaintTimingMixin on every paint timing update, without
  // feature flag check. We shouldn't have a url context without the feature.
  if (!context_for_current_url_) {
    return;
  }
  CHECK(RuntimeEnabledFeatures::SoftNavigationDetectionEnabled(window_));

  bool has_new_lcp_candidate =
      context_for_current_url_->TryUpdateLcpCandidate();
  if (!has_new_lcp_candidate) {
    return;
  }

  // Performance timeline won't allow emitting soft-LCP entries without this
  // flag, but we can save some needless work by just not even trying to report.
  if (RuntimeEnabledFeatures::SoftNavigationHeuristicsEnabled(window_)) {
    context_for_current_url_->UpdateWebExposedLargestContentfulPaintIfNeeded();
  }

  soft_navigation_lcp_details_for_metrics_ =
      context_for_current_url_->LatestLcpDetailsForUkm();

  Document* document = window_->document();
  if (!document) {
    return;
  }
  DocumentLoader* loader = document->Loader();
  if (!loader) {
    return;
  }
  loader->DidChangePerformanceTiming();
}

void SoftNavigationHeuristics::ReportSoftNavigationToMetrics(
    SoftNavigationContext* context) const {
  LocalFrame* frame = GetLocalFrameIfOutermostAndNotDetached();
  if (!frame) {
    return;
  }
  auto* loader = frame->Loader().GetDocumentLoader();
  if (!loader) {
    return;
  }

  CHECK(EnsureContextForCurrentWindow(context));

  if (LocalFrameClient* frame_client = frame->Client()) {
    blink::SoftNavigationMetrics metrics = {
        .count = soft_navigation_count_,
        .start_time = loader->GetTiming().MonotonicTimeToPseudoWallTime(
            context->UserInteractionTimestamp()),
        .first_contentful_paint =
            loader->GetTiming().MonotonicTimeToPseudoWallTime(
                context->FirstContentfulPaint()),
        .navigation_id = context->GetNavigationId().Utf8()};
    // This notifies UKM about this soft navigation.
    frame_client->DidObserveSoftNavigation(metrics);
  }

  // Count "successful soft nav" in histogram
  base::UmaHistogramEnumeration(kPageLoadInternalSoftNavigationOutcome,
                                SoftNavigationOutcome::kSoftNavigationDetected);
}

void SoftNavigationHeuristics::Trace(Visitor* visitor) const {
  visitor->Trace(active_interaction_context_);
  visitor->Trace(context_for_current_url_);
  visitor->Trace(context_for_first_contentful_paint_);
  visitor->Trace(window_);
  visitor->Trace(paint_attribution_tracker_);
  // Register a custom weak callback, which runs after processing weakness for
  // the container. This allows us to observe the collection becoming empty
  // without needing to observe individual element disposal.
  visitor->RegisterWeakCallbackMethod<
      SoftNavigationHeuristics,
      &SoftNavigationHeuristics::ProcessCustomWeakness>(this);
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
  TRACE_EVENT_INSTANT("loading", "SoftNavigationHeuristics::OnCreateTaskScope",
                      perfetto::Track::FromPointer(active_interaction_context_),
                      "context", active_interaction_context_.Get(), "task_id",
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
  potential_soft_navigations_.erase_if([&](const auto& context) {
    if (!info.IsHeapObjectAlive(context)) {
      OnSoftNavigationContextWasExhausted(
          *context.Get(), CalculateViewportArea(), required_paint_area);
      return true;
    }
    return false;
  });

  // If we fully clear out all contexts via GC, then turn off soft-navs tracking
  // on document.  This should never happen if we have a
  // `context_for_current_url_`, which means we won't ever turn off tracking
  // once an attributable URL change is detected.
  // TODO(crbug.com/416706750, crbug.com/420402247): Consider enabling some
  // mechanism for eventually resetting things.
  if (potential_soft_navigations_.empty()) {
    CHECK(!active_interaction_context_, base::NotFatalUntil::M142);
    CHECK(!context_for_current_url_, base::NotFatalUntil::M142);
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
          *window_, paint_attribution_mode_);
      potential_soft_navigations_.insert(active_interaction_context_);
      TRACE_EVENT_BEGIN(
          "loading", "SoftNavigationHeuristics::SoftNavigation",
          perfetto::Track::FromPointer(active_interaction_context_));
      TRACE_EVENT_INSTANT(
          "loading", "SoftNavigationHeuristics::CreateNewContext",
          perfetto::Track::FromPointer(active_interaction_context_), "context",
          *active_interaction_context_);
    }
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

uint64_t SoftNavigationHeuristics::CalculateViewportArea() const {
  static constexpr uint64_t kMinViewportArea = 1;
  LocalFrame* frame = window_->GetFrame();
  CHECK(frame);
  LocalFrameView* local_frame_view = frame->View();
  if (!local_frame_view) {
    return kMinViewportArea;
  }
  uint64_t viewport_area = local_frame_view->GetLayoutSize().Area64();
  return std::max(viewport_area, kMinViewportArea);
}

uint64_t SoftNavigationHeuristics::CalculateRequiredPaintArea() const {
  static constexpr uint64_t kMinRequiredArea = 1;
  constexpr int kSoftNavigationPaintAreaPercentageInPoints = 1;  // 0.01%
  uint64_t viewport_area = CalculateViewportArea();
  uint64_t required_paint_area =
      (viewport_area * kSoftNavigationPaintAreaPercentageInPoints) / 10000;
  if (required_paint_area > kMinRequiredArea) {
    return required_paint_area;
  }
  return kMinRequiredArea;
}

// static
void SoftNavigationHeuristics::InsertedNode(Node* inserted_node,
                                            Node* container_node) {
  auto* heuristics = GetHeuristicsForNodeIfShouldTrack(*inserted_node);
  if (!heuristics) {
    return;
  }
  // When a child node, which is an HTML-element, is modified within a parent
  // (added, moved, etc), mark that child as modified by soft navigation.
  // Otherwise, if the child is not an HTML-element, mark the parent instead.
  // TODO(crbug.com/41494072): This does not filter out updates from isolated
  // worlds. Should it?
  heuristics->ModifiedDOM(inserted_node->IsHTMLElement() ? inserted_node
                                                         : container_node);
}

void SoftNavigationHeuristics::ModifiedNode(Node* node) {
  auto* heuristics = GetHeuristicsForNodeIfShouldTrack(*node);
  if (!heuristics) {
    return;
  }
  heuristics->ModifiedDOM(node);
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
