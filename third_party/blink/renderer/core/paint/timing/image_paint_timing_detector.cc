// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"

#include "base/feature_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
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
// * |visual_size| refers to the size of the |displayed_image_size| after
// clipping and transforming. The size is in the main-frame's coordinate.
// * |intrinsic_image_size| refers to the the image object's original size
// before scaling. The size is in the image object's coordinate.
// * |displayed_image_size| refers to the paint size in the image object's
// coordinate.
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

void RecordPotentialSoftNavigationPaint(LocalFrameView* frame_view,
                                        gfx::RectF rect,
                                        Node* node) {
  LocalFrame& frame = frame_view->GetFrame();
  if (LocalDOMWindow* window = frame.DomWindow()) {
    if (SoftNavigationHeuristics* heuristics =
            SoftNavigationHeuristics::From(*window)) {
      heuristics->RecordPaint(&frame, rect.size().GetArea(),
                              node->IsModifiedBySoftNavigation());
    }
  }
}

}  // namespace

double ImageRecord::EntropyForLCP() const {
  if (recorded_size == 0 || !media_timing)
    return 0.0;
  return media_timing->ContentSizeForEntropy() * 8.0 / recorded_size;
}

std::optional<WebURLRequest::Priority> ImageRecord::RequestPriority() const {
  if (!media_timing)
    return std::nullopt;
  return media_timing->RequestPriority();
}

void ImageRecord::Trace(Visitor* visitor) const {
  visitor->Trace(media_timing);
}

ImagePaintTimingDetector::ImagePaintTimingDetector(
    LocalFrameView* frame_view,
    PaintTimingCallbackManager* callback_manager)
    : uses_page_viewport_(
          base::FeatureList::IsEnabled(features::kUsePageViewportInLCP)),
      records_manager_(frame_view),
      frame_view_(frame_view),
      callback_manager_(callback_manager) {}

ImageRecord* ImageRecordsManager::LargestImage() const {
  if (!largest_painted_image_ ||
      (largest_pending_image_ && (largest_painted_image_->recorded_size <
                                  largest_pending_image_->recorded_size))) {
    return largest_pending_image_.Get();
  }
  return largest_painted_image_.Get();
}

void ImagePaintTimingDetector::PopulateTraceValue(
    TracedValue& value,
    const ImageRecord& first_image_paint) {
  value.SetInteger("DOMNodeId", static_cast<int>(first_image_paint.node_id));
  // The media_timing could have been deleted when this is called.
  value.SetString("imageUrl",
                  first_image_paint.media_timing
                      ? String(first_image_paint.media_timing->Url())
                      : "(deleted)");
  value.SetInteger("size", static_cast<int>(first_image_paint.recorded_size));
  value.SetInteger("candidateIndex", ++count_candidates_);
  value.SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value.SetBoolean("isOutermostMainFrame",
                   frame_view_->GetFrame().IsOutermostMainFrame());
  value.SetBoolean("isEmbeddedFrame",
                   !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame() ||
                       frame_view_->GetFrame().IsInFencedFrameTree());
  if (first_image_paint.lcp_rect_info_) {
    first_image_paint.lcp_rect_info_->OutputToTraceValue(value);
  }
}

void ImagePaintTimingDetector::ReportCandidateToTrace(
    ImageRecord& largest_image_record,
    base::TimeTicks time) {
  if (!PaintTimingDetector::IsTracing())
    return;
  DCHECK(!time.is_null());
  auto value = std::make_unique<TracedValue>();
  PopulateTraceValue(*value, largest_image_record);
  // TODO(yoav): Report first animated frame times as well.
  TRACE_EVENT_MARK_WITH_TIMESTAMP2(
      "loading", "LargestImagePaint::Candidate", time, "data", std::move(value),
      "frame", GetFrameIdForTracing(&frame_view_->GetFrame()));
}

void ImagePaintTimingDetector::ReportNoCandidateToTrace() {
  if (!PaintTimingDetector::IsTracing())
    return;
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("candidateIndex", ++count_candidates_);
  value->SetBoolean("isMainFrame", frame_view_->GetFrame().IsMainFrame());
  value->SetBoolean("isOutermostMainFrame",
                    frame_view_->GetFrame().IsOutermostMainFrame());
  value->SetBoolean("isEmbeddedFrame",
                    !frame_view_->GetFrame().LocalFrameRoot().IsMainFrame() ||
                        frame_view_->GetFrame().IsInFencedFrameTree());
  TRACE_EVENT2("loading", "LargestImagePaint::NoCandidate", "data",
               std::move(value), "frame",
               GetFrameIdForTracing(&frame_view_->GetFrame()));
}

