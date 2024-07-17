// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/not_fatal_until.h"
#include "base/rand_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-forward.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/pointer_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/browser_controls.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/map_coordinates_flags.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/pointer_type_names.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-shared.h"

namespace blink {
namespace {
constexpr float kIntersectionRatioThreshold = 0.5f;

// Returns true if `document` should have an associated
// AnchorElementMetricsSender.
bool ShouldHaveAnchorElementMetricsSender(Document& document) {
  bool is_feature_enabled =
      base::FeatureList::IsEnabled(features::kNavigationPredictor);
  const KURL& url = document.Url();
  return is_feature_enabled && document.IsInOutermostMainFrame() &&
         url.IsValid() && url.ProtocolIsInHTTPFamily() &&
         document.GetExecutionContext() &&
         document.GetExecutionContext()->IsSecureContext();
}

wtf_size_t GetMaxNumberOfObservations() {
  const base::FeatureParam<int> max_number_of_observations{
      &features::kNavigationPredictor, "max_intersection_observations", -1};
  int value = max_number_of_observations.Get();
  return value >= 0 ? value : std::numeric_limits<wtf_size_t>::max();
}

base::TimeDelta GetIntersectionObserverDelay() {
  const base::FeatureParam<base::TimeDelta> param{
      &features::kNavigationPredictor, "intersection_observer_delay",
      base::Milliseconds(100)};
  return param.Get();
}

bool ShouldReportViewportPositions() {
  return base::FeatureList::IsEnabled(
      features::kNavigationPredictorNewViewportFeatures);
}

float GetBrowserControlsHeight(Document& document) {
  BrowserControls& controls = document.GetPage()->GetBrowserControls();
  if (controls.ShrinkViewport()) {
    return controls.ContentOffset();
  }
  return 0.f;
}

}  // namespace

// static
const char AnchorElementMetricsSender::kSupplementName[] =
    "DocumentAnchorElementMetricsSender";

AnchorElementMetricsSender::~AnchorElementMetricsSender() = default;

// static
AnchorElementMetricsSender* AnchorElementMetricsSender::From(
    Document& document) {
  // Note that this method is on a hot path. If `sender` already exists, we
  // avoid a call to `ShouldHaveAnchorElementMetricsSender`. If we instead had
  // `ShouldHaveAnchorElementMetricsSender` as a guard clause here, that would
  // cause a measurable performance regression.

  AnchorElementMetricsSender* sender =
      Supplement<Document>::From<AnchorElementMetricsSender>(document);
  if (!sender && ShouldHaveAnchorElementMetricsSender(document)) {
    sender = MakeGarbageCollected<AnchorElementMetricsSender>(document);
    ProvideTo(document, sender);
  }
  return sender;
}

// static
AnchorElementMetricsSender* AnchorElementMetricsSender::GetForFrame(
    LocalFrame* frame) {
  if (!frame) {
    return nullptr;
  }

  if (frame->IsCrossOriginToOutermostMainFrame()) {
    return nullptr;
  }

  LocalFrame* local_main_frame = DynamicTo<LocalFrame>(frame->Tree().Top());
  if (!local_main_frame) {
    return nullptr;
  }

  Document* main_document = local_main_frame->GetDocument();
  if (!main_document) {
    return nullptr;
  }

  return From(*main_document);
}

void AnchorElementMetricsSender::
    MaybeReportAnchorElementPointerDataOnHoverTimerFired(
        AnchorId anchor_id,
        mojom::blink::AnchorElementPointerDataPtr pointer_data) {
  DCHECK(base::FeatureList::IsEnabled(features::kNavigationPredictor));
  if (!AssociateInterface()) {
    return;
  }
  auto msg = mojom::blink::AnchorElementPointerDataOnHoverTimerFired::New(
      anchor_id, std::move(pointer_data));
  metrics_host_->ReportAnchorElementPointerDataOnHoverTimerFired(
      std::move(msg));
}

void AnchorElementMetricsSender::MaybeReportClickedMetricsOnClick(
    const HTMLAnchorElement& anchor_element) {
  DCHECK(base::FeatureList::IsEnabled(features::kNavigationPredictor));
  Document* top_document = GetSupplementable();
  CHECK(top_document);
  if (!anchor_element.Href().ProtocolIsInHTTPFamily() ||
      !top_document->Url().ProtocolIsInHTTPFamily() ||
      !anchor_element.GetDocument().Url().ProtocolIsInHTTPFamily()) {
    return;
  }
  if (!AssociateInterface()) {
    return;
  }
  base::TimeDelta navigation_start_to_click =
      clock_->NowTicks() - NavigationStart();
  auto click = mojom::blink::AnchorElementClick::New(
      AnchorElementId(anchor_element), anchor_element.Href(),
      navigation_start_to_click);
  metrics_host_->ReportAnchorElementClick(std::move(click));
}

void AnchorElementMetricsSender::AddAnchorElement(HTMLAnchorElement& element) {
  DCHECK(base::FeatureList::IsEnabled(features::kNavigationPredictor));
  if (!GetSupplementable()->GetFrame()) {
    return;
  }

  // Add this element to the set of elements that we will try to report after
  // the next layout.
  // The anchor may already be in `removed_anchors_to_report_`. We don't remove
  // it from there because it may be reinserted and then removed again. We need
  // to be able to tell the difference from an anchor that was removed before
  // being reported.
  anchor_elements_to_report_.insert(&element);
  RegisterForLifecycleNotifications();
}

void AnchorElementMetricsSender::RemoveAnchorElement(
    HTMLAnchorElement& element) {
  DCHECK(base::FeatureList::IsEnabled(features::kNavigationPredictor));

  auto it = anchor_elements_to_report_.find(&element);
  if (it != anchor_elements_to_report_.end()) {
    // The element was going to be reported, but was removed from the document
    // before the next layout. We'll treat it as if it were never inserted. We
    // don't include it in `removed_anchors_to_report_` because the element
    // might get reinserted. We don't want to exclude from consideration
    // elements that are moved around before layout.
    anchor_elements_to_report_.erase(it);
  } else {
    // The element wasn't recently added, so we may have already informed the
    // browser about it. So we'll inform the browser of its removal with the
    // next batch of new elements, so it can prune its memory usage for old
    // elements.
    removed_anchors_to_report_.push_back(AnchorElementId(element));
  }
}

void AnchorElementMetricsSender::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_elements_to_report_);
  visitor->Trace(metrics_host_);
  visitor->Trace(intersection_observer_);
  visitor->Trace(anchors_in_viewport_);
  visitor->Trace(update_timer_);
  visitor->Trace(position_update_timer_);
  Supplement<Document>::Trace(visitor);
}

