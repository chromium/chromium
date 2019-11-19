// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

namespace {

// In order for |rect_size| to align with the importance of the image, we
// use this heuristics to alleviate the effect of scaling. For example,
// an image has intrinsic size being 1x1 and scaled to 100x100, but only 50x100
// is visible in the viewport. In this case, |intrinsic_image_size| is 1x1;
// |displayed_image_size| is 100x100. |intrinsic_image_size| is 50x100.
// As the image do not have a lot of content, we down scale |visual_size| by the
// ratio of |intrinsic_image_size|/|displayed_image_size| = 1/10000.
//
// * |visual_size| referes to the size of the |displayed_image_size| after
// clipping and transforming. The size is in the main-frame's coordinate.
// * |displayed_image_size| refers to the paint size in the image object's
// coordinate.
// * |intrinsic_image_size| refers to the the image object's original size
// before scaling. The size is in the image object's coordinate.
uint64_t DownScaleIfIntrinsicSizeIsSmaller(
    uint64_t visual_size,
    const uint64_t& intrinsic_image_size,
    const uint64_t& displayed_image_size) {
  // This is an optimized equivalence to:
  // |visual_size| * min(|displayed_image_size|, |intrinsic_image_size|) /
  // |displayed_image_size|
  if (intrinsic_image_size < displayed_image_size) {
    DCHECK_GT(displayed_image_size, 0u);
    return static_cast<double>(visual_size) * intrinsic_image_size /
           displayed_image_size;
  }
  return visual_size;
}

}  // namespace

static bool LargeImageFirst(const base::WeakPtr<ImageRecord>& a,
                            const base::WeakPtr<ImageRecord>& b) {
  DCHECK(a);
  DCHECK(b);
  if (a->first_size != b->first_size)
    return a->first_size > b->first_size;
  // This make sure that two different |ImageRecord|s with the same |first_size|
  // wouldn't be merged in the |size_ordered_set_|.
  return a->insertion_index < b->insertion_index;
}

ImagePaintTimingDetector::ImagePaintTimingDetector(
    LocalFrameView* frame_view,
    PaintTimingCallbackManager* callback_manager)
    : records_manager_(frame_view),
      frame_view_(frame_view),
      callback_manager_(callback_manager) {}

