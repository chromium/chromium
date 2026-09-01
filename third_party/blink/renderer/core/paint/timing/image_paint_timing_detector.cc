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

namespace {

bool IsSufficientlyLoadedForReporting(const MediaTiming& media_timing) {
  if (media_timing.IsSufficientContentLoadedForPaint()) {
    return true;
  }
  if (RuntimeEnabledFeatures::ReportFirstFrameTimeAsRenderTimeEnabled() &&
      media_timing.IsPaintedFirstFrame()) {
    return true;
  }
  return false;
}

}  // namespace

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

  for (ImageRecord* record : images_queued_for_paint_time_) {
    cc::WebVitalMetricType type;

    if (record->GetSoftNavigationContext()) {
      type = cc::WebVitalMetricType::kInteractionContentfulPaint;
    } else if (record->IsNeededForLargestContentfulPaint()) {
      type = cc::WebVitalMetricType::kNavigationContentfulPaint;
    } else {
      continue;
    }

    hud_layer->AddWebVitalsDebugRect(
        {type, gfx::ToEnclosedRect(
                   widget->DIPsToBlinkSpace(record->RootVisualRect()))});
  }
}

void ImagePaintTimingDetector::NotifyImageRemoved(
    const LayoutObject& object,
    const MediaTiming* media_timing) {
  RemoveRecord(MediaRecordId::GenerateHash(&object, media_timing));
  ForEachPaintTimingClient([&](PaintTimingClient* client) {
    client->OnImageRemoved(object, media_timing);
  });
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
  // loaded the last time it was painted.
  auto it = pending_images_.find(record_id_hash);
  ImageRecord* record = it == pending_images_.end() ? nullptr : it->value.Get();

  // Skip measuring content that was already fully measured.
  //  - `record` is null: we aren't actively measuring this content, but we
  //    need to check the historical `recorded_images_` set to see if this is
  //    new or old content.
  //  - `record` is non-null: we are still actively measuring this content.
  if (!record && recorded_images_.Contains(record_id_hash)) {
    return false;
  }

  // The first frame of an autoplaying <video> races with its poster image if it
  // has one, and since we only use `LayoutObject` for the `record_id` for
  // videos (to avoid counting both the poster and first frame), we can end up
  // with a mismatch between the `record`'s `MediaTiming` and `media_timing`
  // while the poster image is pending. Switch to tracking the first video frame
  // in that case.
  if (record && record->GetMediaTiming() != &media_timing &&
      media_timing.IsVideo()) {
    NotifyImageRemoved(object, record->GetMediaTiming());
    record = nullptr;
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

    ForEachPaintTimingClient([&](PaintTimingClient* client) {
      client->OnElementFirstContentfulPaint(record);
    });

    // Mark the image as recorded regardless of if this is needed for any
    // PaintTiming clients so the image isn't reconsidered as a candidate.
    recorded_images_.insert(record_id_hash);

    // Finally, set up tracking future paints for this `record`.
    pending_images_.insert(record->Hash(), record);
  }

  CHECK(record);
  CHECK(!record->IsSufficientlyLoadedForReporting());

  bool is_video = !media_timing.GetFirstVideoFrameTime().is_null();

  // If this is the first frame of an animated image, we need the paint and
  // presentation time of this paint, in addition to when it becomes
  // sufficiently loaded, which could be this frame or a later one.
  //
  // TODO(crbug.com/449779010): Enable ReportFirstFrameTimeAsRenderTime and
  // track a single paint time for animated images/videos.
  if (!is_video && media_timing.IsPaintedFirstFrame() &&
      !record->HasFirstAnimatedFrameTime()) {
    // Always measure the first frame time so it's available if this record is
    // needed when it's sufficiently loaded.
    animated_images_queued_for_first_frame_time_.push_back(record);
  }

  // Check if the image is ready to be reported to clients. For most media, we
  // use `MediaTiming`'s "sufficiently loaded" signal to determine this, but for
  // animated images, we might only need to wait for the first frame (depending
  // on flags).
  if (!IsSufficientlyLoadedForReporting(media_timing)) {
    // The first video frame should always be considered sufficiently loaded.
    CHECK(!is_video);
    return false;
  }

  // Mark the image as sufficiently loaded first since clients may depend on
  // that for filtering.
  record->SetIsSufficientlyLoadedForReporting();
  if (is_video) {
    SetVideoFirstAnimatedFrameTime(record);
  } else {
    record->SetLoadTime(style_image ? LoadTime(*style_image)
                                    : LoadTime(record_id_hash));
  }

  // Inform clients about the contentful paint and set up for measuring
  // presentation time if any clients need the `record`.
  ForEachPaintTimingClient([&](PaintTimingClient* client) {
    client->OnElementLastContentfulPaint(record);
  });

  // Erase the record from `pending_images_` whether or not it's needed by
  // clients since the record is now sufficiently loaded.
  pending_images_.erase(record->Hash());

  // No client needs this `record`, so no need to process any further.
  if (!record->IsNeededForPaintTiming()) {
    return false;
  }

  // Queue the record for presentation time processing since at least one client
  // needs this `record`.
  images_queued_for_paint_time_.push_back(record);
  return true;
}

