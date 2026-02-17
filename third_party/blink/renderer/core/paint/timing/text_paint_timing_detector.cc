// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"

#include <memory>

#include "base/feature_list.h"
#include "cc/layers/heads_up_display_layer.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_callback_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {

TextPaintTimingDetector::TextPaintTimingDetector(
    LocalFrameView* frame_view,
    PaintTimingDetector* paint_timing_detector)
    : frame_view_(frame_view),
      paint_timing_detector_(paint_timing_detector),
      ltp_manager_(frame_view) {}

void TextPaintTimingDetector::SendRectsToHud() {
  auto* hud_layer =
      paint_timing::GetHUDLayerIfContentfulPaintRectsEnabled(frame_view_);
  if (!hud_layer) {
    return;
  }

  LocalFrame& main_frame = frame_view_->GetFrame().LocalFrameRoot();
  FrameWidget* widget = main_frame.GetWidgetForLocalRoot();
  if (!widget) {
    return;
  }

  for (const auto& record : texts_queued_for_paint_time_) {
    if (record->FrameIndex() == frame_index_) {
      cc::WebVitalMetricType type;
      if (record->GetSoftNavigationContext()) {
        type = cc::WebVitalMetricType::kInteractionContentfulPaint;
      } else if (IsRecordingLargestTextPaint()) {
        type = cc::WebVitalMetricType::kNavigationContentfulPaint;
      } else {
        continue;
      }
      hud_layer->AddWebVitalsDebugRect(
          {type, gfx::ToEnclosedRect(
                     widget->DIPsToBlinkSpace(record->RootVisualRect()))});
    }
  }
}

OptionalPaintTimingCallback TextPaintTimingDetector::TakePaintTimingCallback() {
  if (!added_entry_in_latest_frame_)
    return std::nullopt;

  // Do this before incrementing frame_index_;
  SendRectsToHud();

  added_entry_in_latest_frame_ = false;
  auto callback =
      blink::BindOnce(&TextPaintTimingDetector::AssignPaintTimeToQueuedRecords,
                      WrapWeakPersistent(this), frame_index_++);
  if (!callback_manager_) {
    return callback;
  }

  // This is for unit-tests only.
  callback_manager_->RegisterCallback(std::move(callback));
  return std::nullopt;
}

void TextPaintTimingDetector::LayoutObjectWillBeDestroyed(
    const LayoutObject& object) {
  if (const TextRecord* record = ltp_manager_.LargestIgnoredText();
      record && record->GetNode() == object.GetNode()) {
    ltp_manager_.TakeLargestIgnoredText();
  }
}

void TextPaintTimingDetector::ResetPaintTrackingOnInteraction(
    const LayoutObject& object) {
  if (auto iter = recorded_set_.find(&object); iter != recorded_set_.end()) {
    iter->value = TextPaintStatus::kAllowRepaint;
  }
}

bool TextPaintTimingDetector::ShouldWalkObject(
    const LayoutBoxModelObject& aggregator) {
  Node* node = aggregator.GetNode();
  if (!node)
    return false;

  // Do not walk the object if it has already been recorded, unless it has
  // specifically been marked for "re-walking" or allowing repaint.
  if (auto iter = recorded_set_.find(&aggregator);
      iter != recorded_set_.end() &&
      iter->value != TextPaintStatus::kAllowRepaint) {
    // TODO(crbug.com/40220033): rewalkable_set_ should be empty most of the
    // time, until we ship the feature for custom fonts.
    // HashSet::Contains() appears to hash key even when container is empty.
    return !rewalkable_set_.empty() && rewalkable_set_.Contains(&aggregator);
  }

  // Check if we know for certain that we need to measure this node, first.
  if (IsRecordingLargestTextPaint() ||
      TextElementTiming::NeededForTiming(*node)) {
    return true;
  }

  // If we haven't seen this node before, an we aren't recording LCP nor is this
  // node needed for element timing, the only remaining reason to measure text
  // timing is for soft navs paints.  We leave this check for last, just because
  // it might be more expensive.
  // TODO(crbug.com/423670827): If we cache this value during pre-paint, then we
  // might not need to worry about it.
  if (LocalDOMWindow* window = frame_view_->GetFrame().DomWindow()) {
    if (SoftNavigationHeuristics* heuristics =
            window->GetSoftNavigationHeuristics();
        heuristics && heuristics->MaybeGetSoftNavigationContextForTiming(
                          aggregator.GetNode())) {
      return true;
    }
  }

  // If we've decided not to visit this node for any reason, then let's add it
  // to the set of recorded nodes, even without measuring its paint, so we never
  // bother to check it again.
  // TODO(crbug.com/423670827): Part of the motivation for doing this is so we
  // don't try to look up context more than once per node.  But then this
  // content becomes un-recorded for any future observers, and that isn't always
  // correct (i.e. late application of elementtiming or an Interaction which
  // toggles content within the node, i.e. adding textContent for the first time
  // to a previously empty node.)
  recorded_set_.insert(&aggregator, TextPaintStatus::kPainted);
  return false;
}