bool AnchorElementMetricsSender::AssociateInterface() {
  if (metrics_host_.is_bound()) {
    return true;
  }

  Document* document = GetSupplementable();
  // Unable to associate since no frame is attached.
  if (!document->GetFrame()) {
    return false;
  }

  document->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      metrics_host_.BindNewPipeAndPassReceiver(
          document->GetExecutionContext()->GetTaskRunner(
              TaskType::kInternalDefault)));

  metrics_host_->ShouldSkipUpdateDelays(
      WTF::BindOnce(&AnchorElementMetricsSender::SetShouldSkipUpdateDelays,
                    WrapWeakPersistent(this)));

  return true;
}

AnchorElementMetricsSender::AnchorElementMetricsSender(Document& document)
    : Supplement<Document>(document),
      metrics_host_(document.GetExecutionContext()),
      update_timer_(document.GetExecutionContext()->GetTaskRunner(
                        TaskType::kInternalDefault),
                    this,
                    &AnchorElementMetricsSender::UpdateMetrics),
      random_anchor_sampling_period_(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kNavigationPredictor,
          "random_anchor_sampling_period",
          100)),
      max_number_of_observations_(GetMaxNumberOfObservations()),
      intersection_observer_delay_(GetIntersectionObserverDelay()),
      clock_(base::DefaultTickClock::GetInstance()),
      position_update_timer_(
          document.GetExecutionContext()->GetTaskRunner(
              TaskType::kInternalDefault),
          this,
          &AnchorElementMetricsSender::PositionUpdateTimerFired) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(document.IsInOutermostMainFrame());
  DCHECK(clock_);

  intersection_observer_ = IntersectionObserver::Create(
      document,
      WTF::BindRepeating(&AnchorElementMetricsSender::UpdateVisibleAnchors,
                         WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kAnchorElementMetricsIntersectionObserver,
      {.thresholds = {kIntersectionRatioThreshold},
       .delay = intersection_observer_delay_});
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

IntersectionObserver*
AnchorElementMetricsSender::GetIntersectionObserverForTesting() {
  return intersection_observer_;
}

void AnchorElementMetricsSender::SetShouldSkipUpdateDelays(
    bool should_skip_for_testing) {
  if (!should_skip_for_testing) {
    return;
  }

  should_skip_update_delays_for_testing_ = true;

  if (update_timer_.IsActive()) {
    update_timer_.Stop();
  }
  UpdateMetrics(&update_timer_);
}

void AnchorElementMetricsSender::UpdateVisibleAnchors(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  DCHECK(base::FeatureList::IsEnabled(features::kNavigationPredictor));
  DCHECK(!entries.empty());
  if (!GetSupplementable()->GetFrame()) {
    return;
  }

  for (const auto& entry : entries) {
    const Element* element = entry->target();
    const HTMLAnchorElement& anchor_element =
        IsA<HTMLAreaElement>(*element) ? To<HTMLAreaElement>(*element)
                                       : To<HTMLAnchorElement>(*element);
    if (!entry->isIntersecting()) {
      // The anchor is leaving the viewport.
      anchors_in_viewport_.erase(&anchor_element);
      EnqueueLeftViewport(anchor_element);
    } else {
      // The anchor is visible.
      anchors_in_viewport_.insert(&anchor_element);
      EnqueueEnteredViewport(anchor_element);
    }
  }

  if (position_update_timer_.IsActive()) {
    CHECK(ShouldReportViewportPositions());
    position_update_timer_.Stop();
    should_compute_positions_after_next_lifecycle_update_ = true;
  }

  RegisterForLifecycleNotifications();
}

base::TimeTicks AnchorElementMetricsSender::NavigationStart() const {
  if (mock_navigation_start_for_testing_.has_value()) {
    return mock_navigation_start_for_testing_.value();
  }

  const Document* top_document = GetSupplementable();
  CHECK(top_document);

  return top_document->Loader()->GetTiming().NavigationStart();
}

void AnchorElementMetricsSender::MaybeReportAnchorElementPointerEvent(
    HTMLAnchorElement& element,
    const PointerEvent& pointer_event) {
  if (!AssociateInterface()) {
    return;
  }

  const auto anchor_id = AnchorElementId(element);
  const AtomicString& event_type = pointer_event.type();

  auto pointer_event_for_ml_model =
      mojom::blink::AnchorElementPointerEventForMLModel::New();
  pointer_event_for_ml_model->anchor_id = anchor_id;
  pointer_event_for_ml_model->is_mouse =
      pointer_event.pointerType() == pointer_type_names::kMouse;
  if (event_type == event_type_names::kPointerover) {
    pointer_event_for_ml_model->user_interaction_event_type = mojom::blink::
        AnchorElementUserInteractionEventForMLModelType::kPointerOver;
  } else if (event_type == event_type_names::kPointerout) {
    pointer_event_for_ml_model->user_interaction_event_type = mojom::blink::
        AnchorElementUserInteractionEventForMLModelType::kPointerOut;
  } else {
    pointer_event_for_ml_model->user_interaction_event_type =
        mojom::blink::AnchorElementUserInteractionEventForMLModelType::kUnknown;
  }
  metrics_host_->ProcessPointerEventUsingMLModel(
      std::move(pointer_event_for_ml_model));

  auto it = anchor_elements_timing_stats_.find(anchor_id);
  if (it == anchor_elements_timing_stats_.end()) {
    return;
  }
  AnchorElementTimingStats& element_timing = it->value;
  if (event_type == event_type_names::kPointerover) {
    if (!element_timing.pointer_over_timer_.has_value()) {
      element_timing.pointer_over_timer_ = clock_->NowTicks();

      base::TimeDelta navigation_start_to_pointer_over =
          clock_->NowTicks() - NavigationStart();
      auto msg = mojom::blink::AnchorElementPointerOver::New(
          anchor_id, navigation_start_to_pointer_over);

      metrics_host_->ReportAnchorElementPointerOver(std::move(msg));
    }
  } else if (event_type == event_type_names::kPointerout) {
    if (!element_timing.pointer_over_timer_.has_value()) {
      return;
    }

    base::TimeDelta hover_dwell_time =
        clock_->NowTicks() - element_timing.pointer_over_timer_.value();
    element_timing.pointer_over_timer_.reset();
    auto msg =
        mojom::blink::AnchorElementPointerOut::New(anchor_id, hover_dwell_time);
    metrics_host_->ReportAnchorElementPointerOut(std::move(msg));
  } else if (event_type == event_type_names::kPointerdown) {
    // TODO(crbug.com/1297312): Check if user changed the default mouse
    // settings
    if (pointer_event.button() !=
            static_cast<int>(WebPointerProperties::Button::kLeft) &&
        pointer_event.button() !=
            static_cast<int>(WebPointerProperties::Button::kMiddle)) {
      return;
    }

    base::TimeDelta navigation_start_to_pointer_down =
        clock_->NowTicks() - NavigationStart();
    auto msg = mojom::blink::AnchorElementPointerDown::New(
        anchor_id, navigation_start_to_pointer_down);
    metrics_host_->ReportAnchorElementPointerDown(std::move(msg));
  }
}

void AnchorElementMetricsSender::
    MaybeReportAnchorElementsPositionOnScrollEnd() {
  if (!ShouldReportViewportPositions()) {
    return;
  }

  // At this point, we're unsure of whether we have the latest
  // IntersectionObserver data or not (|intersection_observer_| is configured
  // with a delay), and the post-scroll intersection computations may or may not
  // have happened yet. We set a timer for |intersection_observer_delay_| and
  // wait for either:
  // 1) UpdateVisibleAnchors to be called before the timer (we stop the timer)
  // 2) The timer finishes (no intersection changes and UpdateVisibleAnchors
  //    wasn't called)
  // After either of the two conditions are met, we wait for a lifecycle update
  // before computing anchor element position metrics.

  // |position_update_timer_| might already be active in a scenario where a
  // second scroll completes before the timer finishes.
  if (!position_update_timer_.IsActive()) {
    position_update_timer_.StartOneShot(intersection_observer_delay_,
                                        FROM_HERE);
  }
}

void AnchorElementMetricsSender::RecordPointerDown(
    const PointerEvent& pointer_event) {
  CHECK_EQ(pointer_event.type(), event_type_names::kPointerdown);
  Document* document = pointer_event.GetDocument();
  // TODO(crbug.com/347719430): LocalFrameView::FrameToViewport called below
  // doesn't work for subframes whose local root is not the main frame.
  if (!document || !document->GetFrame()->LocalFrameRoot().IsMainFrame()) {
    return;
  }

  gfx::PointF pointer_down_location = pointer_event.AbsoluteLocation();
  pointer_down_location =
      document->GetFrame()->View()->FrameToViewport(pointer_down_location);
  pointer_down_location.Offset(0,
                               GetBrowserControlsHeight(*GetSupplementable()));
  last_pointer_down_ = pointer_down_location.y();
}

void AnchorElementMetricsSender::EnqueueLeftViewport(
    const HTMLAnchorElement& element) {
  const auto anchor_id = AnchorElementId(element);
  auto it = anchor_elements_timing_stats_.find(anchor_id);
  CHECK(it != anchor_elements_timing_stats_.end(), base::NotFatalUntil::M130);
  AnchorElementTimingStats& timing_stats = it->value;
  timing_stats.entered_viewport_should_be_enqueued_ = true;
  std::optional<base::TimeTicks>& entered_viewport =
      timing_stats.viewport_entry_time_;
  if (!entered_viewport.has_value()) {
    return;
  }

  base::TimeDelta time_in_viewport =
      clock_->NowTicks() - entered_viewport.value();
  entered_viewport.reset();
  auto msg =
      mojom::blink::AnchorElementLeftViewport::New(anchor_id, time_in_viewport);
  left_viewport_messages_.push_back(std::move(msg));
}

void AnchorElementMetricsSender::EnqueueEnteredViewport(
    const HTMLAnchorElement& element) {
  const auto anchor_id = AnchorElementId(element);
  auto it = anchor_elements_timing_stats_.find(anchor_id);
  CHECK(it != anchor_elements_timing_stats_.end(), base::NotFatalUntil::M130);
  AnchorElementTimingStats& timing_stats = it->value;
  timing_stats.viewport_entry_time_ = clock_->NowTicks();
  if (!timing_stats.entered_viewport_should_be_enqueued_) {
    return;
  }
  timing_stats.entered_viewport_should_be_enqueued_ = false;

  base::TimeDelta time_entered_viewport =
      clock_->NowTicks() - NavigationStart();
  auto msg = mojom::blink::AnchorElementEnteredViewport::New(
      anchor_id, time_entered_viewport);
  entered_viewport_messages_.push_back(std::move(msg));
}

void AnchorElementMetricsSender::RegisterForLifecycleNotifications() {
  if (is_registered_for_lifecycle_notifications_) {
    return;
  }

  if (LocalFrameView* view = GetSupplementable()->View()) {
    view->RegisterForLifecycleNotifications(this);
    is_registered_for_lifecycle_notifications_ = true;
  }
}

void AnchorElementMetricsSender::PositionUpdateTimerFired(TimerBase*) {
  CHECK(ShouldReportViewportPositions());
  should_compute_positions_after_next_lifecycle_update_ = true;
  if (LocalFrameView* view = GetSupplementable()->View()) {
    view->ScheduleAnimation();
    RegisterForLifecycleNotifications();
  }
}

void AnchorElementMetricsSender::ComputeAnchorElementsPositionUpdates() {
  CHECK(ShouldReportViewportPositions());

  Screen* screen = GetSupplementable()->domWindow()->screen();
  FrameWidget* widget =
      GetSupplementable()->GetFrame()->GetWidgetForLocalRoot();
  Page* page = GetSupplementable()->GetPage();
  if (!screen || !widget || !page) {
    return;
  }

  const int screen_height_dips = screen->height();
  const int viewport_height = page->GetVisualViewport().Size().height();
  if (!screen_height_dips || !viewport_height) {
    return;
  }

  const float screen_height = widget->DIPsToBlinkSpace(screen_height_dips);
  const float browser_controls_height =
      GetBrowserControlsHeight(*GetSupplementable());

  for (const HTMLAnchorElement* anchor : anchors_in_viewport_) {
    LocalFrame* frame = anchor->GetDocument().GetFrame();
    if (!frame) {
      continue;
    }
    const LocalFrame& local_root = frame->LocalFrameRoot();
    // TODO(crbug.com/347719430): LocalFrameView::FrameToViewport called below
    // doesn't work for subframes whose local root is not the main frame.
    if (!local_root.IsMainFrame()) {
      continue;
    }

    gfx::Rect rect = anchor->VisibleBoundsInLocalRoot();
    if (rect.IsEmpty()) {
      continue;
    }
    rect = local_root.View()->FrameToViewport(rect);
    rect.Offset(0, browser_controls_height);
    float center_point_y = gfx::RectF(rect).CenterPoint().y();

    // TODO(crbug.com/347638530): Ideally we would do this entire calculation
    // in screen coordinates and use screen_height (that would be a more useful
    // metric for us), but we don't have an accurate way to do so right now.
    float vertical_position =
        center_point_y / (viewport_height + browser_controls_height);

    std::optional<float> distance_from_pointer_down_ratio;
    if (last_pointer_down_.has_value()) {
      // Note: Distances in viewport space should be the same as distances in
      // screen space, so dividing by |screen_height| instead of viewport height
      // is fine (and likely a more useful metric).
      float distance_from_pointer_down =
          center_point_y - last_pointer_down_.value();
      distance_from_pointer_down_ratio =
          distance_from_pointer_down / screen_height;
    }

    auto position_update = mojom::blink::AnchorElementPositionUpdate::New(
        AnchorElementId(*anchor), vertical_position,
        distance_from_pointer_down_ratio);
    position_update_messages_.push_back(std::move(position_update));
  }
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
  if (!GetSupplementable()->GetFrame()) {
    return;
  }

  for (const auto& member_element : anchor_elements_to_report_) {
    HTMLAnchorElement& anchor_element = *member_element;

    mojom::blink::AnchorElementMetricsPtr anchor_element_metrics =
        CreateAnchorElementMetrics(anchor_element);
    if (!anchor_element_metrics) {
      continue;
    }

    if (!intersection_observer_limit_exceeded_) {
      int random = base::RandInt(1, random_anchor_sampling_period_);
      if (random == 1) {
        // This anchor element is sampled in.
        const auto anchor_id = AnchorElementId(anchor_element);
        anchor_elements_timing_stats_.insert(anchor_id,
                                             AnchorElementTimingStats{});
        // Observe the element to collect time_in_viewport stats.
        intersection_observer_->observe(&anchor_element);
        // If we've exceeded the limit of anchors observed by the intersection
        // observer, disconnect the observer (stop observing all anchors).
        // We disconnect instead of keeping previous observations alive as a
        // viewport based heuristic is unlikely to be useful in pages with
        // a large number of anchors (too many false positives, or no
        // predictions made at all), and we might be better off saving CPU time
        // by avoiding intersection computations altogether in such pages. This
        // could be revisited in the future.
        if (intersection_observer_->Observations().size() >
            max_number_of_observations_) {
          intersection_observer_limit_exceeded_ = true;
          intersection_observer_->disconnect();
        }
      }
    }

    metrics_.push_back(std::move(anchor_element_metrics));
  }
  // Remove all anchors, including the ones that did not qualify. This means
  // that elements that are inserted in the DOM but have an empty bounding box
  // (e.g. because they're detached from the DOM, or not currently visible)
  // during the next layout will never be reported, unless they are re-inserted
  // into the DOM later or if they enter the viewport.
  anchor_elements_to_report_.clear();

  metrics_removed_anchors_.AppendVector(removed_anchors_to_report_);
  removed_anchors_to_report_.clear();

  if (!metrics_.empty() || !metrics_removed_anchors_.empty()) {
    // Note that if an element removal happens between the population of
    // `metrics_` and sending the update to the browser, we may have a scenario
    // where an update would report the same element as being added and removed.
    // We record information to disambiguate when flushing the metrics.
    std::pair<wtf_size_t, wtf_size_t> metrics_partition =
        std::make_pair(metrics_.size(), metrics_removed_anchors_.size());
    if (metrics_partitions_.empty() ||
        metrics_partitions_.back() != metrics_partition) {
      metrics_partitions_.push_back(metrics_partition);
    }
  }

  if (should_compute_positions_after_next_lifecycle_update_) {
    ComputeAnchorElementsPositionUpdates();
    should_compute_positions_after_next_lifecycle_update_ = false;
  }
  MaybeUpdateMetrics();

  DCHECK_EQ(&local_frame_view, GetSupplementable()->View());
  DCHECK(is_registered_for_lifecycle_notifications_);
  GetSupplementable()->View()->UnregisterFromLifecycleNotifications(this);
  is_registered_for_lifecycle_notifications_ = false;
}

void AnchorElementMetricsSender::MaybeUpdateMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (should_skip_update_delays_for_testing_) {
    DCHECK(!update_timer_.IsActive());
    UpdateMetrics(&update_timer_);
  } else if (!update_timer_.IsActive()) {
    update_timer_.StartOneShot(kUpdateMetricsTimeGap, FROM_HERE);
  }
}