std::pair<ImageRecord*, bool>
ImagePaintTimingDetector::UpdateMetricsCandidate() {
  ImageRecord* largest_image_record = records_manager_.LargestImage();
  base::TimeTicks time = largest_image_record ? largest_image_record->paint_time
                                              : base::TimeTicks();
  bool animated_first_frame_ready =
      largest_image_record &&
      !largest_image_record->first_animated_frame_time.is_null();
  if (animated_first_frame_ready) {
    time = largest_image_record->first_animated_frame_time;
  }

  const uint64_t size =
      largest_image_record ? largest_image_record->recorded_size : 0;

  double bpp =
      largest_image_record ? largest_image_record->EntropyForLCP() : 0.0;

  std::optional<WebURLRequest::Priority> priority =
      largest_image_record ? largest_image_record->RequestPriority()
                           : std::nullopt;

  PaintTimingDetector& detector = frame_view_->GetPaintTimingDetector();
  // Calling NotifyMetricsIfLargestImagePaintChanged only has an impact on
  // PageLoadMetrics, and not on the web exposed metrics.
  //
  // Two different candidates are rare to have the same time and size.
  // So when they are unchanged, the candidate is considered unchanged.
  bool changed =
      detector.GetLargestContentfulPaintCalculator()
          ->NotifyMetricsIfLargestImagePaintChanged(
              time, size, largest_image_record, bpp, std::move(priority));
  if (changed) {
    if (!time.is_null() && largest_image_record->loaded) {
      ReportCandidateToTrace(*largest_image_record, time);
    } else {
      ReportNoCandidateToTrace();
    }
  }
  return {largest_image_record, changed};
}

void ImagePaintTimingDetector::OnPaintFinished() {
  viewport_size_ = std::nullopt;
  if (!added_entry_in_latest_frame_)
    return;

  added_entry_in_latest_frame_ = false;

  last_registered_frame_index_ = frame_index_++;
  RegisterNotifyPresentationTime();
}

void ImagePaintTimingDetector::NotifyImageRemoved(
    const LayoutObject& object,
    const MediaTiming* media_timing) {
  records_manager_.RemoveRecord(
      MediaRecordId::GenerateHash(&object, media_timing));
}

void ImagePaintTimingDetector::StopRecordEntries() {
  // Clear the records queued for presentation callback to ensure no new updates
  // occur.
  records_manager_.ClearImagesQueuedForPaintTime();
  if (frame_view_->GetFrame().IsOutermostMainFrame()) {
    DCHECK(frame_view_->GetFrame().GetDocument());
    ukm::builders::Blink_PaintTiming(
        frame_view_->GetFrame().GetDocument()->UkmSourceID())
        .SetLCPDebugging_HasViewportImage(contains_full_viewport_image_)
        .Record(ukm::UkmRecorder::Get());
  }
}

void ImagePaintTimingDetector::RegisterNotifyPresentationTime() {
  auto callback =
      WTF::BindOnce(&ImagePaintTimingDetector::ReportPresentationTime,
                    WrapWeakPersistent(this), last_registered_frame_index_);
  callback_manager_->RegisterCallback(std::move(callback));
}

void ImagePaintTimingDetector::ReportPresentationTime(
    unsigned last_queued_frame_index,
    base::TimeTicks timestamp) {
  // The callback is safe from race-condition only when running on main-thread.
  DCHECK(ThreadState::Current()->IsMainThread());
  records_manager_.AssignPaintTimeToRegisteredQueuedRecords(
      timestamp, last_queued_frame_index);
}

void ImageRecordsManager::AssignPaintTimeToRegisteredQueuedRecords(
    const base::TimeTicks& timestamp,
    unsigned last_queued_frame_index) {
  while (!images_queued_for_paint_time_.empty()) {
    ImageRecord* record = images_queued_for_paint_time_.front();
    if (!record) {
      images_queued_for_paint_time_.pop_front();
      continue;
    }
    if (record->frame_index > last_queued_frame_index) {
      break;
    }
    if (record->queue_animated_paint) {
      record->first_animated_frame_time = timestamp;
      record->queue_animated_paint = false;
    }
    auto it = pending_images_.find(record->hash);
    images_queued_for_paint_time_.pop_front();
    // A record may be in |images_queued_for_paint_time_| twice, for instance if
    // is already loaded by the time of its first paint.
    if (!record->loaded || !record->paint_time.is_null() ||
        it == pending_images_.end()) {
      continue;
    }
    record->paint_time = timestamp;
    if (!largest_painted_image_ ||
        largest_painted_image_->recorded_size < record->recorded_size) {
      largest_painted_image_ = std::move(it->value);
    }
    pending_images_.erase(it);
  }
}

