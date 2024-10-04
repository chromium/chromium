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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/anchor_element_metrics.h"
#include "third_party/blink/renderer/core/html/anchor_element_viewport_position_tracker.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/pointer_type_names.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-shared.h"

namespace blink {
namespace {
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

bool ShouldReportViewportPositions() {
  return base::FeatureList::IsEnabled(
      features::kNavigationPredictorNewViewportFeatures);
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
    const HTMLAnchorElementBase& anchor_element) {
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

void AnchorElementMetricsSender::AddAnchorElement(
    HTMLAnchorElementBase& element) {
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
    HTMLAnchorElementBase& element) {
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
    // browser about it. So we'll inform the browser of its removal so it can
    // prune its memory usage for old elements.
    removed_anchors_to_report_.push_back(AnchorElementId(element));

    if (auto* viewport_position_tracker =
            AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(
                *GetSupplementable())) {
      viewport_position_tracker->RemoveAnchor(element);
    }
  }
  RegisterForLifecycleNotifications();
}

void AnchorElementMetricsSender::DocumentDetached(Document& document) {
  // We don't need to do anything if the main frame's document is being detached
  // as we don't want to notify the browser of anchors being removed in that
  // scenario.
  if (document.IsInMainFrame()) {
    return;
  }
  // We also don't need to do anything if a subframe is being detached as part
  // of the main frame being detached, or when a navigation is committing.
  LocalFrame* main_frame = GetSupplementable()->GetFrame();
  CHECK(main_frame);
  if (!main_frame->IsAttached() ||
      main_frame->Loader().IsCommittingNavigation()) {
    return;
  }
  for (Element* element : *(document.links())) {
    HTMLAnchorElementBase* anchor = To<HTMLAnchorElementBase>(element);
    RemoveAnchorElement(*anchor);
  }
}

void AnchorElementMetricsSender::Trace(Visitor* visitor) const {
  visitor->Trace(anchor_elements_to_report_);
  visitor->Trace(metrics_host_);
  visitor->Trace(update_timer_);
  Supplement<Document>::Trace(visitor);
  AnchorElementViewportPositionTracker::Observer::Trace(visitor);
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

  if (auto* viewport_position_tracker =
          AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(document)) {
    viewport_position_tracker->AddObserver(this);
  }
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

base::TimeTicks AnchorElementMetricsSender::NavigationStart() const {
  if (mock_navigation_start_for_testing_.has_value()) {
    return mock_navigation_start_for_testing_.value();
  }

  const Document* top_document = GetSupplementable();
  CHECK(top_document);

  return top_document->Loader()->GetTiming().NavigationStart();
}

void AnchorElementMetricsSender::MaybeReportAnchorElementPointerEvent(
    HTMLAnchorElementBase& element,
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

void AnchorElementMetricsSender::EnqueueLeftViewport(
    const HTMLAnchorElementBase& element) {
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
    const HTMLAnchorElementBase& element) {
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

  auto* viewport_position_tracker =
      AnchorElementViewportPositionTracker::MaybeGetOrCreateFor(
          *GetSupplementable());

  for (const auto& member_element : anchor_elements_to_report_) {
    HTMLAnchorElementBase& anchor_element = *member_element;

    mojom::blink::AnchorElementMetricsPtr anchor_element_metrics =
        CreateAnchorElementMetrics(anchor_element);
    if (!anchor_element_metrics) {
      continue;
    }

    int random = base::RandInt(1, random_anchor_sampling_period_);
    if (random == 1) {
      // This anchor element is sampled in.
      if (viewport_position_tracker) {
        HTMLAnchorElementBase* anchor_unobserved =
            viewport_position_tracker->MaybeObserveAnchor(
                anchor_element, *anchor_element_metrics);
        if (anchor_unobserved) {
          EnqueueLeftViewport(*anchor_unobserved);
        }
      }
      const auto anchor_id = AnchorElementId(anchor_element);
      anchor_elements_timing_stats_.insert(anchor_id,
                                           AnchorElementTimingStats{});
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

void AnchorElementMetricsSender::ViewportIntersectionUpdate(
    const HeapVector<Member<const HTMLAnchorElementBase>>& entered_viewport,
    const HeapVector<Member<const HTMLAnchorElementBase>>& left_viewport) {
  if (!GetSupplementable()->GetFrame()) {
    return;
  }

  for (const HTMLAnchorElementBase* anchor : entered_viewport) {
    EnqueueEnteredViewport(*anchor);
  }
  for (const HTMLAnchorElementBase* anchor : left_viewport) {
    EnqueueLeftViewport(*anchor);
  }

  RegisterForLifecycleNotifications();
}

void AnchorElementMetricsSender::AnchorPositionsUpdated(
    HeapVector<Member<AnchorPositionUpdate>>& position_updates) {
  CHECK(ShouldReportViewportPositions());

  for (AnchorPositionUpdate* update : position_updates) {
    position_update_messages_.push_back(
        mojom::blink::AnchorElementPositionUpdate::New(
            AnchorElementId(*update->anchor_element), update->vertical_position,
            update->distance_from_pointer_down));
  }

  MaybeUpdateMetrics();
}

}  // namespace blink
