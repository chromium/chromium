// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_tracker.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"

namespace blink {

// Set a big enough limit for the number of nodes to ensure memory usage is
// capped. Exceeding such limit will deactivate the algorithm.
constexpr size_t kImageNodeNumberLimit = 5000;

static bool LargeImageOnTop(const base::WeakPtr<ImageRecord>& a,
                            const base::WeakPtr<ImageRecord>& b) {
  // null value should be at the bottom of the priority queue, so:
  // * When a == null && b!=null, we treat b as larger, return true.
  // * When a != null && b==null, we treat b as no larger than a, return false.
  // * When a == null && b==null, we treat b as no larger than a, return false.
  return (a && b) ? (a->first_size < b->first_size) : bool(b);
}

static bool LateImageOnTop(const base::WeakPtr<ImageRecord>& a,
                           const base::WeakPtr<ImageRecord>& b) {
  // null value should be at the bottom of the priority queue, so:
  // * When a == null && b!=null, we treat b as larger, return true.
  // * When a != null && b==null, we treat b as no larger than a, return false.
  // * When a == null && b==null, we treat b as no larger than a, return false.
  return (a && b) ? (a->first_paint_index < b->first_paint_index) : bool(b);
}

ImagePaintTimingDetector::ImagePaintTimingDetector(LocalFrameView* frame_view)
    : largest_image_heap_(&LargeImageOnTop),
      latest_image_heap_(&LateImageOnTop),
      frame_view_(frame_view) {}

void ImagePaintTimingDetector::PopulateTraceValue(
    TracedValue& value,
    const ImageRecord& first_image_paint,
    unsigned candidate_index) const {
  value.SetInteger("DOMNodeId", first_image_paint.node_id);
  value.SetString("imageUrl", first_image_paint.image_url);
  value.SetInteger("size", first_image_paint.first_size);
  value.SetInteger("candidateIndex", candidate_index);
  value.SetString("frame",
                  IdentifiersFactory::FrameId(&frame_view_->GetFrame()));
}

IntRect ImagePaintTimingDetector::CalculateTransformedRect(
    LayoutRect& invalidated_rect,
    const PaintLayer& painting_layer) const {
  const auto* local_transform = painting_layer.GetLayoutObject()
                                    .FirstFragment()
                                    .LocalBorderBoxProperties()
                                    .Transform();
  const auto* ancestor_transform = painting_layer.GetLayoutObject()
                                       .View()
                                       ->FirstFragment()
                                       .LocalBorderBoxProperties()
                                       .Transform();
  FloatRect invalidated_rect_abs = FloatRect(invalidated_rect);
  if (invalidated_rect_abs.IsEmpty() || invalidated_rect_abs.IsZero())
    return IntRect();
  DCHECK(local_transform);
  DCHECK(ancestor_transform);
  GeometryMapper::SourceToDestinationRect(local_transform, ancestor_transform,
                                          invalidated_rect_abs);
  IntRect invalidated_rect_in_viewport = RoundedIntRect(invalidated_rect_abs);
  ScrollableArea* scrollable_area = frame_view_->GetScrollableArea();
  DCHECK(scrollable_area);
  IntRect viewport = scrollable_area->VisibleContentRect();
  invalidated_rect_in_viewport.Intersect(viewport);
  return invalidated_rect_in_viewport;
}

void ImagePaintTimingDetector::OnLargestImagePaintDetected(
    const ImageRecord& largest_image_record) {
  largest_image_paint_ = largest_image_record.first_paint_time_after_loaded;
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  PopulateTraceValue(*value, largest_image_record,
                     ++largest_image_candidate_index_max_);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "loading", "LargestImagePaint::Candidate", TRACE_EVENT_SCOPE_THREAD,
      largest_image_record.first_paint_time_after_loaded, "data",
      std::move(value));
  frame_view_->GetPaintTracker().DidChangePerformanceTiming();
}

void ImagePaintTimingDetector::OnLastImagePaintDetected(
    const ImageRecord& last_image_record) {
  last_image_paint_ = last_image_record.first_paint_time_after_loaded;
  std::unique_ptr<TracedValue> value = TracedValue::Create();
  PopulateTraceValue(*value, last_image_record,
                     ++last_image_candidate_index_max_);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP1(
      "loading", "LastImagePaint::Candidate", TRACE_EVENT_SCOPE_THREAD,
      last_image_record.first_paint_time_after_loaded, "data",
      std::move(value));
  frame_view_->GetPaintTracker().DidChangePerformanceTiming();
}

