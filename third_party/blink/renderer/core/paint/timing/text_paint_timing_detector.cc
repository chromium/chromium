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
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
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

  SoftNavigationContext* context = nullptr;
  LocalDOMWindow* window = aggregator.GetDocument().domWindow();
  CHECK(window);
  if (SoftNavigationHeuristics* heuristics =
          window->GetSoftNavigationHeuristics()) {
    context = heuristics->MaybeGetSoftNavigationContextForTiming(
        aggregator.GetNode());
  }

  auto result = recorded_set_.Set(&aggregator, TextPaintStatus::kPainted);
  TextRecord* record = MaybeRecordTextRecord(
      aggregator, aggregated_size, property_tree_state, aggregated_visual_rect,
      mapped_visual_rect, context, /*is_repaint=*/!result.is_new_entry);
  if (context && record) {
    context->AddPaintedArea(record);
  }
  if (PaintTimingVisualizer* visualizer =
          paint_timing_detector_->Visualizer()) {
    visualizer->DumpTextDebuggingRect(aggregator, mapped_visual_rect);
  }
}

void TextPaintTimingDetector::StopRecordingLargestTextPaint() {
  recording_largest_text_paint_ = false;
}

void TextPaintTimingDetector::ReportLargestIgnoredText() {
  TextRecord* record = ltp_manager_.TakeLargestIgnoredText();
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
  visitor->Trace(ltp_manager_);
  visitor->Trace(paint_timing_detector_);
}

LargestTextPaintManager::LargestTextPaintManager() = default;

void LargestTextPaintManager::MaybeUpdateLargestIgnoredText(
    const LayoutObject& object,
    const uint64_t size,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect) {
  if (!size) {
    return;
  }
  if (TextRecord* current_candidate = GetLargestIgnoredTextIfNotRemoved();
      current_candidate && current_candidate->RecordedSize() >= size) {
    return;
  }
  largest_ignored_text_.key = &object;
  largest_ignored_text_.value = MakeGarbageCollected<TextRecord>(
      object.GetNode(), size, gfx::RectF(), frame_visual_rect, root_visual_rect,
      /*is_needed_for_timing=*/false,
      /*soft_navigation_context=*/nullptr);
}

void LargestTextPaintManager::Trace(Visitor* visitor) const {
  visitor->Trace(largest_ignored_text_);
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
        TextElementTiming::ComputeIntersectionRect(object, frame_visual_rect,
                                                   property_tree_state),
        frame_visual_rect, root_visual_rect, is_needed_for_element_timing,
        context);
  }

  QueueToMeasurePaintTime(record);
  return record;
}

}  // namespace blink