void TextPaintTimingDetector::RecordAggregatedText(
    const LayoutBoxModelObject& aggregator,
    const gfx::Rect& aggregated_visual_rect,
    const PropertyTreeStateOrAlias& property_tree_state) {
  bool is_color_transparent = aggregator.StyleRef()
                                  .VisitedDependentColor(GetCSSPropertyColor())
                                  .IsFullyTransparent();
  bool has_shadow = !!aggregator.StyleRef().TextShadow();
  bool has_text_stroke = aggregator.StyleRef().TextStrokeWidth();

  if (is_color_transparent && !has_shadow && !has_text_stroke) {
    return;
  }

  DCHECK(ShouldWalkObject(aggregator));

  // The caller should check this.
  DCHECK(!aggregated_visual_rect.IsEmpty());

  gfx::RectF mapped_visual_rect =
      frame_view_->GetPaintTimingDetector().CalculateVisualRect(
          aggregated_visual_rect, property_tree_state);
  uint64_t aggregated_size = mapped_visual_rect.size().GetArea();

  DCHECK_LE(IgnorePaintTimingScope::IgnoreDepth(), 1);
  // Record the largest aggregated text that is hidden due to documentElement
  // being invisible but by no other reason (i.e. IgnoreDepth() needs to be 1).
  if (IgnorePaintTimingScope::IgnoreDepth() == 1) {
    if (IgnorePaintTimingScope::IsDocumentElementInvisible() &&
        IsRecordingLargestTextPaint()) {
      ltp_manager_.MaybeUpdateLargestIgnoredText(aggregator, aggregated_size,
                                                 aggregated_visual_rect,
                                                 mapped_visual_rect);
    }
    return;
  }

  // Web font styled node should be rewalkable so that resizing during swap
  // would make the node eligible to be LCP candidate again.
  if (RuntimeEnabledFeatures::WebFontResizeLCPEnabled()) {
    if (aggregator.StyleRef().GetFont()->HasCustomFont()) {
      rewalkable_set_.insert(&aggregator);
    }
  }

  SoftNavigationContext* context = nullptr;
  if (LocalDOMWindow* window = frame_view_->GetFrame().DomWindow()) {
    if (SoftNavigationHeuristics* heuristics =
            window->GetSoftNavigationHeuristics()) {
      context = heuristics->MaybeGetSoftNavigationContextForTiming(
          aggregator.GetNode());
    }
  }

  auto result = recorded_set_.Set(&aggregator, TextPaintStatus::kPainted);
  TextRecord* record = MaybeRecordTextRecord(
      aggregator, aggregated_size, property_tree_state, aggregated_visual_rect,
      mapped_visual_rect, context, /*is_repaint=*/!result.is_new_entry);
  if (context && record) {
    context->AddPaintedArea(record);
  }
  if (PaintTimingVisualizer* visualizer =
          frame_view_->GetPaintTimingDetector().Visualizer()) {
    visualizer->DumpTextDebuggingRect(aggregator, mapped_visual_rect);
  }
}

void TextPaintTimingDetector::StopRecordingLargestTextPaint() {
  recording_largest_text_paint_ = false;
}

void TextPaintTimingDetector::ReportLargestIgnoredText() {
  TextRecord* record = ltp_manager_.TakeLargestIgnoredText();
  // If the content has been removed, abort. It was never visible.
  if (!record || !record->GetNode() || !record->GetNode()->GetLayoutObject()) {
    return;
  }

  // Trigger FCP if it's not already set.
  Document* document = frame_view_->GetFrame().GetDocument();
  DCHECK(document);
  PaintTiming::From(*document).MarkFirstContentfulPaint();

  recorded_set_.insert(record->GetNode()->GetLayoutObject(),
                       TextPaintStatus::kPainted);
  QueueToMeasurePaintTime(record);
}

void TextPaintTimingDetector::Trace(Visitor* visitor) const {
  visitor->Trace(callback_manager_);
  visitor->Trace(frame_view_);
  visitor->Trace(text_element_timing_);
  visitor->Trace(rewalkable_set_);
  visitor->Trace(recorded_set_);
  visitor->Trace(texts_queued_for_paint_time_);
  visitor->Trace(ltp_manager_);
  visitor->Trace(paint_timing_detector_);
}

LargestTextPaintManager::LargestTextPaintManager(LocalFrameView* frame_view)
    : frame_view_(frame_view) {}

