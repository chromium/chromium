// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"

#include <cstddef>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "cc/layers/heads_up_display_layer.h"
#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_image_resource.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_image.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/effective_visual_size_result.h"
#include "third_party/blink/renderer/core/paint/timing/image_element_timing.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/style/style_fetched_image.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/widget/frame_widget.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

ImagePaintTimingDetector::ImagePaintTimingDetector(
    PaintTimingDetector* detector)
    : paint_timing_detector_(detector) {}

void ImagePaintTimingDetector::SendRectsToHud() {
  LocalFrameView* frame_view =
      paint_timing_detector_->GetPaintTiming().GetDocument()->View();
  CHECK(frame_view);
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

  bool is_recording_lcp = !!GetLargestContentfulPaintManager();

  for (const auto& record : images_queued_for_paint_time_) {
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

OptionalPaintTimingDetectorCallback<ImageRecord>
ImagePaintTimingDetector::TakePaintTimingCallback() {
  viewport_size_ = std::nullopt;
  if (!added_entry_in_latest_frame_)
    return std::nullopt;

  // Do this before incrementing frame_index_;
  SendRectsToHud();

  added_entry_in_latest_frame_ = false;
  return BindOnce(
      [](ImagePaintTimingDetector* self, uint32_t frame_index,
         const base::TimeTicks& presentation_timestamp,
         const DOMPaintTimingInfo& paint_timing_info,
         HeapVector<Member<ImageRecord>>& settled_records) {
        if (self) {
          self->AssignPaintTimeToRegisteredQueuedRecords(
              frame_index, presentation_timestamp, paint_timing_info,
              settled_records);
        }
      },
      WrapWeakPersistent(this), frame_index_++);
}

void ImagePaintTimingDetector::NotifyImageRemoved(
    const LayoutObject& object,
    const MediaTiming* media_timing) {
  ImageRecord* record =
      RemoveRecord(MediaRecordId::GenerateHash(&object, media_timing));
  if (auto* manager = GetLargestContentfulPaintManager()) {
    // Notify `manager` even if record is null so it can update the largest
    // ignored image, if needed.
    //
    // TODO(crbug.com/449779010): When soft navs supports largest pending image,
    // this will need to be updated to notify the relevant soft nav context.
    manager->OnImageRemoved(record, object, media_timing);
  }
}

void ImagePaintTimingDetector::StopRecordEntries() {
  // Clear the records queued for presentation callback to ensure no new updates
  // occur.
  images_queued_for_paint_time_.clear();
}

void ImagePaintTimingDetector::AssignPaintTimeToRegisteredQueuedRecords(
    uint32_t last_queued_frame_index,
    const base::TimeTicks& presentation_timestamp,
    const DOMPaintTimingInfo& paint_timing_info,
    HeapVector<Member<ImageRecord>>& settled_records) {
  while (!images_queued_for_paint_time_.empty()) {
    ImageRecord* record = images_queued_for_paint_time_.front();
    // Not ready for this frame yet - we're done with the queue for now.
    if (record->FrameIndex() > last_queued_frame_index) {
      break;
    }

    images_queued_for_paint_time_.pop_front();

    if (record->IsFirstAnimatedFramePaintTimingQueued()) {
      record->SetFirstAnimatedFrameTime(presentation_timestamp);
      record->SetIsFirstAnimatedFramePaintTimingQueued(false);
    }

    // TODO(crbug.com/364860066): When cleaning up the flag, remove this whole
    // block. This re-enables the old behavior where animated images were not
    // reported until fully loaded.
    if (!record->IsLoaded() &&
        !RuntimeEnabledFeatures::ReportFirstFrameTimeAsRenderTimeEnabled()) {
      continue;
    }

    // A record may be in `images_queued_for_paint_time_` twice if it's already
    // loaded by the time of its first contentful paint. It will also be removed
    // from that collection if the image was removed between painting it and
    // running this callback, in which case we still want to set its paint time.
    auto it = pending_images_.find(record->Hash());
    if (it == pending_images_.end() && !record->WasNodeRemoved()) {
      continue;
    }

    // Set paint time if it hasn't been set. Note for first video frame with
    // ReportFirstFrameTimeAsRenderTime enabled, this will already be set.
    if (!record->HasPaintTime()) {
      record->SetPaintTime(presentation_timestamp, paint_timing_info);
    }

    settled_records.push_back(record);

    // Remove from pending.
    if (it != pending_images_.end()) {
      pending_images_.erase(it);
    }
  }
}

void ImagePaintTimingDetector::NotifyInteractionTriggeredVideoSrcChange(
    const LayoutObject& object) {
  // The `MediaTiming` parameter ignored when computing the hash for video
  // elements, so pass nullptr here. It's ignored because of an issue where
  // multiple LCP candidates are created for videos with a poster image, which
  // is why we need to remove the record here so the subsequent first frame is
  // attributed to the relevant interaction. See also crbug.com/330202431.
  MediaRecordId record_id(&object, /*media=*/nullptr);
  MediaRecordIdHash record_id_hash = record_id.GetHash();
  if (recorded_images_.Contains(record_id_hash)) {
    RemoveRecord(record_id_hash);
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
  if (!node) {
    return false;
  }

  // Before the image resource starts loading, <img> has no size info. We wait
  // until the size is known.
  if (image_border.IsEmpty()) {
    return false;
  }

  if (media_timing.IsBroken()) {
    return false;
  }

  gfx::RectF mapped_visual_rect = paint_timing_detector_->CalculateVisualRect(
      image_border, current_paint_chunk_properties);

  if (PaintTimingVisualizer* visualizer =
          paint_timing_detector_->Visualizer()) {
    visualizer->DumpImageDebuggingRect(
        object, mapped_visual_rect,
        media_timing.IsSufficientContentLoadedForPaint(), media_timing.Url());
  }

  MediaRecordId record_id(&object, &media_timing);
  MediaRecordIdHash record_id_hash = record_id.GetHash();

  // `record` will be non-null if the first paint for the image was recorded but
  // the image hasn't been "finalized" yet, i.e. the image wasn't sufficiently
  // loaded the last time it was painted or the presentation time callback for
  // the first paint after being sufficiently loaded is still pending.
  ImageRecord* record = GetPendingImage(record_id_hash);

  // If the image was already processed and has either finished loading or
  // wasn't previously needed, there's nothing to do.
  if (!record && recorded_images_.Contains(record_id_hash)) {
    return false;
  }

  int ignore_paint_depth = IgnorePaintTimingScope::IgnoreDepth();

  // Create a new new `ImageRecord` and initialize paint tracking if:
  //   1. the image was not yet recorded (`record` is null). Note: this includes
  //   the cases where the `MediaTiming` changed (different image) or the layout
  //   object changed, e.g. toggling display:none. The first case is specced in
  //   Paint Timing, but the second is a spec violation --- but ICP also depends
  //   on this behavior (see crbub.com/507049713).
  //
  //   2. there is a pending image `record` and paints are being ignored
  //   (`ignore_paint_depth` > 0). This is rare, but can happen if the opacity
  //   changed after initially painting the image and the image hasn't finished
  //   loading. If the pending `record` was painted and we're only waiting for
  //   presentation feedback, that can proceed and the largest ignored image
  //   will end up being ignored.
  if (!record || ignore_paint_depth > 0) {
    // Compute the effective visual size for LCP and ICP.
    EffectiveVisualSizeResult effective_visual_size_result =
        LargestContentfulPaintCalculator::ComputeEffectiveVisualSize(
            object, media_timing, image_border, mapped_visual_rect,
            intrinsic_size, ViewportSize(), *paint_timing_detector_);

    // Don't process the image yet if it is invisible, as it may later become
    // visible, potentially making it eligible to be an LCP candidate.
    //
    // TODO(crbug.com/503691215): This is incompatible with how ElementTiming is
    // currently implemented. It also appears to violate the PaintTiming spec
    // since "mark paint timing" does not check viewport intersection.
    if (effective_visual_size_result.size == 0u) {
      return false;
    }

    record = MakeGarbageCollected<ImageRecord>(
        node, &media_timing, image_border, mapped_visual_rect,
        record_id.GetHash(), effective_visual_size_result);

    if (ignore_paint_depth > 0) {
      if (auto* manager = GetLargestContentfulPaintManager()) {
        manager->MaybeUpdateLargestIgnoredImage(record);
      }
      return false;
    }

    if (auto* manager = GetLargestContentfulPaintManager()) {
      manager->InitializePaintTracking(record);
    }

    LocalDOMWindow* window = object.GetDocument().domWindow();
    CHECK(window);
    if (SoftNavigationHeuristics* heuristics =
            window->GetSoftNavigationHeuristics()) {
      heuristics->InitializePaintTracking(record);
    }

    // Mark the image as recorded regardless of if this is needed for any
    // PaintTiming clients so the image isn't reconsidered as a candidate.
    recorded_images_.insert(record_id_hash);

    if (!record->IsNeededForInteractionContentfulPaint() &&
        !record->IsNeededForLargestContentfulPaint()) {
      return false;
    }
    // Finally, we have at least one client that wants to track paints for this
    // image, so set up future tracking.
    pending_images_.insert(record->Hash(), record);
  }

  CHECK(record);

  // If this frame is the first painted frame for animated content, mark it and
  // call `QueueToMeasurePaintTime` (eventually) to measure it.
  // This mechanism works a bit differently for images and video.
  // The stored value may or may not be exposed as the `renderTime` depending on
  // flags.
  if (media_timing.IsPaintedFirstFrame()) {
    OnFirstAnimatedFramePainted(record_id_hash);
  }

  // TODO(crbug.com/372929290): This next check will pass when <video> content
  // has loaded just the first frame of video.  This is likely unexpected, and
  // should likely have been handled in the if block for `IsPaintedFirstFrame`,
  // above.
  if (!record->IsLoaded() && media_timing.IsSufficientContentLoadedForPaint()) {
    OnImageLoaded(record, style_image);
    CHECK(added_entry_in_latest_frame_);

    if (SoftNavigationContext* context = record->GetSoftNavigationContext()) {
      context->AddPaintedArea(record);
    }
    return true;
  }
  return false;
}

void ImagePaintTimingDetector::NotifyImageFinished(
    const LayoutObject& object,
    const MediaTiming* media_timing) {
  auto hash(MediaRecordId::GenerateHash(&object, media_timing));
  // TODO(npm): Ideally NotifyImageFinished() would only be called when the
  // record has not yet been inserted in |image_finished_times_| but that's not
  // currently the case. If we plumb some information from MediaTiming we may be
  // able to ensure that this call does not require the Contains() check, which
  // would save time.
  if (!image_finished_times_.Contains(hash)) {
    image_finished_times_.insert(hash, base::TimeTicks::Now());
  }
}

void ImagePaintTimingDetector::ReportLargestIgnoredImage() {
  auto* lcp_manager = GetLargestContentfulPaintManager();
  if (!lcp_manager) {
    return;
  }
  ImageRecord* record = lcp_manager->TakeLargestIgnoredImage();
  if (!record) {
    return;
  }

  // Trigger FCP if it's not already set.
  paint_timing_detector_->GetPaintTiming().MarkFirstImagePaint();

  recorded_images_.insert(record->Hash());
  pending_images_.insert(record->Hash(), record);

  CHECK(record->HasLoadTime());
  CHECK(record->IsLoaded());
  QueueToMeasurePaintTime(record);
}

void ImagePaintTimingDetector::OnFirstAnimatedFramePainted(
    MediaRecordIdHash record_id_hash) {
  ImageRecord* record = GetPendingImage(record_id_hash);
  DCHECK(record);
  if (record->GetMediaTiming() &&
      !record->GetMediaTiming()->GetFirstVideoFrameTime().is_null()) {
    // If this is a video record, then we can get the first frame time from the
    // MediaTiming object, and can use that to set the first frame time in the
    // ImageRecord object.
    record->SetFirstAnimatedFrameTime(
        record->GetMediaTiming()->GetFirstVideoFrameTime());
    if (RuntimeEnabledFeatures::ReportFirstFrameTimeAsRenderTimeEnabled()) {
      base::TimeTicks paint_time = record->FirstAnimatedFrameTime();
      // TODO(crbug.com/383568320): this timestamp it not specified, and it's
      // not clear how it should be coarsened.
      LocalDOMWindow* window =
          paint_timing_detector_->GetPaintTiming().GetDocument()->domWindow();
      DOMHighResTimeStamp dom_timestamp =
          DOMWindowPerformance::performance(CHECK_DEREF(window))
              ->MonotonicTimeToDOMHighResTimeStamp(paint_time);
      record->SetPaintTime(paint_time,
                           DOMPaintTimingInfo{dom_timestamp, dom_timestamp});
    }
  } else if (!record->HasFirstAnimatedFrameTime()) {
    // Otherwise, this is an animated image, and so we should wait for the
    // presentation callback to fire to set the first frame presentation time.
    record->SetIsFirstAnimatedFramePaintTimingQueued(true);
    QueueToMeasurePaintTime(record);
  }
}

void ImagePaintTimingDetector::OnImageLoaded(ImageRecord* record,
                                             const StyleImage* style_image) {
  if (!style_image) {
    auto it = image_finished_times_.find(record->Hash());
    if (it != image_finished_times_.end()) {
      record->SetLoadTime(it->value);
      CHECK(record->HasLoadTime());
    }
  } else {
    LocalDOMWindow* window =
        paint_timing_detector_->GetPaintTiming().GetDocument()->domWindow();
    record->SetLoadTime(ImageElementTiming::From(CHECK_DEREF(window))
                            .GetBackgroundImageLoadTime(style_image));
  }
  record->MarkLoaded();
  QueueToMeasurePaintTime(record);
}

ImageRecord* ImagePaintTimingDetector::RemoveRecord(
    MediaRecordIdHash record_id_hash) {
  recorded_images_.erase(record_id_hash);
  image_finished_times_.erase(record_id_hash);
  auto it = pending_images_.find(record_id_hash);
  if (it != pending_images_.end()) {
    ImageRecord* record = it->value;
    pending_images_.erase(it);
    // Leave out `images_queued_for_paint_time_` intentionally because the null
    // record can be removed in `AssignPaintTimeToRegisteredQueuedRecords`.
    return record;
  }
  return nullptr;
}

void ImagePaintTimingDetector::Trace(Visitor* visitor) const {
  visitor->Trace(paint_timing_detector_);
  visitor->Trace(pending_images_);
  visitor->Trace(images_queued_for_paint_time_);
}

LargestContentfulPaintManager*
ImagePaintTimingDetector::GetLargestContentfulPaintManager() const {
  return paint_timing_detector_->GetPaintTiming()
      .GetLargestContentfulPaintManager();
}

uint64_t ImagePaintTimingDetector::ViewportSize() {
  if (viewport_size_.has_value()) {
    return *viewport_size_;
  }
  // Use the page viewport (aka the main frame viewport) for all frames,
  // including iframes. This prevents us from discarding images with size equal
  // to the size of its embedding iframe.
  Page* page =
      paint_timing_detector_->GetPaintTiming().GetDocument()->GetPage();
  gfx::Rect viewport_int_rect =
      page->GetVisualViewport().VisibleContentRect(kExcludeScrollbars);
  gfx::RectF viewport =
      paint_timing_detector_->BlinkSpaceToDIPs(gfx::RectF(viewport_int_rect));
  viewport_size_ = viewport.size().GetArea();
  return *viewport_size_;
}

}  // namespace blink
