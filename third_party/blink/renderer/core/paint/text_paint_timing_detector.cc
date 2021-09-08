// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/text_paint_timing_detector.h"

#include <memory>

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

namespace {

bool LargeTextFirst(const TextRecord* a, const TextRecord* b) {
  DCHECK(a);
  DCHECK(b);
  if (a->first_size != b->first_size)
    return a->first_size > b->first_size;
  // This make sure that two different nodes with the same |first_size| wouldn't
  // be merged in the set.
  return a->insertion_index_ < b->insertion_index_;
}

}  // namespace

void TextRecord::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
}

TextPaintTimingDetector::TextPaintTimingDetector(
    LocalFrameView* frame_view,
    PaintTimingDetector* paint_timing_detector,
    PaintTimingCallbackManager* callback_manager)
    : records_manager_(frame_view, paint_timing_detector),
      callback_manager_(callback_manager),
      frame_view_(frame_view) {}

void LargestTextPaintManager::PopulateTraceValue(
    TracedValue& value,
    const TextRecord& first_text_paint) {
  value.SetInteger(
      "DOMNodeId",
      static_cast<int>(DOMNodeIds::IdForNode(first_text_paint.node_)));
  value.SetInteger("size", static_cast<int>(first_text_paint.first_size));
  value.SetInteger("candidateIndex", ++count_candidates_);
  value.SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value.SetBoolean("isOOPIF",
                   !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
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
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading", "LargestTextPaint::Candidate",
                                   largest_text_record.paint_time, "data",
                                   std::move(value), "frame",
                                   ToTraceValue(&frame_view_->GetFrame()));
}