bool ImagePaintTimingDetector::RecordImage(
    const LayoutObject& object,
    const gfx::Size& intrinsic_size,
    const MediaTiming& media_timing,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const StyleImage* style_image,
    const gfx::Rect& image_border) {
  Node* node = object.GetNode();

  if (!node)
    return false;

  // Before the image resource starts loading, <img> has no size info. We wait
  // until the size is known.
  if (image_border.IsEmpty())
    return false;

  if (media_timing.IsBroken()) {
    return false;
  }

  MediaRecordId record_id(&object, &media_timing);
  MediaRecordIdHash record_id_hash = record_id.GetHash();

  if (int depth = IgnorePaintTimingScope::IgnoreDepth()) {
    // Record the largest loaded image that is hidden due to documentElement
    // being invisible but by no other reason (i.e. IgnoreDepth() needs to be
    // 1).
    if (depth == 1 && IgnorePaintTimingScope::IsDocumentElementInvisible() &&
        media_timing.IsSufficientContentLoadedForPaint()) {
      gfx::RectF mapped_visual_rect =
          frame_view_->GetPaintTimingDetector().CalculateVisualRect(
              image_border, current_paint_chunk_properties);
      uint64_t rect_size = ComputeImageRectSize(
          image_border, mapped_visual_rect, intrinsic_size,
          current_paint_chunk_properties, object, media_timing);
      records_manager_.MaybeUpdateLargestIgnoredImage(
          record_id, rect_size, image_border, mapped_visual_rect);
    }
    return false;
  }

  if (records_manager_.IsRecordedImage(record_id_hash)) {
    ImageRecord* record = records_manager_.GetPendingImage(record_id_hash);
    if (!record)
      return false;
    if (media_timing.IsPaintedFirstFrame() &&
        RuntimeEnabledFeatures::LCPAnimatedImagesWebExposedEnabled()) {
      added_entry_in_latest_frame_ |=
          records_manager_.OnFirstAnimatedFramePainted(record_id_hash,
                                                       frame_index_);
    }
    if (!record->loaded && media_timing.IsSufficientContentLoadedForPaint()) {
      records_manager_.OnImageLoaded(record_id_hash, frame_index_, style_image);
      added_entry_in_latest_frame_ = true;
      if (std::optional<PaintTimingVisualizer>& visualizer =
              frame_view_->GetPaintTimingDetector().Visualizer()) {
        gfx::RectF mapped_visual_rect =
            frame_view_->GetPaintTimingDetector().CalculateVisualRect(
                image_border, current_paint_chunk_properties);
        visualizer->DumpImageDebuggingRect(
            object, mapped_visual_rect,
            media_timing.IsSufficientContentLoadedForPaint(),
            media_timing.Url());
      }
      return true;
    }
    return false;
  }

  gfx::RectF mapped_visual_rect =
      frame_view_->GetPaintTimingDetector().CalculateVisualRect(
          image_border, current_paint_chunk_properties);
  uint64_t rect_size = ComputeImageRectSize(
      image_border, mapped_visual_rect, intrinsic_size,
      current_paint_chunk_properties, object, media_timing);

  RecordPotentialSoftNavigationPaint(frame_view_, mapped_visual_rect, node);

  double bpp = (rect_size > 0)
                   ? media_timing.ContentSizeForEntropy() * 8.0 / rect_size
                   : 0.0;

  bool added_pending = records_manager_.RecordFirstPaintAndReturnIsPending(
      record_id, rect_size, image_border, mapped_visual_rect, bpp);
  if (!added_pending)
    return false;

  if (media_timing.IsPaintedFirstFrame() &&
      RuntimeEnabledFeatures::LCPAnimatedImagesWebExposedEnabled()) {
    added_entry_in_latest_frame_ |=
        records_manager_.OnFirstAnimatedFramePainted(record_id_hash,
                                                     frame_index_);
  }
  if (media_timing.IsSufficientContentLoadedForPaint()) {
    records_manager_.OnImageLoaded(record_id_hash, frame_index_, style_image);
    added_entry_in_latest_frame_ = true;
    return true;
  }
  return false;
}

