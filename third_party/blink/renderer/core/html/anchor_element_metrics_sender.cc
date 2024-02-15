// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/anchor_element_metrics_sender.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
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
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/pointer_type_names.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
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
        uint32_t anchor_id,
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
  Document* top_document = GetTopDocument(anchor_element);
  if (!anchor_element.Href().ProtocolIsInHTTPFamily() || !top_document ||
      !top_document->Url().ProtocolIsInHTTPFamily() ||
      !anchor_element.GetDocument().Url().ProtocolIsInHTTPFamily()) {
    return;
  }
  if (!AssociateInterface()) {
    return;
  }
  base::TimeDelta navigation_start_to_click =
      clock_->NowTicks() - NavigationStart(anchor_element);
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
  anchor_elements_to_report_.insert(&element);
  RegisterForLifecycleNotifications();
}

void AnchorElementMetricsSender::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_elements_to_report_);
  visitor->Trace(metrics_host_);
  visitor->Trace(intersection_observer_);
  visitor->Trace(update_timer_);
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
      clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(document.IsInOutermostMainFrame());
  DCHECK(clock_);

  intersection_observer_ = IntersectionObserver::Create(
      document,
      WTF::BindRepeating(&AnchorElementMetricsSender::UpdateVisibleAnchors,
                         WrapWeakPersistent(this)),
      LocalFrameUkmAggregator::kAnchorElementMetricsIntersectionObserver,
      {.thresholds = {kIntersectionRatioThreshold}, .delay = 100});
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
      EnqueueLeftViewport(anchor_element);
    } else {
      // The anchor is visible.
      EnqueueEnteredViewport(anchor_element);
    }
  }

  RegisterForLifecycleNotifications();
}

base::TimeTicks AnchorElementMetricsSender::NavigationStart(
    const HTMLAnchorElement& element) {
  if (mock_navigation_start_for_testing_.has_value()) {
    return mock_navigation_start_for_testing_.value();
  }

  Document* top_document = GetTopDocument(element);
  DCHECK(top_document);

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
          clock_->NowTicks() - NavigationStart(element);
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
        clock_->NowTicks() - NavigationStart(element);
    auto msg = mojom::blink::AnchorElementPointerDown::New(
        anchor_id, navigation_start_to_pointer_down);
    metrics_host_->ReportAnchorElementPointerDown(std::move(msg));
  }
}

void AnchorElementMetricsSender::EnqueueLeftViewport(
    const HTMLAnchorElement& element) {
  const auto anchor_id = AnchorElementId(element);
  auto it = anchor_elements_timing_stats_.find(anchor_id);
  DCHECK(it != anchor_elements_timing_stats_.end());
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
  DCHECK(it != anchor_elements_timing_stats_.end());
  AnchorElementTimingStats& timing_stats = it->value;
  timing_stats.viewport_entry_time_ = clock_->NowTicks();
  if (!timing_stats.entered_viewport_should_be_enqueued_) {
    return;
  }
  timing_stats.entered_viewport_should_be_enqueued_ = false;

  base::TimeDelta time_entered_viewport =
      clock_->NowTicks() - NavigationStart(element);
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

    int random = base::RandInt(1, random_anchor_sampling_period_);
    if (random == 1) {
      // This anchor element is sampled in.
      const auto anchor_id = AnchorElementId(anchor_element);
      anchor_elements_timing_stats_.insert(anchor_id,
                                           AnchorElementTimingStats{});
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

  if (metrics_.empty() && entered_viewport_messages_.empty() &&
      left_viewport_messages_.empty()) {
    return;
  }

  if (!AssociateInterface()) {
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
