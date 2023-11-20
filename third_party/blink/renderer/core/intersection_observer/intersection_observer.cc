// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"

#include <algorithm>
#include <limits>

#include "base/numerics/clamped_math.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_delegate.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_intersection_observer_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_document_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_double_doublesequence.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/intersection_observer/element_intersection_observer_data.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_controller.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_delegate.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

namespace {

// Internal implementation of IntersectionObserverDelegate when using
// IntersectionObserver with an EventCallback.
class IntersectionObserverDelegateImpl final
    : public IntersectionObserverDelegate {

 public:
  IntersectionObserverDelegateImpl(
      ExecutionContext* context,
      IntersectionObserver::EventCallback callback,
      LocalFrameUkmAggregator::MetricId ukm_metric_id,
      IntersectionObserver::DeliveryBehavior delivery_behavior,
      bool needs_initial_observation_with_detached_target)
      : context_(context),
        callback_(std::move(callback)),
        ukm_metric_id_(ukm_metric_id),
        delivery_behavior_(delivery_behavior),
        needs_initial_observation_with_detached_target_(
            needs_initial_observation_with_detached_target) {}
  IntersectionObserverDelegateImpl(const IntersectionObserverDelegateImpl&) =
      delete;
  IntersectionObserverDelegateImpl& operator=(
      const IntersectionObserverDelegateImpl&) = delete;

  LocalFrameUkmAggregator::MetricId GetUkmMetricId() const override {
    return ukm_metric_id_;
  }

  IntersectionObserver::DeliveryBehavior GetDeliveryBehavior() const override {
    return delivery_behavior_;
  }

  bool NeedsInitialObservationWithDetachedTarget() const override {
    return needs_initial_observation_with_detached_target_;
  }

  void Deliver(const HeapVector<Member<IntersectionObserverEntry>>& entries,
               IntersectionObserver& observer) override {
    callback_.Run(entries);
  }

  ExecutionContext* GetExecutionContext() const override {
    return context_.Get();
  }

  void Trace(Visitor* visitor) const override {
    IntersectionObserverDelegate::Trace(visitor);
    visitor->Trace(context_);
  }

 private:
  WeakMember<ExecutionContext> context_;
  IntersectionObserver::EventCallback callback_;
  LocalFrameUkmAggregator::MetricId ukm_metric_id_;
  IntersectionObserver::DeliveryBehavior delivery_behavior_;
  bool needs_initial_observation_with_detached_target_;
};

void ParseMargin(const String& margin_parameter,
                 Vector<Length>& margin,
                 ExceptionState& exception_state,
                 const String& marginName) {
  // TODO(szager): Make sure this exact syntax and behavior is spec-ed
  // somewhere.

  // The root margin argument accepts syntax similar to that for CSS margin:
  //
  // "1px" = top/right/bottom/left
  // "1px 2px" = top/bottom left/right
  // "1px 2px 3px" = top left/right bottom
  // "1px 2px 3px 4px" = top left right bottom

  CSSTokenizer tokenizer(margin_parameter);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange token_range(tokens);
  token_range.ConsumeWhitespace();
  while (token_range.Peek().GetType() != kEOFToken &&
         !exception_state.HadException()) {
    if (margin.size() == 4) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "Extra text found at the end of " + marginName + "Margin.");
      break;
    }
    const CSSParserToken& token = token_range.ConsumeIncludingWhitespace();
    switch (token.GetType()) {
      case kPercentageToken:
        margin.push_back(Length::Percent(token.NumericValue()));
        break;
      case kDimensionToken:
        switch (token.GetUnitType()) {
          case CSSPrimitiveValue::UnitType::kPixels:
            margin.push_back(
                Length::Fixed(static_cast<int>(floor(token.NumericValue()))));
            break;
          case CSSPrimitiveValue::UnitType::kPercentage:
            margin.push_back(Length::Percent(token.NumericValue()));
            break;
          default:
            exception_state.ThrowDOMException(
                DOMExceptionCode::kSyntaxError,
                marginName + "Margin must be specified in pixels or percent.");
        }
        break;
      default:
        exception_state.ThrowDOMException(
            DOMExceptionCode::kSyntaxError,
            marginName + "Margin must be specified in pixels or percent.");
    }
  }
}