void ImagePaintTimingDetector::NotifyImageFinished(
    const LayoutObject& object,
    const MediaTiming* media_timing) {
  auto hash(MediaRecordId::GenerateHash(&object, media_timing));
  const auto& insertion_result =
      image_finished_times_.insert(hash, base::TimeTicks());
  if (insertion_result.is_new_entry) {
    insertion_result.stored_value->value = base::TimeTicks::Now();
  }
}

void ImagePaintTimingDetector::NotifyBackgroundImageFinished(
    const StyleImage* style_image) {
  const auto& insertion_result =
      background_image_finished_times_.insert(style_image, base::TimeTicks());
  if (insertion_result.is_new_entry) {
    insertion_result.stored_value->value = base::TimeTicks::Now();
  }
}

void ImagePaintTimingDetector::ReportLargestIgnoredImage() {
  // TODO(crbug.com/454082773): This is called on style change, so there's a
  // window between style and paint where the image can be removed. We should
  // defer this until paint time.
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

  // Notify clients of the first and contentful paints and set up presentation
  // feedback.
  ForEachPaintTimingClient([&](PaintTimingClient* client) {
    client->OnElementFirstContentfulPaint(record);
    client->OnElementLastContentfulPaint(record);
  });
  recorded_images_.insert(record->Hash());

  CHECK(record->IsSufficientlyLoadedForReporting());
  images_queued_for_paint_time_.push_back(record);
}

void ImagePaintTimingDetector::SetVideoFirstAnimatedFrameTime(
    ImageRecord* record) {
  CHECK(record->GetMediaTiming());
  CHECK(!record->GetMediaTiming()->GetFirstVideoFrameTime().is_null(),
        base::NotFatalUntil::M156);
  record->SetFirstAnimatedFrameTime(
      record->GetMediaTiming()->GetFirstVideoFrameTime());

  // Without this feature, the paint time will be set based on the next frame.
  if (!RuntimeEnabledFeatures::ReportFirstFrameTimeAsRenderTimeEnabled()) {
    return;
  }
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

void ImagePaintTimingDetector::RemoveRecord(MediaRecordIdHash record_id_hash) {
  recorded_images_.erase(record_id_hash);
  image_finished_times_.erase(record_id_hash);
  pending_images_.erase(record_id_hash);
}

void ImagePaintTimingDetector::Trace(Visitor* visitor) const {
  visitor->Trace(paint_timing_detector_);
  visitor->Trace(pending_images_);
  visitor->Trace(background_image_finished_times_);
  visitor->Trace(animated_images_queued_for_first_frame_time_);
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

base::TimeTicks ImagePaintTimingDetector::LoadTime(
    const LayoutObject* object,
    const MediaTiming* timing) const {
  return LoadTime(MediaRecordId::GenerateHash(object, timing));
}

base::TimeTicks ImagePaintTimingDetector::LoadTime(
    const StyleImage& image) const {
  auto it = background_image_finished_times_.find(&image);
  return it != background_image_finished_times_.end() ? it->value
                                                      : base::TimeTicks();
}

base::TimeTicks ImagePaintTimingDetector::LoadTime(
    MediaRecordIdHash hash) const {
  auto it = image_finished_times_.find(hash);
  return it != image_finished_times_.end() ? it->value : base::TimeTicks();
}

void ImagePaintTimingDetector::ForEachPaintTimingClient(
    base::FunctionRef<void(PaintTimingClient*)> callback) {
  paint_timing_detector_->GetPaintTiming().ForEachClient(std::move(callback));
}

HeapVector<Member<ImageRecord>>
ImagePaintTimingDetector::TakeImageRecordsOnPaintFinished() {
  if (!images_queued_for_paint_time_.empty()) {
    SendRectsToHud();
  }
  return std::move(images_queued_for_paint_time_);
}

HeapVector<Member<ImageRecord>>
ImagePaintTimingDetector::TakeAnimatedImageRecordsOnPaintFinished() {
  return std::move(animated_images_queued_for_first_frame_time_);
}

}  // namespace blink
