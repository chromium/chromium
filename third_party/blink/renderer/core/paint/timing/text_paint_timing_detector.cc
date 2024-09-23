// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"

#include <memory>

#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

void TextRecord::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
}

TextPaintTimingDetector::TextPaintTimingDetector(
    LocalFrameView* frame_view,
    PaintTimingDetector* paint_timing_detector,
    PaintTimingCallbackManager* callback_manager)
    : callback_manager_(callback_manager),
      frame_view_(frame_view),
      ltp_manager_(MakeGarbageCollected<LargestTextPaintManager>(
          frame_view,
          paint_timing_detector)) {}

void LargestTextPaintManager::PopulateTraceValue(
    TracedValue& value,
    const TextRecord& first_text_paint) {
  value.SetInteger("DOMNodeId",
                   static_cast<int>(first_text_paint.node_->GetDomNodeId()));
  value.SetInteger("size", static_cast<int>(first_text_paint.recorded_size));
  value.SetInteger("candidateIndex", ++count_candidates_);
  value.SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value.SetBoolean("isOutermostMainFrame",
                   frame_view_->GetFrame().IsOutermostMainFrame());
  value.SetBoolean("isEmbeddedFrame",
                   !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame() ||
                       frame_view_->GetFrame().IsInFencedFrameTree());
  if (first_text_paint.lcp_rect_info_) {
    first_text_paint.lcp_rect_info_->OutputToTraceValue(value);
  }
}

void LargestTextPaintManager::ReportCandidateToTrace(
    const TextRecord& largest_text_record) {
  if (!PaintTimingDetector::IsTracing())
    return;
  auto value = std::make_unique<TracedValue>();
  PopulateTraceValue(*value, largest_text_record);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2(
      "loading", "LargestTextPaint::Candidate", largest_text_record.paint_time,
      "data", std::move(value), "frame",
      GetFrameIdForTracing(&frame_view_->GetFrame()));
}

std::pair<TextRecord*, bool> LargestTextPaintManager::UpdateMetricsCandidate() {
  if (!largest_text_) {
    return {nullptr, false};
  }
  const base::TimeTicks time = largest_text_->paint_time;
  const uint64_t size = largest_text_->recorded_size;
  CHECK(paint_timing_detector_);
  CHECK(paint_timing_detector_->GetLargestContentfulPaintCalculator());

  bool changed = paint_timing_detector_->GetLargestContentfulPaintCalculator()
                     ->NotifyMetricsIfLargestTextPaintChanged(time, size);
  if (changed) {
    // It is not possible for an update to happen with a candidate that has no
    // paint time.
    DCHECK(!time.is_null());
    ReportCandidateToTrace(*largest_text_);
  }
  return {largest_text_.Get(), changed};
}

void TextPaintTimingDetector::OnPaintFinished() {
  if (!added_entry_in_latest_frame_)
    return;

  // |WeakPersistent| guarantees that when |this| is killed,
  // the callback function will not be invoked.
  RegisterNotifyPresentationTime(
      WTF::BindOnce(&TextPaintTimingDetector::ReportPresentationTime,
                    WrapWeakPersistent(this), frame_index_++));
  added_entry_in_latest_frame_ = false;
}

void TextPaintTimingDetector::LayoutObjectWillBeDestroyed(
    const LayoutObject& object) {
  recorded_set_.erase(&object);
  rewalkable_set_.erase(&object);
  texts_queued_for_paint_time_.erase(&object);
}

void TextPaintTimingDetector::RegisterNotifyPresentationTime(
    PaintTimingCallbackManager::LocalThreadCallback callback) {
  callback_manager_->RegisterCallback(std::move(callback));
}

void TextPaintTimingDetector::ReportPresentationTime(
    uint32_t frame_index,
    base::TimeTicks timestamp) {
  if (!text_element_timing_) {
    Document* document = frame_view_->GetFrame().GetDocument();
    if (document) {
      LocalDOMWindow* window = document->domWindow();
      if (window) {
        text_element_timing_ = TextElementTiming::From(*window);
      }
    }
  }
  AssignPaintTimeToQueuedRecords(frame_index, timestamp);
}