void ParseThresholds(
    const V8UnionDoubleOrDoubleSequence* threshold_parameter,
    Vector<float>& thresholds,
    ExceptionState& exception_state) {
  switch (threshold_parameter->GetContentType()) {
    case V8UnionDoubleOrDoubleSequence::ContentType::kDouble:
      thresholds.push_back(
          base::MakeClampedNum<float>(threshold_parameter->GetAsDouble()));
      break;
    case V8UnionDoubleOrDoubleSequence::ContentType::kDoubleSequence:
      for (auto threshold_value : threshold_parameter->GetAsDoubleSequence())
        thresholds.push_back(base::MakeClampedNum<float>(threshold_value));
      break;
  }

  if (thresholds.empty())
    thresholds.push_back(0.f);

  for (auto threshold_value : thresholds) {
    if (std::isnan(threshold_value) || threshold_value < 0.0 ||
        threshold_value > 1.0) {
      exception_state.ThrowRangeError(
          "Threshold values must be numbers between 0 and 1");
      break;
    }
  }

  std::sort(thresholds.begin(), thresholds.end());
}

// Returns a Vector of 4 margins (top, right, bottom, left) following
// https://drafts.csswg.org/css-box-4/#margin-shorthand
Vector<Length> NormalizeMargins(const Vector<Length>& margins) {
  Vector<Length> normalized_margins(4, Length::Fixed(0));

  switch (margins.size()) {
    case 0:
      break;
    case 1:
      normalized_margins[0] = normalized_margins[1] = normalized_margins[2] =
          normalized_margins[3] = margins[0];
      break;
    case 2:
      normalized_margins[0] = normalized_margins[2] = margins[0];
      normalized_margins[1] = normalized_margins[3] = margins[1];
      break;
    case 3:
      normalized_margins[0] = margins[0];
      normalized_margins[1] = normalized_margins[3] = margins[1];
      normalized_margins[2] = margins[2];
      break;
    case 4:
      normalized_margins[0] = margins[0];
      normalized_margins[1] = margins[1];
      normalized_margins[2] = margins[2];
      normalized_margins[3] = margins[3];
      break;
    default:
      NOTREACHED();
      break;
  }

  return normalized_margins;
}

String StringifyMargin(const Vector<Length>& margin) {
  StringBuilder string_builder;

  const auto append_length = [&](const Length& length) {
    string_builder.AppendNumber(length.IntValue());
    if (length.IsPercent()) {
      string_builder.Append('%');
    } else {
      string_builder.Append("px", 2);
    }
  };

  if (margin.empty()) {
    string_builder.Append("0px 0px 0px 0px");
  } else {
    DCHECK_EQ(margin.size(), 4u);
    append_length(margin[0]);
    string_builder.Append(' ');
    append_length(margin[1]);
    string_builder.Append(' ');
    append_length(margin[2]);
    string_builder.Append(' ');
    append_length(margin[3]);
  }

  return string_builder.ToString();
}

}  // anonymous namespace

static bool throttle_delay_enabled = true;

void IntersectionObserver::SetThrottleDelayEnabledForTesting(bool enabled) {
  throttle_delay_enabled = enabled;
}