uint64_t ImagePaintTimingDetector::ComputeImageRectSize(
    const gfx::Rect& image_border,
    const gfx::RectF& mapped_visual_rect,
    const gfx::Size& intrinsic_size,
    const PropertyTreeStateOrAlias& current_paint_chunk_properties,
    const LayoutObject& object,
    const MediaTiming& media_timing) {
  if (std::optional<PaintTimingVisualizer>& visualizer =
          frame_view_->GetPaintTimingDetector().Visualizer()) {
    visualizer->DumpImageDebuggingRect(
        object, mapped_visual_rect,
        media_timing.IsSufficientContentLoadedForPaint(), media_timing.Url());
  }
  uint64_t rect_size = mapped_visual_rect.size().GetArea();
  // Transform visual rect to window before calling downscale.
  gfx::RectF float_visual_rect =
      frame_view_->GetPaintTimingDetector().BlinkSpaceToDIPs(
          gfx::RectF(image_border));
  if (!viewport_size_.has_value()) {
    // If the flag to use page viewport is enabled, we use the page viewport
    // (aka the main frame viewport) for all frames, including iframes. This
    // prevents us from discarding images with size equal to the size of its
    // embedding iframe.
    gfx::Rect viewport_int_rect =
        uses_page_viewport_
            ? frame_view_->GetPage()->GetVisualViewport().VisibleContentRect()
            : frame_view_->GetScrollableArea()->VisibleContentRect();
    gfx::RectF viewport =
        frame_view_->GetPaintTimingDetector().BlinkSpaceToDIPs(
            gfx::RectF(viewport_int_rect));
    viewport_size_ = viewport.size().GetArea();
  }
  // An SVG image size is computed with respect to the virtual viewport of the
  // SVG, so |rect_size| can be larger than |*viewport_size| in edge cases. If
  // the rect occupies the whole viewport, disregard this candidate by saying
  // the size is 0.
  if (rect_size >= *viewport_size_) {
    contains_full_viewport_image_ = true;
    return 0;
  }

  rect_size = DownScaleIfIntrinsicSizeIsSmaller(
      rect_size, intrinsic_size.Area64(), float_visual_rect.size().GetArea());
  return rect_size;
}

void ImagePaintTimingDetector::NotifyImageFinished(
    const LayoutObject& object,
    const MediaTiming* media_timing) {
  records_manager_.NotifyImageFinished(
      MediaRecordId::GenerateHash(&object, media_timing));
}

void ImagePaintTimingDetector::ReportLargestIgnoredImage() {
  added_entry_in_latest_frame_ = true;
  records_manager_.ReportLargestIgnoredImage(frame_index_);
}

ImageRecordsManager::ImageRecordsManager(LocalFrameView* frame_view)
    : frame_view_(frame_view) {}

bool ImageRecordsManager::OnFirstAnimatedFramePainted(
    MediaRecordIdHash record_id_hash,
    unsigned current_frame_index) {
  ImageRecord* record = GetPendingImage(record_id_hash);
  DCHECK(record);
  if (record->media_timing &&
      !record->media_timing->GetFirstVideoFrameTime().is_null()) {
    // If this is a video record, then we can get the first frame time from the
    // MediaTiming object, and can use that to set the first frame time in the
    // ImageRecord object.
    record->first_animated_frame_time =
        record->media_timing->GetFirstVideoFrameTime();
  } else if (record->first_animated_frame_time.is_null()) {
    // Otherwise, this is an animated images, and so we should wait for the
    // presentation callback to fire to set the first frame presentation time.
    record->queue_animated_paint = true;
    QueueToMeasurePaintTime(record, current_frame_index);
    return true;
  }
  return false;
}

void ImageRecordsManager::OnImageLoaded(MediaRecordIdHash record_id_hash,
                                        unsigned current_frame_index,
                                        const StyleImage* style_image) {
  ImageRecord* record = GetPendingImage(record_id_hash);
  DCHECK(record);
  if (!style_image) {
    auto it = image_finished_times_.find(record_id_hash);
    if (it != image_finished_times_.end()) {
      record->load_time = it->value;
      DCHECK(!record->load_time.is_null());
    }
  } else {
    Document* document = frame_view_->GetFrame().GetDocument();
    if (document && document->domWindow()) {
      record->load_time = ImageElementTiming::From(*document->domWindow())
                              .GetBackgroundImageLoadTime(style_image);
      record->origin_clean = style_image->IsOriginClean();
    }
  }
  OnImageLoadedInternal(record, current_frame_index);
}