bool TextPaintTimingDetector::ShouldWalkObject(
    const LayoutBoxModelObject& object) const {
  // TODO(crbug.com/933479): Use LayoutObject::GeneratingNode() to include
  // anonymous objects' rect.
  Node* node = object.GetNode();
  if (!node)
    return false;
  // If we have finished recording Largest Text Paint and the element is a
  // shadow element or has no elementtiming attribute, then we should not record
  // its text.
  if (!IsRecordingLargestTextPaint() &&
      !TextElementTiming::NeededForElementTiming(*node)) {
    return false;
  }

  if (rewalkable_set_.Contains(&object))
    return true;

  // This metric defines the size of a text block by its first size, so we
  // should not walk the object if it has been recorded.
  return !recorded_set_.Contains(&object);
}

void TextPaintTimingDetector::RecordAggregatedText(
    const LayoutBoxModelObject& aggregator,
    const gfx::Rect& aggregated_visual_rect,
    const PropertyTreeStateOrAlias& property_tree_state) {
  if (RuntimeEnabledFeatures::
          ExcludeTransparentTextsFromBeingLcpEligibleEnabled()) {
    bool is_color_transparent =
        aggregator.StyleRef()
            .VisitedDependentColor(GetCSSPropertyColor())
            .IsFullyTransparent();
    bool has_shadow = !!aggregator.StyleRef().TextShadow();
    bool has_text_stroke = aggregator.StyleRef().TextStrokeWidth();

    if (is_color_transparent && !has_shadow && !has_text_stroke) {
      return;
    }
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
      ltp_manager_->MaybeUpdateLargestIgnoredText(aggregator, aggregated_size,
                                                  aggregated_visual_rect,
                                                  mapped_visual_rect);
    }
    return;
  }

  // Web font styled node should be rewalkable so that resizing during swap
  // would make the node eligible to be LCP candidate again.
  if (RuntimeEnabledFeatures::WebFontResizeLCPEnabled()) {
    if (aggregator.StyleRef().GetFont().HasCustomFont()) {
      rewalkable_set_.insert(&aggregator);
    }
  }

  LocalFrame& frame = frame_view_->GetFrame();
  if (LocalDOMWindow* window = frame.DomWindow()) {
    if (SoftNavigationHeuristics* heuristics =
            SoftNavigationHeuristics::From(*window)) {
      heuristics->RecordPaint(
          &frame, mapped_visual_rect.size().GetArea(),
          aggregator.GetNode()->IsModifiedBySoftNavigation());
    }
  }
  recorded_set_.insert(&aggregator);
  MaybeRecordTextRecord(aggregator, aggregated_size, property_tree_state,
                        aggregated_visual_rect, mapped_visual_rect);
  if (std::optional<PaintTimingVisualizer>& visualizer =
          frame_view_->GetPaintTimingDetector().Visualizer()) {
    visualizer->DumpTextDebuggingRect(aggregator, mapped_visual_rect);
  }
}

void TextPaintTimingDetector::StopRecordingLargestTextPaint() {
  recording_largest_text_paint_ = false;
}

void TextPaintTimingDetector::RestartRecordingLargestTextPaint() {
  recording_largest_text_paint_ = true;
  texts_queued_for_paint_time_.clear();
  ltp_manager_->Clear();
}

void TextPaintTimingDetector::ReportLargestIgnoredText() {
  if (!ltp_manager_)
    return;
  TextRecord* record = ltp_manager_->PopLargestIgnoredText();
  // If the content has been removed, abort. It was never visible.
  if (!record || !record->node_ || !record->node_->GetLayoutObject())
    return;

  // Trigger FCP if it's not already set.
  Document* document = frame_view_->GetFrame().GetDocument();
  DCHECK(document);
  PaintTiming::From(*document).MarkFirstContentfulPaint();

  record->frame_index_ = frame_index_;
  QueueToMeasurePaintTime(*record->node_->GetLayoutObject(), record);
}