void LargestTextPaintManager::ReportNoCandidateToTrace() {
  if (!PaintTimingDetector::IsTracing())
    return;
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("candidateIndex", ++count_candidates_);
  value->SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value->SetBoolean("isOOPIF",
                    !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
  TRACE_EVENT2("loading", "LargestTextPaint::NoCandidate", "data",
               std::move(value), "frame",
               ToTraceValue(&frame_view_->GetFrame()));
}

TextRecord* LargestTextPaintManager::UpdateCandidate() {
  TextRecord* largest_text_record = FindLargestPaintCandidate();
  const base::TimeTicks time =
      largest_text_record ? largest_text_record->paint_time : base::TimeTicks();
  const uint64_t size =
      largest_text_record ? largest_text_record->first_size : 0;
  DCHECK(paint_timing_detector_);
  bool changed =
      paint_timing_detector_->NotifyIfChangedLargestTextPaint(time, size);
  if (changed) {
    if (!time.is_null())
      ReportCandidateToTrace(*largest_text_record);
    else
      ReportNoCandidateToTrace();
  }
  return largest_text_record;
}

void TextPaintTimingDetector::OnPaintFinished() {
  if (need_update_timing_at_frame_end_) {
    need_update_timing_at_frame_end_ = false;
    frame_view_->GetPaintTimingDetector()
        .UpdateLargestContentfulPaintCandidate();
  }
  if (records_manager_.NeedMeausuringPaintTime()) {
    if (!awaiting_presentation_promise_) {
      // |WrapCrossThreadWeakPersistent| guarantees that when |this| is killed,
      // the callback function will not be invoked.
      RegisterNotifyPresentationTime(
          WTF::Bind(&TextPaintTimingDetector::ReportPresentationTime,
                    WrapCrossThreadWeakPersistent(this)));
    }
  }
}

void TextPaintTimingDetector::LayoutObjectWillBeDestroyed(
    const LayoutObject& object) {
  if (records_manager_.IsKnownVisible(object)) {
    records_manager_.RemoveVisibleRecord(object);
    need_update_timing_at_frame_end_ = true;
  } else if (records_manager_.IsKnownInvisible(object)) {
    records_manager_.RemoveInvisibleRecord(object);
    need_update_timing_at_frame_end_ = true;
  }
}

void TextPaintTimingDetector::RegisterNotifyPresentationTime(
    PaintTimingCallbackManager::LocalThreadCallback callback) {
  callback_manager_->RegisterCallback(std::move(callback));
  awaiting_presentation_promise_ = true;
}

void TextPaintTimingDetector::ReportPresentationTime(
    base::TimeTicks timestamp) {
  if (!records_manager_.HasTextElementTiming()) {
    Document* document = frame_view_->GetFrame().GetDocument();
    if (document) {
      LocalDOMWindow* window = document->domWindow();
      if (window) {
        records_manager_.SetTextElementTiming(
            &TextElementTiming::From(*window));
      }
    }
  }
  records_manager_.AssignPaintTimeToQueuedRecords(timestamp);
  if (IsRecordingLargestTextPaint())
    UpdateCandidate();
  awaiting_presentation_promise_ = false;
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
  if (!records_manager_.IsRecordingLargestTextPaint() &&
      !TextElementTiming::NeededForElementTiming(*node)) {
    return false;
  }

  // This metric defines the size of a text block by its first size, so we
  // should not walk the object if it has been recorded.
  return !records_manager_.HasRecorded(object);
}

void TextPaintTimingDetector::RecordAggregatedText(
    const LayoutBoxModelObject& aggregator,
    const IntRect& aggregated_visual_rect,
    const PropertyTreeStateOrAlias& property_tree_state) {
  DCHECK(ShouldWalkObject(aggregator));

  // The caller should check this.
  DCHECK(!aggregated_visual_rect.IsEmpty());

  FloatRect mapped_visual_rect =
      frame_view_->GetPaintTimingDetector().CalculateVisualRect(
          aggregated_visual_rect, property_tree_state);
  uint64_t aggregated_size = mapped_visual_rect.Size().Area();
  DCHECK_LE(IgnorePaintTimingScope::IgnoreDepth(), 1);
  // Record the largest aggregated text that is hidden due to documentElement
  // being invisible but by no other reason (i.e. IgnoreDepth() needs to be 1).
  if (IgnorePaintTimingScope::IgnoreDepth() == 1) {
    if (IgnorePaintTimingScope::IsDocumentElementInvisible() &&
        records_manager_.IsRecordingLargestTextPaint()) {
      records_manager_.MaybeUpdateLargestIgnoredText(
          aggregator, aggregated_size, aggregated_visual_rect,
          mapped_visual_rect);
    }
    return;
  }

  if (aggregated_size == 0) {
    records_manager_.RecordInvisibleObject(aggregator);
  } else {
    records_manager_.RecordVisibleObject(
        aggregator, aggregated_size,
        TextElementTiming::ComputeIntersectionRect(
            aggregator, aggregated_visual_rect, property_tree_state,
            frame_view_),
        aggregated_visual_rect, mapped_visual_rect);
    if (absl::optional<PaintTimingVisualizer>& visualizer =
            frame_view_->GetPaintTimingDetector().Visualizer()) {
      visualizer->DumpTextDebuggingRect(aggregator, mapped_visual_rect);
    }
  }
}

void TextPaintTimingDetector::StopRecordingLargestTextPaint() {
  records_manager_.CleanUpLargestTextPaint();
}

void TextPaintTimingDetector::ReportLargestIgnoredText() {
  records_manager_.ReportLargestIgnoredText();
}

void TextPaintTimingDetector::Trace(Visitor* visitor) const {
  visitor->Trace(records_manager_);
  visitor->Trace(frame_view_);
  visitor->Trace(callback_manager_);
}

LargestTextPaintManager::LargestTextPaintManager(
    LocalFrameView* frame_view,
    PaintTimingDetector* paint_timing_detector)
    : size_ordered_set_(&LargeTextFirst),
      frame_view_(frame_view),
      paint_timing_detector_(paint_timing_detector) {}

void LargestTextPaintManager::MaybeUpdateLargestIgnoredText(
    const LayoutObject& object,
    const uint64_t& size,
    const IntRect& frame_visual_rect,
    const FloatRect& root_visual_rect) {
  if (size &&
      (!largest_ignored_text_ || size > largest_ignored_text_->first_size)) {
    largest_ignored_text_ =
        MakeGarbageCollected<TextRecord>(*object.GetNode(), size, FloatRect(),
                                         frame_visual_rect, root_visual_rect);
  }
}

void LargestTextPaintManager::Trace(Visitor* visitor) const {
  visitor->Trace(cached_largest_paint_candidate_);
  visitor->Trace(largest_ignored_text_);
  visitor->Trace(frame_view_);
  visitor->Trace(paint_timing_detector_);
}

void TextRecordsManager::RemoveVisibleRecord(const LayoutObject& object) {
  DCHECK(visible_objects_.Contains(&object));
  auto it = visible_objects_.find(&object);
  if (ltp_manager_) {
    ltp_manager_->RemoveVisibleRecord(it->value);
  }
  texts_queued_for_paint_time_.erase(it->value);
  visible_objects_.erase(it);
}

void TextRecordsManager::CleanUpLargestTextPaint() {
  ltp_manager_.Clear();
}

void TextRecordsManager::RemoveInvisibleRecord(const LayoutObject& object) {
  DCHECK(invisible_objects_.Contains(&object));
  invisible_objects_.erase(&object);
  size_zero_texts_queued_for_paint_time_.erase(&object);
}

void TextRecordsManager::AssignPaintTimeToQueuedRecords(
    const base::TimeTicks& timestamp) {
  bool can_report_element_timing =
      text_element_timing_ ? text_element_timing_->CanReportElements() : false;
  for (const auto& record : texts_queued_for_paint_time_) {
    DCHECK_EQ(record->paint_time, base::TimeTicks());
    record->paint_time = timestamp;
    if (can_report_element_timing)
      text_element_timing_->OnTextObjectPainted(*record);
  }
  if (can_report_element_timing) {
    for (const auto& record : size_zero_texts_queued_for_paint_time_) {
      DCHECK_EQ(record.value->paint_time, base::TimeTicks());
      record.value->paint_time = timestamp;
      text_element_timing_->OnTextObjectPainted(*record.value);
    }
  }
  texts_queued_for_paint_time_.clear();
  size_zero_texts_queued_for_paint_time_.clear();
  if (ltp_manager_)
    ltp_manager_->SetCachedResultInvalidated(true);
}

void TextRecordsManager::RecordVisibleObject(
    const LayoutObject& object,
    const uint64_t& visual_size,
    const FloatRect& element_timing_rect,
    const IntRect& frame_visual_rect,
    const FloatRect& root_visual_rect) {
  DCHECK_GT(visual_size, 0u);

  TextRecord* record = MakeGarbageCollected<TextRecord>(
      *object.GetNode(), visual_size, element_timing_rect, frame_visual_rect,
      root_visual_rect);
  if (ltp_manager_)
    ltp_manager_->InsertRecord(record);

  QueueToMeasurePaintTime(record);
  visible_objects_.insert(&object, record);
}

void TextRecordsManager::RecordInvisibleObject(const LayoutObject& object) {
  invisible_objects_.insert(&object);
  Node* node = object.GetNode();
  DCHECK(node);
  if (!TextElementTiming::NeededForElementTiming(*node))
    return;
  // Since it is invisible, the record will have a size of 0 and an empty rect.
  TextRecord* record = MakeGarbageCollected<TextRecord>(*node, 0, FloatRect(),
                                                        IntRect(), FloatRect());
  size_zero_texts_queued_for_paint_time_.insert(&object, record);
}

void TextRecordsManager::ReportLargestIgnoredText() {
  if (!ltp_manager_)
    return;
  TextRecord* record = ltp_manager_->PopLargestIgnoredText();
  // If the content has been removed, abort. It was never visible.
  if (!record || !record->node_ || !record->node_->GetLayoutObject())
    return;

  ltp_manager_->InsertRecord(record);
  QueueToMeasurePaintTime(record);
  visible_objects_.insert(record->node_->GetLayoutObject(), record);
}

TextRecord* LargestTextPaintManager::FindLargestPaintCandidate() {
  if (!is_result_invalidated_ && cached_largest_paint_candidate_) {
    return cached_largest_paint_candidate_;
  }
  TextRecord* new_largest_paint_candidate = nullptr;
  for (const auto& text_record : size_ordered_set_) {
    if (text_record->paint_time.is_null())
      continue;
    new_largest_paint_candidate = text_record;
    break;
  }
  cached_largest_paint_candidate_ = new_largest_paint_candidate;
  is_result_invalidated_ = false;
  return new_largest_paint_candidate;
}

TextRecordsManager::TextRecordsManager(
    LocalFrameView* frame_view,
    PaintTimingDetector* paint_timing_detector)
    : ltp_manager_(MakeGarbageCollected<LargestTextPaintManager>(
          frame_view,
          paint_timing_detector)) {}

void TextRecordsManager::Trace(Visitor* visitor) const {
  visitor->Trace(visible_objects_);
  visitor->Trace(invisible_objects_);
  visitor->Trace(text_element_timing_);
  visitor->Trace(ltp_manager_);
  visitor->Trace(size_zero_texts_queued_for_paint_time_);
  visitor->Trace(texts_queued_for_paint_time_);
}

}  // namespace blink