void ImagePaintTimingDetector::Analyze() {
  // These conditions represents the following scenarios:
  // 1. candiate being nullptr: no loaded image is found.
  // 2. candidate's first paint being null: largest/last image is still pending
  //   for timing. We discard the candidate and wait for the next analysis.
  // 3. new candidate equals to old candidate: we don't need to update the
  //   result unless it's a new candidate.
  ImageRecord* largest_image_record = FindLargestPaintCandidate();
  bool new_candidate_detected = false;
  if (largest_image_record &&
      !largest_image_record->first_paint_time_after_loaded.is_null() &&
      largest_image_record->first_paint_time_after_loaded !=
          largest_image_paint_) {
    new_candidate_detected = true;
    OnLargestImagePaintDetected(*largest_image_record);
  }
  ImageRecord* last_image_record = FindLastPaintCandidate();
  if (last_image_record &&
      !last_image_record->first_paint_time_after_loaded.is_null() &&
      last_image_record->first_paint_time_after_loaded != last_image_paint_) {
    new_candidate_detected = true;
    OnLastImagePaintDetected(*last_image_record);
  }
  if (new_candidate_detected)
    frame_view_->GetPaintTracker().DidChangePerformanceTiming();
}

void ImagePaintTimingDetector::OnPrePaintFinished() {
  frame_index_++;
  if (records_pending_timing_.size() <= 0)
    return;
  // If the last frame index of queue has changed, it means there are new
  // records pending timing.
  DOMNodeId node_id = records_pending_timing_.back();
  if (!id_record_map_.Contains(node_id))
    return;
  unsigned last_frame_index = id_record_map_.at(node_id)->frame_index;
  if (last_frame_index_queued_for_timing_ >= last_frame_index)
    return;

  last_frame_index_queued_for_timing_ = last_frame_index;
  // Records with frame index up to last_frame_index_queued_for_timing_ will be
  // queued for swap time.
  RegisterNotifySwapTime();
}

void ImagePaintTimingDetector::NotifyNodeRemoved(DOMNodeId node_id) {
  if (id_record_map_.Contains(node_id)) {
    // We assume that the removed node's id wouldn't be recycled, so we don't
    // bother to remove these records from largest_image_heap_ or
    // latest_image_heap_, to reduce computation.
    id_record_map_.erase(node_id);

    if (id_record_map_.size() == 0) {
      const bool largest_image_paint_invalidated =
          largest_image_paint_ != base::TimeTicks();
      const bool last_image_paint_invalidated =
          last_image_paint_ != base::TimeTicks();
      if (largest_image_paint_invalidated)
        largest_image_paint_ = base::TimeTicks();
      if (last_image_paint_invalidated)
        last_image_paint_ = base::TimeTicks();
      if (largest_image_paint_invalidated || last_image_paint_invalidated)
        frame_view_->GetPaintTracker().DidChangePerformanceTiming();
    }
  }
}

void ImagePaintTimingDetector::RegisterNotifySwapTime() {
  WebLayerTreeView::ReportTimeCallback callback =
      WTF::Bind(&ImagePaintTimingDetector::ReportSwapTime,
                WrapWeakPersistent(this), last_frame_index_queued_for_timing_);
  if (notify_swap_time_override_for_testing_) {
    // Run is not to run the |callback|, but to queue it.
    notify_swap_time_override_for_testing_.Run(std::move(callback));
    return;
  }
  // ReportSwapTime on layerTreeView will queue a swap-promise, the callback is
  // called when the swap for current render frame completes or fails to happen.
  LocalFrame& frame = frame_view_->GetFrame();
  if (!frame.GetPage())
    return;
  WebLayerTreeView* layerTreeView =
      frame.GetPage()->GetChromeClient().GetWebLayerTreeView(&frame);
  if (!layerTreeView)
    return;
  layerTreeView->NotifySwapTime(std::move(callback));
}

void ImagePaintTimingDetector::ReportSwapTime(
    unsigned max_frame_index_to_time,
    WebLayerTreeView::SwapResult result,
    base::TimeTicks timestamp) {
  // The callback is safe from race-condition only when running on main-thread.
  DCHECK(ThreadState::Current()->IsMainThread());
  // This callback is queued only when there are records in
  // records_pending_timing_.
  DCHECK_GT(records_pending_timing_.size(), 0UL);
  while (records_pending_timing_.size() > 0) {
    DOMNodeId node_id = records_pending_timing_.front();
    if (!id_record_map_.Contains(node_id)) {
      records_pending_timing_.pop();
      continue;
    }
    ImageRecord* record = id_record_map_.at(node_id);
    if (record->frame_index > max_frame_index_to_time)
      break;
    record->first_paint_time_after_loaded = timestamp;
    records_pending_timing_.pop();
  }

  Analyze();
}

