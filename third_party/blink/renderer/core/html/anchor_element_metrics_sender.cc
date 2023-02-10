// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-forward.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-shared.h"

namespace blink {
namespace {
static float INTERSECTION_RATIO_THRESHOLD = 0.5f;

}  // namespace

// static
const char AnchorElementMetricsSender::kSupplementName[] =
    "DocumentAnchorElementMetricsSender";

AnchorElementMetricsSender::~AnchorElementMetricsSender() = default;

// static
AnchorElementMetricsSender* AnchorElementMetricsSender::From(
    Document& document) {
  if (!HasAnchorElementMetricsSender(document)) {
    return nullptr;
  }

  AnchorElementMetricsSender* sender =
      Supplement<Document>::From<AnchorElementMetricsSender>(document);
  if (!sender) {
    sender = MakeGarbageCollected<AnchorElementMetricsSender>(document);
    ProvideTo(document, sender);
  }
  return sender;
}

// static
bool AnchorElementMetricsSender::HasAnchorElementMetricsSender(
    Document& document) {
  bool is_feature_enabled =
      base::FeatureList::IsEnabled(features::kNavigationPredictor);
  const KURL& url = document.BaseURL();
  return is_feature_enabled && document.IsInOutermostMainFrame() &&
         url.IsValid() && url.ProtocolIs("https");
}

void AnchorElementMetricsSender::MaybeReportClickedMetricsOnClick(
    const HTMLAnchorElement& anchor_element) {
  DCHECK(base::FeatureList::IsEnabled(features::kNavigationPredictor));
  if (!anchor_element.Href().ProtocolIsInHTTPFamily() ||
      !GetRootDocument(anchor_element)->Url().ProtocolIsInHTTPFamily() ||
      !anchor_element.GetDocument().BaseURL().ProtocolIsInHTTPFamily()) {
    return;
  }
  if (!AssociateInterface()) {
    return;
  }
  auto click = mojom::blink::AnchorElementClick::New();
  click->anchor_id = AnchorElementId(anchor_element);
  click->target_url = anchor_element.Href();
  base::TimeDelta navigation_start_to_click =
      clock_->NowTicks() - NavigationStart(anchor_element);
  click->navigation_start_to_click = navigation_start_to_click;
  metrics_host_->ReportAnchorElementClick(std::move(click));
}

void AnchorElementMetricsSender::AddAnchorElement(HTMLAnchorElement& element) {
  DCHECK(base::FeatureList::IsEnabled(features::kNavigationPredictor));
  if (!AssociateInterface()) {
    return;
  }

  // Add this element to the set of elements that we will try to report after
  // the next layout.
  anchor_elements_to_report_.insert(&element);
}

void AnchorElementMetricsSender::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_elements_to_report_);
  visitor->Trace(metrics_host_);
  visitor->Trace(intersection_observer_);
  visitor->Trace(update_timer_);
  Supplement<Document>::Trace(visitor);
}

bool AnchorElementMetricsSender::AssociateInterface() {
  if (metrics_host_.is_bound())
    return true;

  Document* document = GetSupplementable();
  // Unable to associate since no frame is attached.
  if (!document->GetFrame())
    return false;

  document->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      metrics_host_.BindNewPipeAndPassReceiver(
          document->GetExecutionContext()->GetTaskRunner(
              TaskType::kInternalDefault)));
  return true;
}

AnchorElementMetricsSender::AnchorElementMetricsSender(Document& document)
    : Supplement<Document>(document),
      metrics_host_(document.GetExecutionContext()),
      update_timer_(document.GetExecutionContext()->GetTaskRunner(
                        TaskType::kInternalDefault),
                    this,
                    &AnchorElementMetricsSender::UpdateMetrics),
      clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(document.IsInOutermostMainFrame());
  DCHECK(clock_);

  document.View()->RegisterForLifecycleNotifications(this);
  intersection_observer_ = IntersectionObserver::Create(
      {}, {INTERSECTION_RATIO_THRESHOLD}, &document,
      WTF::BindRepeating(&AnchorElementMetricsSender::UpdateVisibleAnchors,
                         WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kAnchorElementMetricsIntersectionObserver,
      IntersectionObserver::kDeliverDuringPostLifecycleSteps,
      IntersectionObserver::kFractionOfTarget, 100 /* delay in ms */);
}