void ImageRecordsManager::ReportLargestIgnoredImage(
    unsigned current_frame_index) {
  if (!largest_ignored_image_)
    return;
  Node* node = DOMNodeIds::NodeForId(largest_ignored_image_->node_id);
  if (!node || !node->GetLayoutObject() ||
      !largest_ignored_image_->media_timing) {
    // The image has been removed, so we have no content to report.
    largest_ignored_image_ = nullptr;
    return;
  }

  // Trigger FCP if it's not already set.
  Document* document = frame_view_->GetFrame().GetDocument();
  DCHECK(document);
  PaintTiming::From(*document).MarkFirstContentfulPaint();

  ImageRecord* record = largest_ignored_image_.Get();
  CHECK(record);
  recorded_images_.insert(record->hash);
  AddPendingImage(record);
  OnImageLoadedInternal(record, current_frame_index);
}

void ImageRecordsManager::OnImageLoadedInternal(ImageRecord* record,
                                                unsigned current_frame_index) {
  SetLoaded(record);
  QueueToMeasurePaintTime(record, current_frame_index);
}

void ImageRecordsManager::MaybeUpdateLargestIgnoredImage(
    const MediaRecordId& record_id,
    const uint64_t& visual_size,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect) {
  if (visual_size && (!largest_ignored_image_ ||
                      visual_size > largest_ignored_image_->recorded_size)) {
    largest_ignored_image_ = CreateImageRecord(
        *record_id.GetLayoutObject(), record_id.GetMediaTiming(), visual_size,
        frame_visual_rect, root_visual_rect, record_id.GetHash());
    largest_ignored_image_->load_time = base::TimeTicks::Now();
  }
}

bool ImageRecordsManager::RecordFirstPaintAndReturnIsPending(
    const MediaRecordId& record_id,
    const uint64_t& visual_size,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect,
    double bpp) {
  // Don't process the image yet if it is invisible, as it may later become
  // visible, and potentially eligible to be an LCP candidate.
  if (visual_size == 0u) {
    return false;
  }
  recorded_images_.insert(record_id.GetHash());
  // If this cannot become an LCP candidate, no need to do anything else.
  if (visual_size == 0u ||
      (largest_painted_image_ &&
       largest_painted_image_->recorded_size > visual_size)) {
    return false;
  }
  if (base::FeatureList::IsEnabled(features::kExcludeLowEntropyImagesFromLCP) &&
      bpp < features::kMinimumEntropyForLCP.Get()) {
    return false;
  }

  ImageRecord* record = CreateImageRecord(
      *record_id.GetLayoutObject(), record_id.GetMediaTiming(), visual_size,
      frame_visual_rect, root_visual_rect, record_id.GetHash());
  AddPendingImage(record);
  return true;
}
void ImageRecordsManager::AddPendingImage(ImageRecord* record) {
  if (!largest_pending_image_ ||
      (largest_pending_image_->recorded_size < record->recorded_size)) {
    largest_pending_image_ = record;
  }
  pending_images_.insert(record->hash, record);
}

ImageRecord* ImageRecordsManager::CreateImageRecord(
    const LayoutObject& object,
    const MediaTiming* media_timing,
    const uint64_t& visual_size,
    const gfx::Rect& frame_visual_rect,
    const gfx::RectF& root_visual_rect,
    MediaRecordIdHash hash) {
  DCHECK_GT(visual_size, 0u);
  Node* node = object.GetNode();
  DOMNodeId node_id = node->GetDomNodeId();
  return MakeGarbageCollected<ImageRecord>(node_id, media_timing, visual_size,
                                           frame_visual_rect, root_visual_rect,
                                           hash);
}

void ImageRecordsManager::ClearImagesQueuedForPaintTime() {
  images_queued_for_paint_time_.clear();
}

void ImageRecordsManager::Clear() {
  largest_painted_image_ = nullptr;
  largest_pending_image_ = nullptr;
  images_queued_for_paint_time_.clear();
  recorded_images_.clear();
  pending_images_.clear();
  image_finished_times_.clear();
  largest_ignored_image_ = nullptr;
}

void ImageRecordsManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_view_);
  visitor->Trace(largest_painted_image_);
  visitor->Trace(largest_pending_image_);
  visitor->Trace(pending_images_);
  visitor->Trace(images_queued_for_paint_time_);
  visitor->Trace(largest_ignored_image_);
}

void ImagePaintTimingDetector::Trace(Visitor* visitor) const {
  visitor->Trace(records_manager_);
  visitor->Trace(frame_view_);
  visitor->Trace(callback_manager_);
}
}  // namespace blink