IntersectionObserver* IntersectionObserver::Create(
    const IntersectionObserverInit* observer_init,
    IntersectionObserverDelegate& delegate,
    ExceptionState& exception_state) {
  Node* root = nullptr;
  if (observer_init->root()) {
    switch (observer_init->root()->GetContentType()) {
      case V8UnionDocumentOrElement::ContentType::kDocument:
        root = observer_init->root()->GetAsDocument();
        break;
      case V8UnionDocumentOrElement::ContentType::kElement:
        root = observer_init->root()->GetAsElement();
        break;
    }
  }

  DOMHighResTimeStamp delay = 0;
  bool track_visibility = false;
  delay = observer_init->delay();
  track_visibility = observer_init->trackVisibility();
  if (track_visibility && delay < 100) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "To enable the 'trackVisibility' option, you must also use a "
        "'delay' option with a value of at least 100. Visibility is more "
        "expensive to compute than the basic intersection; enabling this "
        "option may negatively affect your page's performance. Please make "
        "sure you *really* need visibility tracking before enabling the "
        "'trackVisibility' option.");
    return nullptr;
  }

  Vector<Length> margin;
  ParseMargin(observer_init->rootMargin(), margin, exception_state, "root");
  if (exception_state.HadException()) {
    return nullptr;
  }

  Vector<Length> scroll_margin;
  if (RuntimeEnabledFeatures::IntersectionObserverScrollMarginEnabled()) {
    ParseMargin(observer_init->scrollMargin(), scroll_margin, exception_state,
                "scroll");
    if (exception_state.HadException()) {
      return nullptr;
    }
  }

  Vector<float> thresholds;
  ParseThresholds(observer_init->threshold(), thresholds, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  return MakeGarbageCollected<IntersectionObserver>(
      delegate, root, margin, scroll_margin, thresholds, kFractionOfTarget,
      delay, track_visibility, false, kApplyMarginToRoot, false);
}

IntersectionObserver* IntersectionObserver::Create(
    ScriptState* script_state,
    V8IntersectionObserverCallback* callback,
    const IntersectionObserverInit* observer_init,
    ExceptionState& exception_state) {
  V8IntersectionObserverDelegate* delegate =
      MakeGarbageCollected<V8IntersectionObserverDelegate>(callback,
                                                           script_state);
  if (observer_init && observer_init->trackVisibility()) {
    UseCounter::Count(delegate->GetExecutionContext(),
                      WebFeature::kIntersectionObserverV2);
  }
  return Create(observer_init, *delegate, exception_state);
}

IntersectionObserver* IntersectionObserver::Create(
    const Vector<Length>& margin,
    const Vector<Length>& scroll_margin,
    const Vector<float>& thresholds,
    Document* document,
    EventCallback callback,
    LocalFrameUkmAggregator::MetricId ukm_metric_id,
    DeliveryBehavior behavior,
    ThresholdInterpretation semantics,
    DOMHighResTimeStamp delay,
    bool track_visibility,
    bool always_report_root_bounds,
    MarginTarget margin_target,
    bool use_overflow_clip_edge,
    bool needs_initial_observation_with_detached_target,
    ExceptionState& exception_state) {
  IntersectionObserverDelegateImpl* intersection_observer_delegate =
      MakeGarbageCollected<IntersectionObserverDelegateImpl>(
          document->GetExecutionContext(), std::move(callback), ukm_metric_id,
          behavior, needs_initial_observation_with_detached_target);
  return MakeGarbageCollected<IntersectionObserver>(
      *intersection_observer_delegate, nullptr, margin, scroll_margin,
      thresholds, semantics, delay, track_visibility, always_report_root_bounds,
      margin_target, use_overflow_clip_edge);
}

IntersectionObserver::IntersectionObserver(
    IntersectionObserverDelegate& delegate,
    Node* root,
    const Vector<Length>& margin,
    const Vector<Length>& scroll_margin,
    const Vector<float>& thresholds,
    ThresholdInterpretation semantics,
    DOMHighResTimeStamp delay,
    bool track_visibility,
    bool always_report_root_bounds,
    MarginTarget margin_target,
    bool use_overflow_clip_edge)
    : ActiveScriptWrappable<IntersectionObserver>({}),
      ExecutionContextClient(delegate.GetExecutionContext()),
      delegate_(&delegate),
      root_(root),
      thresholds_(thresholds),
      delay_(delay),
      margin_target_(margin_target),
      root_is_implicit_(root ? 0 : 1),
      track_visibility_(track_visibility),
      track_fraction_of_root_(semantics == kFractionOfRoot),
      always_report_root_bounds_(always_report_root_bounds),
      use_overflow_clip_edge_(use_overflow_clip_edge) {
  margin_ = NormalizeMargins(margin);

  scroll_margin_ = NormalizeMargins(scroll_margin);
  if (std::all_of(scroll_margin_.begin(), scroll_margin_.end(),
                  [](const auto& m) { return m.IsZero(); })) {
    scroll_margin_.clear();
  }

  if (root) {
    if (root->IsDocumentNode()) {
      To<Document>(root)
          ->EnsureDocumentExplicitRootIntersectionObserverData()
          .AddObserver(*this);
    } else {
      DCHECK(root->IsElementNode());
      To<Element>(root)->EnsureIntersectionObserverData().AddObserver(*this);
    }
  }
}