void TextPaintTimingDetector::Trace(Visitor* visitor) const {
  visitor->Trace(callback_manager_);
  visitor->Trace(frame_view_);
  visitor->Trace(text_element_timing_);
  visitor->Trace(rewalkable_set_);
  visitor->Trace(recorded_set_);
  visitor->Trace(text_element_timing_);
  visitor->Trace(texts_queued_for_paint_time_);
  visitor->Trace(ltp_manager_);
}

LargestTextPaintManager::LargestTextPaintManager(
    LocalFrameView* frame_view,
    PaintTimingDetector* paint_timing_detector)
    : frame_view_(frame_view), paint_timing_detector_(paint_timing_detector) {}

void LargestTextPaintManager::MaybeUpdateLargestText(TextRecord* record) {
  if (!largest_text_ || largest_text_->recorded_size < record->recorded_size) {
    largest_text_ = record;
  }
}

void LargestTextPaintManager::MaybeUpdateLargestIgnoredText(
    const LayoutObject& object,
    const uint64_t& size,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect) {
  if (size &&
      (!largest_ignored_text_ || size > largest_ignored_text_->recorded_size)) {
    // Create the largest ignored text with a |frame_index_| of 0. When it is
    // queued for paint, we'll set the appropriate |frame_index_|.
    largest_ignored_text_ = MakeGarbageCollected<TextRecord>(
        *object.GetNode(), size, gfx::RectF(), frame_visual_rect,
        root_visual_rect, 0u);
  }
}

void LargestTextPaintManager::Trace(Visitor* visitor) const {
  visitor->Trace(largest_text_);
  visitor->Trace(largest_ignored_text_);
  visitor->Trace(frame_view_);
  visitor->Trace(paint_timing_detector_);
}

void TextPaintTimingDetector::AssignPaintTimeToQueuedRecords(
    uint32_t frame_index,
    const base::TimeTicks& timestamp) {
  bool can_report_element_timing =
      text_element_timing_ ? text_element_timing_->CanReportElements() : false;
  HeapVector<Member<const LayoutObject>> keys_to_be_removed;
  for (const auto& it : texts_queued_for_paint_time_) {
    const auto& record = it.value;
    if (!record->paint_time.is_null() || record->frame_index_ > frame_index) {
      continue;
    }
    record->paint_time = timestamp;
    if (can_report_element_timing)
      text_element_timing_->OnTextObjectPainted(*record);

    if (ltp_manager_ && (record->recorded_size > 0u) &&
        !(record->node_ &&
          ltp_manager_->IsUnrelatedSoftNavigationPaint(*(record->node_)))) {
      ltp_manager_->MaybeUpdateLargestText(record);
    }
    keys_to_be_removed.push_back(it.key);
  }
  texts_queued_for_paint_time_.RemoveAll(keys_to_be_removed);
}

void TextPaintTimingDetector::MaybeRecordTextRecord(
    const LayoutObject& object,
    const uint64_t& visual_size,
    const PropertyTreeStateOrAlias& property_tree_state,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect) {
  Node* node = object.GetNode();
  DCHECK(node);
  // If the node is not required by LCP and not required by ElementTiming, we
  // can bail out early.
  if ((visual_size == 0u || !IsRecordingLargestTextPaint()) &&
      !TextElementTiming::NeededForElementTiming(*node)) {
    return;
  }
  TextRecord* record;
  if (visual_size == 0u) {
    record = MakeGarbageCollected<TextRecord>(
        *node, 0, gfx::RectF(), gfx::Rect(), gfx::RectF(), frame_index_);
  } else {
    record = MakeGarbageCollected<TextRecord>(
        *object.GetNode(), visual_size,
        TextElementTiming::ComputeIntersectionRect(
            object, frame_visual_rect, property_tree_state, frame_view_),
        frame_visual_rect, root_visual_rect, frame_index_);
  }
  QueueToMeasurePaintTime(object, record);
}

}  // namespace blink