void ImagePaintTimingDetector::RecordImage(const LayoutObject& object,
                                           const PaintLayer& painting_layer) {
  Node* node = object.GetNode();
  if (!node)
    return;
  DOMNodeId node_id = DOMNodeIds::IdForNode(node);
  if (size_zero_ids_.Contains(node_id))
    return;

  const LayoutImage* image = ToLayoutImage(&object);
  if (!id_record_map_.Contains(node_id)) {
    recorded_node_count_++;
    if (recorded_node_count_ < kImageNodeNumberLimit) {
      LayoutRect invalidated_rect = image->FirstFragment().VisualRect();
      // Do not record first size until invalidated_rect's size becomes
      // non-empty.
      if (invalidated_rect.IsEmpty())
        return;
      IntRect invalidated_rect_in_viewport =
          CalculateTransformedRect(invalidated_rect, painting_layer);
      int rect_size = invalidated_rect_in_viewport.Height() *
                      invalidated_rect_in_viewport.Width();
      if (rect_size == 0) {
        // When rect_size == 0, it either means the image is size 0 or the image
        // is out of viewport. Either way, we don't track this image anymore, to
        // reduce computation.
        size_zero_ids_.insert(node_id);
        return;
      }
      std::unique_ptr<ImageRecord> record = std::make_unique<ImageRecord>();
      record->node_id = node_id;
      record->frame_index = frame_index_;
      record->first_size = rect_size;
      record->first_paint_index = ++first_paint_index_max_;
      ImageResourceContent* cachedImg = image->CachedImage();
      record->image_url =
          !cachedImg ? "" : cachedImg->Url().StrippedForUseAsReferrer();
      largest_image_heap_.push(record->AsWeakPtr());
      latest_image_heap_.push(record->AsWeakPtr());
      id_record_map_.insert(node_id, std::move(record));
    } else {
      // for assessing whether kImageNodeNumberLimit is large enough for all
      // normal cases
      TRACE_EVENT_INSTANT1("loading", "ImagePaintTimingDetector::OverNodeLimit",
                           TRACE_EVENT_SCOPE_THREAD, "recorded_node_count",
                           recorded_node_count_);
    }
  }

  if (id_record_map_.Contains(node_id) &&
      IsJustLoaded(image, *id_record_map_.at(node_id))) {
    records_pending_timing_.push(node_id);
    id_record_map_.at(node_id)->loaded = true;
  }
}

bool ImagePaintTimingDetector::IsJustLoaded(const LayoutImage* image,
                                            const ImageRecord& record) const {
  ImageResourceContent* cachedImg = image->CachedImage();
  return cachedImg && cachedImg->IsLoaded() && !record.loaded;
}

ImageRecord* ImagePaintTimingDetector::FindLargestPaintCandidate() {
  while (!largest_image_heap_.empty() && !largest_image_heap_.top()) {
    // Discard the elements that have been removed from |id_record_map_|.
    largest_image_heap_.pop();
  }
  // We report the result as the first paint after the largest image finishes
  // loading. If the largest image is still loading, we report nothing and come
  // back later to see if the largest image by then has finished loading.
  if (!largest_image_heap_.empty() && largest_image_heap_.top()->loaded) {
    DCHECK(id_record_map_.Contains(largest_image_heap_.top()->node_id));
    return largest_image_heap_.top().get();
  }
  return nullptr;
}

ImageRecord* ImagePaintTimingDetector::FindLastPaintCandidate() {
  while (!latest_image_heap_.empty() && !latest_image_heap_.top()) {
    // Discard the elements that have been removed from |id_record_map_|.
    latest_image_heap_.pop();
  }
  // We report the result as the first paint after the latest image finishes
  // loading. If the latest image is still loading, we report nothing and come
  // back later to see if the latest image at that time has finished loading.
  if (!latest_image_heap_.empty() && latest_image_heap_.top()->loaded) {
    DCHECK(id_record_map_.Contains(latest_image_heap_.top()->node_id));
    return latest_image_heap_.top().get();
  }
  return nullptr;
}

void ImagePaintTimingDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_view_);
}
}  // namespace blink