void AnchorElementMetricsSender::SetNowAsNavigationStartForTesting() {
  mock_navigation_start_for_testing_ = clock_->NowTicks();
}

void AnchorElementMetricsSender::SetTickClockForTesting(
    const base::TickClock* clock) {
  clock_ = clock;
}

void AnchorElementMetricsSender::FireUpdateTimerForTesting() {
  if (update_timer_.IsActive()) {
    update_timer_.Stop();
  }
  UpdateMetrics(&update_timer_);
}

void AnchorElementMetricsSender::UpdateVisibleAnchors(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(base::FeatureList::IsEnabled(features::kNavigationPredictor));
  DCHECK(!entries.empty());
  if (!AssociateInterface()) {
    return;
  }

  for (auto entry : entries) {
    Element* element = entry->target();
    const auto& anchor_element = To<HTMLAnchorElement>(*element);
    if (!entry->isIntersecting()) {
      // The anchor is leaving the viewport.
      EnqueueLeftViewport(anchor_element);
    } else {
      //  The anchor is visible.
      EnqueueEnteredViewport(anchor_element);
    }
  }
}

base::TimeTicks AnchorElementMetricsSender::NavigationStart(
    const HTMLAnchorElement& element) {
  return mock_navigation_start_for_testing_.has_value()
             ? mock_navigation_start_for_testing_.value()
             : GetRootDocument(element)
                   ->Loader()
                   ->GetTiming()
                   .NavigationStart();
}

void AnchorElementMetricsSender::MaybeReportAnchorElementPointerEvent(
    HTMLAnchorElement& element,
    const PointerEvent& pointer_event) {
  const auto anchor_id = AnchorElementId(element);
  auto it = anchor_elements_timing_stats_.find(anchor_id);
  if (it == anchor_elements_timing_stats_.end()) {
    return;
  }
  auto& element_timing = it->value;
  const AtomicString& event_type = pointer_event.type();
  if (event_type == event_type_names::kPointerover) {
    if (!element_timing->pointer_over_timer_.has_value()) {
      element_timing->pointer_over_timer_ = clock_->NowTicks();

      auto msg = mojom::blink::AnchorElementPointerOver::New();
      msg->anchor_id = anchor_id;
      base::TimeDelta navigation_start_to_pointer_over =
          clock_->NowTicks() - NavigationStart(element);
      msg->navigation_start_to_pointer_over = navigation_start_to_pointer_over;

      metrics_host_->ReportAnchorElementPointerOver(std::move(msg));
    }
  } else if (event_type == event_type_names::kPointerout) {
    if (!element_timing->pointer_over_timer_.has_value()) {
      return;
    }
    auto msg = mojom::blink::AnchorElementPointerOut::New();
    msg->anchor_id = anchor_id;
    base::TimeDelta hover_dwell_time =
        clock_->NowTicks() - element_timing->pointer_over_timer_.value();
    element_timing->pointer_over_timer_.reset();
    msg->hover_dwell_time = hover_dwell_time;
    metrics_host_->ReportAnchorElementPointerOut(std::move(msg));
  }
}

void AnchorElementMetricsSender::EnqueueLeftViewport(
    const HTMLAnchorElement& element) {
  const auto anchor_id = AnchorElementId(element);
  DCHECK(anchor_elements_timing_stats_.Contains(anchor_id));
  auto* timing_stats = anchor_elements_timing_stats_.at(anchor_id);
  timing_stats->entered_viewport_should_be_enqueued_ = true;
  absl::optional<base::TimeTicks>& entered_viewport =
      timing_stats->viewport_entry_time_;
  if (!entered_viewport.has_value()) {
    return;
  }

  auto msg = mojom::blink::AnchorElementLeftViewport::New();
  msg->anchor_id = anchor_id;
  base::TimeDelta time_in_viewport =
      clock_->NowTicks() - entered_viewport.value();
  entered_viewport.reset();
  msg->time_in_viewport = time_in_viewport;
  left_viewport_messages_.push_back(std::move(msg));
}