void AnchorElementMetricsSender::UpdateMetrics(TimerBase* /*timer*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (metrics_.empty() && metrics_removed_anchors_.empty() &&
      entered_viewport_messages_.empty() && left_viewport_messages_.empty() &&
      position_update_messages_.empty()) {
    return;
  }

  if (!AssociateInterface()) {
    return;
  }

  if (!metrics_.empty() || !metrics_removed_anchors_.empty()) {
    CHECK(!metrics_partitions_.empty());
    CHECK(metrics_partitions_.back() ==
          std::make_pair(metrics_.size(), metrics_removed_anchors_.size()));

    // Multiple lifecycle updates, during which we buffer metrics updates, may
    // have happened before we send the buffered metrics updates here. Between
    // lifecycle updates, the anchors whose metrics are buffered may have
    // changed, so we now remove any stale updates which no longer accurately
    // represent the state of the page on the most recent lifecycle update. The
    // metrics from a more recent lifecycle update reflect the current state.
    // Within the changes of a single lifecycle update, if the same anchor is
    // both removed and added then it must have been removed first. So to
    // reconstruct the correct state, we do a pass over the buffered updates
    // where we process the removals of the first lifecycle update, then the
    // additions of the first lifecycle update, then the removals of the second
    // lifecycle update, then the additions of the second lifecycle update, and
    // so on.
    WTF::HashMap<AnchorId, bool> present;
    WTF::HashMap<AnchorId, bool> newly_removed;
    wtf_size_t insert_idx = 0;
    wtf_size_t remove_idx = 0;
    for (const auto& [insert_end, remove_end] : metrics_partitions_) {
      // For each partition, removals are processed before insertions.
      const auto removals = base::make_span(metrics_removed_anchors_)
                                .subspan(remove_idx, (remove_end - remove_idx));
      for (AnchorId removed_id : removals) {
        auto result = present.Set(removed_id, false);
        newly_removed.insert(removed_id, result.is_new_entry);
      }
      const auto insertions = base::make_span(metrics_).subspan(
          insert_idx, (insert_end - insert_idx));
      for (const auto& insertion : insertions) {
        present.Set(insertion->anchor_id, true);
      }
      insert_idx = insert_end;
      remove_idx = remove_end;
    }
    WTF::EraseIf(
        metrics_,
        [&present](const mojom::blink::AnchorElementMetricsPtr& metric) {
          return !present.at(metric->anchor_id);
        });
    WTF::EraseIf(metrics_removed_anchors_,
                 [&present, &newly_removed](AnchorId id) {
                   return !newly_removed.at(id) || present.at(id);
                 });

    metrics_host_->ReportNewAnchorElements(std::move(metrics_),
                                           std::move(metrics_removed_anchors_));
    metrics_.clear();
    metrics_removed_anchors_.clear();
    metrics_partitions_.clear();
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
  if (!position_update_messages_.empty()) {
    CHECK(ShouldReportViewportPositions());
    metrics_host_->ReportAnchorElementsPositionUpdate(
        std::move(position_update_messages_));
    position_update_messages_.clear();
  }
}

}  // namespace blink