void LargestTextPaintManager::MaybeUpdateLargestIgnoredText(
    const LayoutObject& object,
    const uint64_t& size,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect) {
  if (size && (!largest_ignored_text_ ||
               size > largest_ignored_text_->RecordedSize())) {
    largest_ignored_text_ = MakeGarbageCollected<TextRecord>(
        object.GetNode(), size, gfx::RectF(), frame_visual_rect,
        root_visual_rect, /*is_needed_for_timing=*/false,
        /*soft_navigation_context=*/nullptr);
  }
}

void LargestTextPaintManager::Trace(Visitor* visitor) const {
  visitor->Trace(largest_ignored_text_);
  visitor->Trace(frame_view_);
}

void TextPaintTimingDetector::AssignPaintTimeToQueuedRecords(
    uint32_t frame_index,
    const base::TimeTicks& timestamp,
    const DOMPaintTimingInfo& paint_timing_info) {
  if (!text_element_timing_) {
    if (Document* document = frame_view_->GetFrame().GetDocument()) {
      if (LocalDOMWindow* window = document->domWindow()) {
        text_element_timing_ = MakeGarbageCollected<TextElementTiming>(*window);
      }
    }
  }

  LargestContentfulPaintCalculator* lcp_calculator =
      paint_timing_detector_->GetLargestContentfulPaintCalculator();
  bool is_needed_for_lcp = IsRecordingLargestTextPaint();
  bool can_report_timing =
      text_element_timing_ ? text_element_timing_->CanReportElements() : false;
  TextRecord* largest_removed_text = nullptr;

  while (!texts_queued_for_paint_time_.empty()) {
    TextRecord* record = texts_queued_for_paint_time_.front().Get();
    // `texts_queued_for_paint_time_` is in frame index order, so we're done
    // when we find an entry for a later frame.
    if (record->FrameIndex() > frame_index) {
      break;
    }
    texts_queued_for_paint_time_.pop_front();

    CHECK(!record->HasPaintTime());
    record->SetPaintTime(timestamp, paint_timing_info);

    // `record` may have been removed from the `recorded_set_` because the node
    // was detached from the DOM but left in `texts_queued_for_paint_time_` to
    // record paint and presentation time for soft navigation heuristics. To
    // match current LCP and element timing behavior, we don't want such nodes
    // to be LCP/timing candidates.
    //
    // TODO(crbug.com/454082773): we should consider allowing these to be LCP
    // candidates since they would have been shown to the user, and since it
    // better matches the LCP spec.
    //
    // Note: we can't check `recorded_set_` here to detect removal because the
    // `largest_ignored_text_` is not added to that set when document opacity
    // changes to a non-zero value (crbug.com/459517297).
    if (record->WasNodeRemoved()) {
      if (is_needed_for_lcp && record->RecordedSize() > 0u &&
          (!largest_removed_text ||
           largest_removed_text->RecordedSize() < record->RecordedSize())) {
        largest_removed_text = record;
      }
      continue;
    }

    if (can_report_timing && record->IsNeededForElementTiming()) {
      text_element_timing_->OnTextObjectPainted(*record, paint_timing_info);
    }

    if (is_needed_for_lcp && record->RecordedSize() > 0u) {
      lcp_calculator->MaybeUpdateLargestText(record);
    }
  }

  if (largest_removed_text) {
    CHECK(lcp_calculator);
    lcp_calculator->MaybeRecordRemovedCandidateUseCounter(
        *largest_removed_text);
  }
}

TextRecord* TextPaintTimingDetector::MaybeRecordTextRecord(
    const LayoutObject& object,
    const uint64_t& visual_size,
    const PropertyTreeStateOrAlias& property_tree_state,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect,
    SoftNavigationContext* context,
    bool is_repaint) {
  Node* node = object.GetNode();
  DCHECK(node);

  bool is_needed_for_lcp = IsRecordingLargestTextPaint() && visual_size > 0u;
  bool is_needed_for_element_timing =
      !is_repaint && TextElementTiming::NeededForTiming(*node);
  bool is_needed_for_soft_navs = context != nullptr;

  // If the node is not required by LCP and not required by ElementTiming,
  // we can bail out early.
  if (!is_needed_for_lcp && !is_needed_for_element_timing &&
      !is_needed_for_soft_navs) {
    return nullptr;
  }

  TextRecord* record;
  if (visual_size == 0u) {
    record = MakeGarbageCollected<TextRecord>(
        node, visual_size, gfx::RectF(), gfx::Rect(), gfx::RectF(),
        is_needed_for_element_timing, context);
  } else {
    record = MakeGarbageCollected<TextRecord>(
        node, visual_size,
        TextElementTiming::ComputeIntersectionRect(
            object, frame_visual_rect, property_tree_state, frame_view_),
        frame_visual_rect, root_visual_rect, is_needed_for_element_timing,
        context);
  }

  QueueToMeasurePaintTime(record);
  return record;
}

}  // namespace blink
