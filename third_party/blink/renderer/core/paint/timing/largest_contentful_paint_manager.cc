// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_manager.h"

#include "base/check.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"

namespace blink {

LargestContentfulPaintManager::LargestContentfulPaintManager(
    LocalDOMWindow* window)
    : window_(window),
      largest_contentful_paint_calculator_(
          MakeGarbageCollected<LargestContentfulPaintCalculator>(
              DOMWindowPerformance::performance(*window),
              this)) {}

void LargestContentfulPaintManager::EmitLcpPerformanceEntry(
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t paint_size,
    base::TimeTicks load_time,
    const AtomicString& id,
    const String& url,
    Element* element) {
  // `largest_contentful_paint_calculator_` can be null if input arrived between
  // paint and presentation time.
  // TODO(crbug.com/454082773): These values should count towards LCP.
  if (!largest_contentful_paint_calculator_) {
    return;
  }
  DOMWindowPerformance::performance(*window_.Get())
      ->OnLargestContentfulPaintUpdated(paint_timing_info, paint_size,
                                        load_time, id, url, element);
}

void LargestContentfulPaintManager::OnLcpMetricsForReportingChanged() {
  // `largest_contentful_paint_calculator_` can be null if input arrived between
  // paint and presentation time.
  // TODO(crbug.com/454082773): These values should count towards LCP.
  if (!largest_contentful_paint_calculator_) {
    return;
  }
  const LargestContentfulPaintDetails& lcp_details =
      largest_contentful_paint_calculator_->LatestLcpDetails();
  DOMWindowPerformance::performance(*window_.Get())
      ->timingForReporting()
      ->SetLargestContentfulPaintDetailsForMetrics(lcp_details);
  paint_timing::NotifyLoaderPerformanceTimingChanged(window_);

  // Notify the browser of the updated largest contentful paint candidate so
  // that startup profiling can observe it (mirrors the FCP notification), and
  // pass the candidate's renderer-side presentation timestamp so the browser
  // does not have to approximate the timing on IPC arrival. The candidate is
  // the larger of the image and text records, with ties broken by the earlier
  // paint time, matching LargestContentfulPaintCalculator's own selection in
  // UpdateLatestLcpDetailsTypeIfNeeded().
  const bool text_is_larger = lcp_details.largest_text_paint_size >
                                  lcp_details.largest_image_paint_size ||
                              (lcp_details.largest_text_paint_size ==
                                   lcp_details.largest_image_paint_size &&
                               lcp_details.largest_text_paint_time <
                                   lcp_details.largest_image_paint_time);
  const base::TimeTicks presentation_time =
      text_is_larger ? lcp_details.largest_text_paint_time
                     : lcp_details.largest_image_paint_time;
  // LocalFrame::OnLargestContentfulPaint() filters to the outermost main frame.
  if (LocalFrame* frame = window_->GetFrame()) {
    frame->OnLargestContentfulPaint(presentation_time);
  }
}

void LargestContentfulPaintManager::OnInputOrScroll() {
  // `PaintTiming` is only expected to call this once.
  CHECK(largest_contentful_paint_calculator_);

  // Stop recording LCP.
  largest_contentful_paint_calculator_ = nullptr;

  LocalFrame* frame = window_->GetFrame();
  CHECK(frame);
  if (frame->IsOutermostMainFrame()) {
    Document* document = frame->GetDocument();
    ukm::builders::Blink_PaintTiming(document->UkmSourceID())
        .SetLCPDebugging_HasViewportImage(contains_full_viewport_image_)
        .Record(document->UkmRecorder());
  }
}

void LargestContentfulPaintManager::Trace(Visitor* visitor) const {
  visitor->Trace(largest_contentful_paint_calculator_);
  visitor->Trace(window_);
  visitor->Trace(largest_ignored_text_);
  visitor->Trace(largest_ignored_image_);
}

void LargestContentfulPaintManager::OnElementFirstContentfulPaint(
    ImageRecord* record) {
  CHECK(largest_contentful_paint_calculator_);
  if (!largest_contentful_paint_calculator_->ShouldTrackForPaintTiming(
          *record)) {
    return;
  }
  // Inform the `largest_contentful_paint_calculator_` so it can update the
  // largest pending image if needed.
  largest_contentful_paint_calculator_->OnImageFirstPaint(record);
}

void LargestContentfulPaintManager::OnElementLastContentfulPaint(
    ImageRecord* record) {
  contains_full_viewport_image_ |=
      record->GetEffectiveVisualSizeResult().is_viewport_covered;
  CHECK(largest_contentful_paint_calculator_);
  record->SetIsNeededForLargestContentfulPaint(
      largest_contentful_paint_calculator_->ShouldTrackForPaintTiming(*record));
}

void LargestContentfulPaintManager::OnElementLastContentfulPaint(
    TextRecord* record,
    bool was_previously_reported) {
  CHECK(largest_contentful_paint_calculator_);
  // Note: unlike images, this tracks any records that are eligible for LCP,
  // even if they're not larger than the current candidate. This affects the
  // HUD, but doesn't affect LCP.
  record->SetIsNeededForLargestContentfulPaint(
      !was_previously_reported &&
      largest_contentful_paint_calculator_->IsEligibleForLcp(*record));
}

void LargestContentfulPaintManager::OnImageRemoved(const LayoutObject& object,
                                                   const MediaTiming* timing) {
  // Notify the lcp calculator so it can clear the largest pending image, if
  // that was removed.
  CHECK(largest_contentful_paint_calculator_);
  largest_contentful_paint_calculator_->OnImageRemoved(object, timing);
  // Also check if the `largest_ignored_image_` was removed. Compare
  // `LayoutObject`s as well since the `MediaTiming` can be shared.
  ImageRecord* record = GetLargestIgnoredImageIfNotRemoved();
  if (!record || (record->GetMediaTiming() == timing &&
                  record->GetLayoutObject() == &object)) {
    largest_ignored_image_ = nullptr;
  }
}

void LargestContentfulPaintManager::OnFramePresented(
    const HeapVector<Member<ImageRecord>>& image_records,
    const HeapVector<Member<TextRecord>>& text_records,
    const HeapVector<Member<ElementTimingInfo>>&,
    const DOMPaintTimingInfo&) {
  // `largest_contentful_paint_calculator_` can be null if input arrived between
  // paint and presentation time.
  // TODO(crbug.com/454082773): These values should count towards LCP.
  if (!largest_contentful_paint_calculator_) {
    return;
  }
  largest_contentful_paint_calculator_->OnFramePresented(image_records,
                                                         text_records);
}

TextRecord* LargestContentfulPaintManager::TakeLargestIgnoredText() {
  CHECK(largest_contentful_paint_calculator_);

  TextRecord* record = GetLargestIgnoredTextIfNotRemoved();
  largest_ignored_text_ = {nullptr, nullptr};
  return record;
}

ImageRecord* LargestContentfulPaintManager::TakeLargestIgnoredImage() {
  CHECK(largest_contentful_paint_calculator_);

  ImageRecord* record = GetLargestIgnoredImageIfNotRemoved();
  if (record) {
    // Notify the `largest_contentful_paint_calculator_` about the image so it
    // can update the largest pending image.
    largest_contentful_paint_calculator_->OnImageFirstPaint(record);
  }
  largest_ignored_image_ = nullptr;
  return record;
}

TextRecord* LargestContentfulPaintManager::GetLargestIgnoredTextIfNotRemoved()
    const {
  TextRecord* record = largest_ignored_text_.value;
  return record && !record->WasNodeRemoved() ? record : nullptr;
}

ImageRecord* LargestContentfulPaintManager::GetLargestIgnoredImageIfNotRemoved()
    const {
  ImageRecord* record = largest_ignored_image_;
  return record && !record->WasNodeRemoved() && !!record->GetMediaTiming()
             ? record
             : nullptr;
}

void LargestContentfulPaintManager::MaybeUpdateLargestIgnoredText(
    const LayoutObject& object,
    TextRecord* record) {
  CHECK(largest_contentful_paint_calculator_);

  if (IgnorePaintTimingScope::IgnoreDepth() != 1 ||
      !IgnorePaintTimingScope::IsDocumentElementInvisible()) {
    return;
  }

  if (largest_contentful_paint_calculator_->IsEligibleForLcp(*record) &&
      record->IsEffectiveSizeLargerThan(GetLargestIgnoredTextIfNotRemoved())) {
    largest_ignored_text_.key = &object;
    largest_ignored_text_.value = record;
  }
}

void LargestContentfulPaintManager::MaybeUpdateLargestIgnoredImage(
    ImageRecord* record) {
  CHECK(largest_contentful_paint_calculator_);

  if (IgnorePaintTimingScope::IgnoreDepth() != 1 ||
      !IgnorePaintTimingScope::IsDocumentElementInvisible()) {
    return;
  }

  CHECK(record->GetMediaTiming());
  // TODO(crbug.com/449779010): This should probably be based on first frame for
  // animated images.
  if (!record->GetMediaTiming()->IsSufficientContentLoadedForPaint()) {
    return;
  }

  // Set the load time now since the image is sufficiently loaded, rather
  // than waiting until the opacity changes.
  //
  // TODO(crbug.com/503691215): Can we use the actual image load time here
  // rather instead? It's not clear why this inconsistency exists.
  record->SetLoadTime(base::TimeTicks::Now());
  record->SetIsSufficientlyLoadedForReporting();

  if (largest_contentful_paint_calculator_->IsEligibleForLcp(*record) &&
      record->IsEffectiveSizeLargerThan(largest_ignored_image_)) {
    largest_ignored_image_ = record;
  }
}

}  // namespace blink