void AnchorElementMetricsSender::EnqueueEnteredViewport(
    const HTMLAnchorElement& element) {
  const auto anchor_id = AnchorElementId(element);
  DCHECK(anchor_elements_timing_stats_.Contains(anchor_id));
  auto* timing_stats = anchor_elements_timing_stats_.at(anchor_id);
  timing_stats->viewport_entry_time_ = clock_->NowTicks();
  if (!timing_stats->entered_viewport_should_be_enqueued_) {
    return;
  }
  timing_stats->entered_viewport_should_be_enqueued_ = false;

  auto msg = mojom::blink::AnchorElementEnteredViewport::New();
  msg->anchor_id = anchor_id;
  base::TimeDelta time_entered_viewport =
      clock_->NowTicks() - NavigationStart(element);
  msg->navigation_start_to_entered_viewport = time_entered_viewport;
  entered_viewport_messages_.push_back(std::move(msg));
}

void AnchorElementMetricsSender::DidFinishLifecycleUpdate(
    const LocalFrameView& local_frame_view) {
  // Check that layout is stable. If it is, we can report pending
  // AnchorElements.
  Document* document = local_frame_view.GetFrame().GetDocument();
  if (document->Lifecycle().GetState() <
      DocumentLifecycle::kAfterPerformLayout) {
    return;
  }
  if (!AssociateInterface()) {
    return;
  }

  for (const auto& member_element : anchor_elements_to_report_) {
    HTMLAnchorElement& anchor_element = *member_element;
    if (!anchor_element.Href().ProtocolIsInHTTPFamily()) {
      continue;
    }

    // If the anchor doesn't have a valid frame/root document, skip it.
    if (!anchor_element.GetDocument().GetFrame() ||
        !GetRootDocument(anchor_element)) {
      continue;
    }

    // Only anchors with width/height should be evaluated.
    if (!anchor_element.GetLayoutObject() ||
        anchor_element.GetLayoutObject()->AbsoluteBoundingBoxRect().IsEmpty()) {
      continue;
    }
    mojom::blink::AnchorElementMetricsPtr anchor_element_metrics =
        CreateAnchorElementMetrics(anchor_element);

    int sampling_period = base::GetFieldTrialParamByFeatureAsInt(
        blink::features::kNavigationPredictor, "random_anchor_sampling_period",
        100);
    int random = base::RandInt(1, sampling_period);
    if (random == 1) {
      // This anchor element is sampled in.
      const auto anchor_id = AnchorElementId(anchor_element);
      if (!anchor_elements_timing_stats_.Contains(anchor_id)) {
        anchor_elements_timing_stats_.insert(
            anchor_id, std::make_unique<AnchorElementTimingStats>());
      }
      if (anchor_element_metrics->ratio_visible_area >=
          INTERSECTION_RATIO_THRESHOLD) {
        // The element is already visible.
        EnqueueEnteredViewport(anchor_element);
      }
      // Observe the element to collect time_in_viewport stats.
      intersection_observer_->observe(&anchor_element);
    }

    metrics_.push_back(std::move(anchor_element_metrics));
  }
  // Remove all anchors, including the ones that did not qualify. This means
  // that elements that are inserted in the DOM but have an empty bounding box
  // (e.g. because they're detached from the DOM, or not currently visible)
  // during the next layout will never be reported, unless they are re-inserted
  // into the DOM later or if they enter the viewport.
  anchor_elements_to_report_.clear();
  MaybeUpdateMetrics();
}

void AnchorElementMetricsSender::MaybeUpdateMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!update_timer_.IsActive()) {
    update_timer_.StartOneShot(kUpdateMetricsTimeGap, FROM_HERE);
  }
}

void AnchorElementMetricsSender::UpdateMetrics(TimerBase* /*timer*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* execution_context = GetSupplementable()->GetExecutionContext();
  if (!execution_context || !metrics_host_.is_bound()) {
    return;
  }

  if (!metrics_.empty()) {
    metrics_host_->ReportNewAnchorElements(std::move(metrics_));
    metrics_.clear();
  }
  if (!entered_viewport_messages_.empty()) {
    metrics_host_->ReportAnchorElementsEnteredViewport(
        std::move(entered_viewport_messages_));
    entered_viewport_messages_.clear();
  }
  if (!left_viewport_messages_.empty()) {
    metrics_host_->ReportAnchorElementsLeftViewport(
        std::move(left_viewport_messages_));
    left_viewport_messages_.clear();
  }
}

}  // namespace blink
