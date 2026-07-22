// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"

#include <optional>

#include "base/feature_list.h"
#include "cc/layers/heads_up_display_layer.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/text_element_timing.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"

namespace blink {

TextPaintTimingDetector::TextPaintTimingDetector(
    PaintTimingDetector* paint_timing_detector)
    : paint_timing_detector_(paint_timing_detector) {}

void TextPaintTimingDetector::SendRectsToHud() {
  LocalFrameView* frame_view =
      paint_timing_detector_->GetPaintTiming().GetDocument()->View();
  auto* hud_layer =
      paint_timing::GetHUDLayerIfContentfulPaintRectsEnabled(frame_view);
  if (!hud_layer) {
    return;
  }

  LocalFrame& main_frame = frame_view->GetFrame().LocalFrameRoot();
  FrameWidget* widget = main_frame.GetWidgetForLocalRoot();
  if (!widget) {
    return;
  }

  bool is_recording_lcp = IsRecordingLargestTextPaint();

  for (const auto& record : texts_queued_for_paint_time_) {
    if (record->FrameIndex() == frame_index_) {
      cc::WebVitalMetricType type;
      if (record->GetSoftNavigationContext()) {
        type = cc::WebVitalMetricType::kInteractionContentfulPaint;
      } else if (is_recording_lcp) {
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

OptionalPaintTimingDetectorCallback<TextRecord>
TextPaintTimingDetector::TakePaintTimingCallback() {
  if (!added_entry_in_latest_frame_)
    return std::nullopt;

  // Do this before incrementing frame_index_;
  SendRectsToHud();

  added_entry_in_latest_frame_ = false;
  return blink::BindOnce(
      &TextPaintTimingDetector::AssignPaintTimeToQueuedRecords,
      WrapWeakPersistent(this), frame_index_++);
}

void TextPaintTimingDetector::ResetPaintTrackingOnInteraction(
    const LayoutObject& object) {
  if (auto iter = recorded_set_.find(&object); iter != recorded_set_.end()) {
    iter->value = TextPaintStatus::kAllowRepaint;
  }
}

bool TextPaintTimingDetector::ShouldWalkObject(
    const LayoutBoxModelObject& aggregator) {
  if (!aggregator.GetNode()) {
    return false;
  }
  // Walk the object unless it's ineligible for paint tracking (previously
  // painted, no repaint allowed). This ensures we retry empty aggregators, e.g.
  // if text nodes are appended later.
  auto iter = recorded_set_.find(&aggregator);
  return iter == recorded_set_.end() ||
         iter->value == TextPaintStatus::kAllowRepaint;
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

  gfx::RectF mapped_visual_rect = paint_timing_detector_->CalculateVisualRect(
      aggregated_visual_rect, property_tree_state);
  uint64_t effective_visual_size = mapped_visual_rect.size().GetArea();

  TextRecord* record =
      CreateTextRecord(aggregator, effective_visual_size, property_tree_state,
                       aggregated_visual_rect, mapped_visual_rect);

  if (IgnorePaintTimingScope::IgnoreDepth()) {
    if (auto* manager = GetLargestContentfulPaintManager()) {
      manager->MaybeUpdateLargestIgnoredText(aggregator, record);
    }
    return;
  }

  // Mark the text as recorded regardless of if this is needed for any
  // PaintTiming clients so the text isn't reconsidered as a candidate.
  auto result = recorded_set_.Set(&aggregator, TextPaintStatus::kPainted);
  bool is_repaint = !result.is_new_entry;

  if (auto* manager = GetLargestContentfulPaintManager()) {
    manager->InitializePaintTracking(record);
  }
  LocalDOMWindow* window = aggregator.GetDocument().domWindow();
  CHECK(window);
  if (SoftNavigationHeuristics* heuristics =
          window->GetSoftNavigationHeuristics()) {
    heuristics->InitializePaintTracking(record);
    if (auto* context = record->GetSoftNavigationContext()) {
      context->AddPaintedArea(record);
    }
  }
  record->SetIsNeededForElementTiming(
      !is_repaint && TextElementTiming::NeededForTiming(*record->GetNode()));

  // If any client needs this `record`, register for presentation time.
  if (record->IsNeededForLargestContentfulPaint() ||
      record->IsNeededForInteractionContentfulPaint() ||
      record->IsNeededForElementTiming()) {
    QueueToMeasurePaintTime(record);
  }

  // TODO(crbug.com/503691215): This is done before the opacity check for
  // images; why the difference?
  if (PaintTimingVisualizer* visualizer =
          paint_timing_detector_->Visualizer()) {
    visualizer->DumpTextDebuggingRect(aggregator, mapped_visual_rect);
  }
}

bool TextPaintTimingDetector::IsRecordingLargestTextPaint() const {
  return !!GetLargestContentfulPaintManager();
}

void TextPaintTimingDetector::ReportLargestIgnoredText() {
  auto* lcp_manager = GetLargestContentfulPaintManager();
  if (!lcp_manager) {
    return;
  }
  TextRecord* record = lcp_manager->TakeLargestIgnoredText();
  if (!record) {
    return;
  }

  // Trigger FCP if it's not already set.
  paint_timing_detector_->GetPaintTiming().MarkFirstContentfulPaint();

  recorded_set_.insert(record->GetNode()->GetLayoutObject(),
                       TextPaintStatus::kPainted);
  QueueToMeasurePaintTime(record);
}

void TextPaintTimingDetector::Trace(Visitor* visitor) const {
  visitor->Trace(recorded_set_);
  visitor->Trace(texts_queued_for_paint_time_);
  visitor->Trace(paint_timing_detector_);
}

void TextPaintTimingDetector::AssignPaintTimeToQueuedRecords(
    uint32_t frame_index,
    const base::TimeTicks& timestamp,
    const DOMPaintTimingInfo& paint_timing_info,
    HeapVector<Member<TextRecord>>& settled_records) {
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

    settled_records.push_back(record);
  }
}

TextRecord* TextPaintTimingDetector::CreateTextRecord(
    const LayoutObject& object,
    uint64_t effective_visual_size,
    const PropertyTreeStateOrAlias& property_tree_state,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect) {
  Node* node = object.GetNode();
  CHECK(node);

  if (effective_visual_size == 0u) {
    return MakeGarbageCollected<TextRecord>(
        node, effective_visual_size, gfx::RectF(), gfx::Rect(), gfx::RectF());
  } else {
    return MakeGarbageCollected<TextRecord>(
        node, effective_visual_size,
        TextElementTiming::ComputeIntersectionRect(object, frame_visual_rect,
                                                   property_tree_state),
        frame_visual_rect, root_visual_rect);
  }
}

LargestContentfulPaintManager*
TextPaintTimingDetector::GetLargestContentfulPaintManager() const {
  return paint_timing_detector_->GetPaintTiming()
      .GetLargestContentfulPaintManager();
}

}  // namespace blink