void IntersectionObserver::ProcessCustomWeakness(const LivenessBroker& info) {
  // For explicit-root observers, if the root element disappears for any reason,
  // any remaining obsevations must be dismantled.
  if (root() && !info.IsHeapObjectAlive(root()))
    root_ = nullptr;
  if (!RootIsImplicit() && !root())
    disconnect();
}

bool IntersectionObserver::RootIsValid() const {
  return RootIsImplicit() || root();
}

void IntersectionObserver::InvalidateCachedRects() {
  for (auto& observation : observations_) {
    observation->InvalidateCachedRects();
  }
}

void IntersectionObserver::observe(Element* target,
                                   ExceptionState& exception_state) {
  if (!RootIsValid() || !target)
    return;

  if (target->EnsureIntersectionObserverData().GetObservationFor(*this))
    return;

  IntersectionObservation* observation =
      MakeGarbageCollected<IntersectionObservation>(*this, *target);
  target->EnsureIntersectionObserverData().AddObservation(*observation);
  observations_.insert(observation);
  if (root() && root()->isConnected()) {
    root()
        ->GetDocument()
        .EnsureIntersectionObserverController()
        .AddTrackedObserver(*this);
  }
  if (target->isConnected()) {
    target->GetDocument()
        .EnsureIntersectionObserverController()
        .AddTrackedObservation(*observation);
    if (LocalFrameView* frame_view = target->GetDocument().View()) {
      // The IntersectionObserver spec requires that at least one observation
      // be recorded after observe() is called, even if the frame is throttled.
      frame_view->SetIntersectionObservationState(LocalFrameView::kRequired);
      frame_view->ScheduleAnimation();
    }
  } else if (delegate_->NeedsInitialObservationWithDetachedTarget()) {
    absl::optional<base::TimeTicks> monotonic_time;
    absl::optional<IntersectionGeometry::RootGeometry> root_geometry;
    observation->ComputeIntersection(
        IntersectionObservation::kImplicitRootObserversNeedUpdate |
            IntersectionObservation::kExplicitRootObserversNeedUpdate |
            IntersectionObservation::kIgnoreDelay |
            (use_overflow_clip_edge_
                 ? IntersectionObservation::kUseOverflowClipEdge
                 : 0),
        IntersectionGeometry::kInfiniteScrollDelta, monotonic_time,
        root_geometry);
  }
}

void IntersectionObserver::unobserve(Element* target,
                                     ExceptionState& exception_state) {
  if (!target || !target->IntersectionObserverData())
    return;

  IntersectionObservation* observation =
      target->IntersectionObserverData()->GetObservationFor(*this);
  if (!observation)
    return;

  observation->Disconnect();
  observations_.erase(observation);
  active_observations_.erase(observation);
  if (root() && root()->isConnected() && observations_.empty()) {
    root()
        ->GetDocument()
        .EnsureIntersectionObserverController()
        .RemoveTrackedObserver(*this);
  }
}

void IntersectionObserver::disconnect(ExceptionState& exception_state) {
  for (auto& observation : observations_)
    observation->Disconnect();
  observations_.clear();
  active_observations_.clear();
  if (root() && root()->isConnected()) {
    root()
        ->GetDocument()
        .EnsureIntersectionObserverController()
        .RemoveTrackedObserver(*this);
  }
}

HeapVector<Member<IntersectionObserverEntry>> IntersectionObserver::takeRecords(
    ExceptionState& exception_state) {
  HeapVector<Member<IntersectionObserverEntry>> entries;
  for (auto& observation : observations_)
    observation->TakeRecords(entries);
  active_observations_.clear();
  return entries;
}

String IntersectionObserver::rootMargin() const {
  return StringifyMargin(RootMargin());
}

String IntersectionObserver::scrollMargin() const {
  return StringifyMargin(ScrollMargin());
}

DOMHighResTimeStamp IntersectionObserver::GetEffectiveDelay() const {
  return throttle_delay_enabled ? delay_ : 0;
}