void ImagePaintTimingDetector::PopulateTraceValue(
    TracedValue& value,
    const ImageRecord& first_image_paint) {
  value.SetInteger("DOMNodeId", static_cast<int>(first_image_paint.node_id));
  // The cached_image could have been deleted when this is called.
  value.SetString("imageUrl",
                  first_image_paint.cached_image
                      ? String(first_image_paint.cached_image->Url())
                      : "(deleted)");
  value.SetInteger("size", static_cast<int>(first_image_paint.first_size));
  value.SetInteger("candidateIndex", ++count_candidates_);
  value.SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value.SetBoolean("isOOPIF",
                   !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
}

void ImagePaintTimingDetector::ReportCandidateToTrace(
    ImageRecord& largest_image_record) {
  if (!PaintTimingDetector::IsTracing())
    return;
  DCHECK(!largest_image_record.paint_time.is_null());
  auto value = std::make_unique<TracedValue>();
  PopulateTraceValue(*value, largest_image_record);
  TRACE_EVENT_MARK_WITH_TIMESTAMP2("loading", "LargestImagePaint::Candidate",
                                   largest_image_record.paint_time, "data",
                                   std::move(value), "frame",
                                   ToTraceValue(&frame_view_->GetFrame()));
}

void ImagePaintTimingDetector::ReportNoCandidateToTrace() {
  if (!PaintTimingDetector::IsTracing())
    return;
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("candidateIndex", ++count_candidates_);
  value->SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value->SetBoolean("isOOPIF",
                    !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame());
  TRACE_EVENT2("loading", "LargestImagePaint::NoCandidate", "data",
               std::move(value), "frame",
               ToTraceValue(&frame_view_->GetFrame()));
}

ImageRecord* ImagePaintTimingDetector::UpdateCandidate() {
  ImageRecord* largest_image_record =
      records_manager_.FindLargestPaintCandidate();
  const base::TimeTicks time = largest_image_record
                                   ? largest_image_record->paint_time
                                   : base::TimeTicks();
  const uint64_t size =
      largest_image_record ? largest_image_record->first_size : 0;
  PaintTimingDetector& detector = frame_view_->GetPaintTimingDetector();
  // Two different candidates are rare to have the same time and size.
  // So when they are unchanged, the candidate is considered unchanged.
  bool changed = detector.NotifyIfChangedLargestImagePaint(time, size);
  if (changed) {
    if (!time.is_null()) {
      DCHECK(largest_image_record->loaded);
      ReportCandidateToTrace(*largest_image_record);
    } else {
      ReportNoCandidateToTrace();
    }
  }
  return largest_image_record;
}

void ImagePaintTimingDetector::OnPaintFinished() {
  frame_index_++;
  if (need_update_timing_at_frame_end_) {
    need_update_timing_at_frame_end_ = false;
    frame_view_->GetPaintTimingDetector()
        .UpdateLargestContentfulPaintCandidate();
  }

  if (!records_manager_.HasUnregisteredRecordsInQueued(
          last_registered_frame_index_))
    return;

  last_registered_frame_index_ = records_manager_.LastQueuedFrameIndex();
  RegisterNotifySwapTime();
}

void ImagePaintTimingDetector::LayoutObjectWillBeDestroyed(
    const LayoutObject& object) {
  if (!is_recording_)
    return;

  // The visible record removal has been handled by
  // |NotifyImageRemoved|.
  records_manager_.RemoveInvisibleRecordIfNeeded(object);
}

void ImagePaintTimingDetector::NotifyImageRemoved(
    const LayoutObject& object,
    const ImageResourceContent* cached_image) {
  if (!is_recording_)
    return;
  RecordId record_id = std::make_pair(&object, cached_image);
  records_manager_.RemoveImageFinishedRecord(record_id);
  if (!records_manager_.IsRecordedVisibleImage(record_id))
    return;
  records_manager_.RemoveVisibleRecord(record_id);
  need_update_timing_at_frame_end_ = true;
}

void ImagePaintTimingDetector::RegisterNotifySwapTime() {
  auto callback = WTF::Bind(&ImagePaintTimingDetector::ReportSwapTime,
                            WrapCrossThreadWeakPersistent(this),
                            last_registered_frame_index_);
  callback_manager_->RegisterCallback(std::move(callback));
  num_pending_swap_callbacks_++;
}

void ImagePaintTimingDetector::ReportSwapTime(
    unsigned last_queued_frame_index,
    base::TimeTicks timestamp) {
  if (!is_recording_)
    return;
  // The callback is safe from race-condition only when running on main-thread.
  DCHECK(ThreadState::Current()->IsMainThread());
  records_manager_.AssignPaintTimeToRegisteredQueuedRecords(
      timestamp, last_queued_frame_index);
  num_pending_swap_callbacks_--;
  DCHECK_GE(num_pending_swap_callbacks_, 0);
}

void ImageRecordsManager::AssignPaintTimeToRegisteredQueuedRecords(
    const base::TimeTicks& timestamp,
    unsigned last_queued_frame_index) {
  // TODO(crbug.com/971419): should guarantee the queue not empty.
  while (!images_queued_for_paint_time_.IsEmpty()) {
    base::WeakPtr<ImageRecord>& record = images_queued_for_paint_time_.front();
    if (!record) {
      images_queued_for_paint_time_.pop_front();
      continue;
    }
    if (record->frame_index > last_queued_frame_index)
      break;
    record->paint_time = timestamp;
    images_queued_for_paint_time_.pop_front();
  }
}

void ImagePaintTimingDetector::RecordImage(
    const LayoutObject& object,
    const IntSize& intrinsic_size,
    const ImageResourceContent& cached_image,
    const PropertyTreeState& current_paint_chunk_properties,
    const StyleFetchedImage* style_image) {
  Node* node = object.GetNode();
  if (!node)
    return;
  if (records_manager_.IsRecordedInvisibleImage(object))
    return;

  RecordId record_id = std::make_pair(&object, &cached_image);
  bool is_recored_visible_image =
      records_manager_.IsRecordedVisibleImage(record_id);
  if (is_recored_visible_image &&
      !records_manager_.IsVisibleImageLoaded(record_id) &&
      cached_image.IsLoaded()) {
    records_manager_.OnImageLoaded(record_id, frame_index_, style_image);
    need_update_timing_at_frame_end_ = true;
    if (base::Optional<PaintTimingVisualizer>& visualizer =
            frame_view_->GetPaintTimingDetector().Visualizer()) {
      FloatRect mapped_visual_rect =
          frame_view_->GetPaintTimingDetector().CalculateVisualRect(
              object.FragmentsVisualRectBoundingBox(),
              current_paint_chunk_properties);
      visualizer->DumpImageDebuggingRect(object, mapped_visual_rect,
                                         cached_image);
    }
    return;
  }

  if (is_recored_visible_image || !is_recording_)
    return;
  IntRect visual_rect = object.FragmentsVisualRectBoundingBox();
  // Before the image resource starts loading, <img> has no size info. We wait
  // until the size is known.
  if (visual_rect.IsEmpty())
    return;
  FloatRect mapped_visual_rect =
      frame_view_->GetPaintTimingDetector().CalculateVisualRect(
          visual_rect, current_paint_chunk_properties);
  if (base::Optional<PaintTimingVisualizer>& visualizer =
          frame_view_->GetPaintTimingDetector().Visualizer()) {
    visualizer->DumpImageDebuggingRect(object, mapped_visual_rect,
                                       cached_image);
  }
  uint64_t rect_size = mapped_visual_rect.Size().Area();
  // Transform visual rect to window before calling downscale.
  WebFloatRect float_visual_rect = FloatRect(visual_rect);
  frame_view_->GetPaintTimingDetector().ConvertViewportToWindow(
      &float_visual_rect);
  rect_size = DownScaleIfIntrinsicSizeIsSmaller(
      rect_size, intrinsic_size.Area(),
      float_visual_rect.width * float_visual_rect.height);
  if (rect_size == 0) {
    records_manager_.RecordInvisible(object);
  } else {
    records_manager_.RecordVisible(record_id, rect_size);
    if (cached_image.IsLoaded()) {
      records_manager_.OnImageLoaded(record_id, frame_index_, style_image);
      need_update_timing_at_frame_end_ = true;
    }
  }
}

void ImagePaintTimingDetector::NotifyImageFinished(
    const LayoutObject& object,
    const ImageResourceContent* cached_image) {
  RecordId record_id = std::make_pair(&object, cached_image);
  records_manager_.NotifyImageFinished(record_id);
}

ImageRecordsManager::ImageRecordsManager(LocalFrameView* frame_view)
    : size_ordered_set_(&LargeImageFirst), frame_view_(frame_view) {}

void ImageRecordsManager::OnImageLoaded(const RecordId& record_id,
                                        unsigned current_frame_index,
                                        const StyleFetchedImage* style_image) {
  base::WeakPtr<ImageRecord> record = FindVisibleRecord(record_id);
  DCHECK(record);
  if (!style_image) {
    record->load_time = image_finished_times_.at(record_id);
    DCHECK(!record->load_time.is_null());
  } else {
    Document* document = frame_view_->GetFrame().GetDocument();
    if (document && document->domWindow()) {
      record->load_time = ImageElementTiming::From(*document->domWindow())
                              .GetBackgroundImageLoadTime(style_image);
    }
  }
  OnImageLoadedInternal(record, current_frame_index);
}

void ImageRecordsManager::OnImageLoadedInternal(
    base::WeakPtr<ImageRecord>& record,
    unsigned current_frame_index) {
  SetLoaded(record);
  QueueToMeasurePaintTime(record, current_frame_index);
}

void ImageRecordsManager::RecordVisible(const RecordId& record_id,
                                        const uint64_t& visual_size) {
  std::unique_ptr<ImageRecord> record =
      CreateImageRecord(*record_id.first, record_id.second, visual_size);
  size_ordered_set_.insert(record->AsWeakPtr());
  visible_images_.insert(record_id, std::move(record));
}

std::unique_ptr<ImageRecord> ImageRecordsManager::CreateImageRecord(
    const LayoutObject& object,
    const ImageResourceContent* cached_image,
    const uint64_t& visual_size) {
  DCHECK_GT(visual_size, 0u);
  Node* node = object.GetNode();
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  std::unique_ptr<ImageRecord> record =
      std::make_unique<ImageRecord>(node_id, cached_image, visual_size);
  return record;
}

ImageRecord* ImageRecordsManager::FindLargestPaintCandidate() const {
  DCHECK_EQ(visible_images_.size(), size_ordered_set_.size());
  if (size_ordered_set_.size() == 0)
    return nullptr;
  return size_ordered_set_.begin()->get();
}

void ImagePaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_view_);
  visitor->Trace(callback_manager_);
}
}  // namespace blink