DOMHighResTimeStamp IntersectionObserver::GetTimeStamp(
    base::TimeTicks monotonic_time) const {
  return DOMWindowPerformance::performance(
             *To<LocalDOMWindow>(delegate_->GetExecutionContext()))
      ->MonotonicTimeToDOMHighResTimeStamp(monotonic_time);
}

int64_t IntersectionObserver::ComputeIntersections(
    unsigned flags,
    absl::optional<base::TimeTicks>& monotonic_time,
    gfx::Vector2dF accumulated_scroll_delta_since_last_update) {
  DCHECK(!RootIsImplicit());
  if (!RootIsValid() || !GetExecutionContext() || observations_.empty())
    return 0;

  if (use_overflow_clip_edge_)
    flags |= IntersectionObservation::kUseOverflowClipEdge;

  absl::optional<IntersectionGeometry::RootGeometry> root_geometry;
  int64_t result = 0;
  if (RuntimeEnabledFeatures::IntersectionOptimizationEnabled()) {
    for (auto& observation : observations_) {
      result += observation->ComputeIntersection(
          flags, accumulated_scroll_delta_since_last_update, monotonic_time,
          root_geometry);
    }
  } else {
    // If we're processing post-layout deliveries only and we're not a
    // post-layout delivery observer, then return early. Likewise, return if we
    // need to compute non-post-layout-delivery observations but the observer
    // behavior is post-layout.
    bool post_layout_delivery_only =
        flags & IntersectionObservation::kPostLayoutDeliveryOnly;
    bool is_post_layout_delivery_observer =
        GetDeliveryBehavior() ==
        IntersectionObserver::kDeliverDuringPostLayoutSteps;
    if (post_layout_delivery_only != is_post_layout_delivery_observer) {
      return 0;
    }
    // TODO(szager): Is this copy necessary?
    HeapVector<Member<IntersectionObservation>> observations_to_process(
        observations_);
    for (auto& observation : observations_to_process) {
      result += observation->ComputeIntersection(flags, gfx::Vector2dF(),
                                                 monotonic_time, root_geometry);
    }
  }
  return result;
}

gfx::Vector2dF IntersectionObserver::MinScrollDeltaToUpdate() const {
  gfx::Vector2dF result = IntersectionGeometry::kInfiniteScrollDelta;
  for (const auto& observation : observations_) {
    result.SetToMin(observation->MinScrollDeltaToUpdate());
  }
  return result;
}

bool IntersectionObserver::IsInternal() const {
  return GetUkmMetricId() !=
         LocalFrameUkmAggregator::kJavascriptIntersectionObserver;
}

LocalFrameUkmAggregator::MetricId IntersectionObserver::GetUkmMetricId() const {
  return delegate_->GetUkmMetricId();
}

void IntersectionObserver::ReportUpdates(IntersectionObservation& observation) {
  DCHECK_EQ(observation.Observer(), this);
  bool needs_scheduling = active_observations_.empty();
  active_observations_.insert(&observation);

  if (needs_scheduling) {
    To<LocalDOMWindow>(GetExecutionContext())
        ->document()
        ->EnsureIntersectionObserverController()
        .ScheduleIntersectionObserverForDelivery(*this);
  }
}

IntersectionObserver::DeliveryBehavior
IntersectionObserver::GetDeliveryBehavior() const {
  return delegate_->GetDeliveryBehavior();
}

void IntersectionObserver::Deliver() {
  if (!NeedsDelivery())
    return;
  HeapVector<Member<IntersectionObserverEntry>> entries;
  for (auto& observation : observations_)
    observation->TakeRecords(entries);
  active_observations_.clear();
  if (entries.size())
    delegate_->Deliver(entries, *this);
}

bool IntersectionObserver::HasPendingActivity() const {
  return NeedsDelivery();
}

void IntersectionObserver::Trace(Visitor* visitor) const {
  visitor->template RegisterWeakCallbackMethod<
      IntersectionObserver, &IntersectionObserver::ProcessCustomWeakness>(this);
  visitor->Trace(delegate_);
  visitor->Trace(observations_);
  visitor->Trace(active_observations_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
