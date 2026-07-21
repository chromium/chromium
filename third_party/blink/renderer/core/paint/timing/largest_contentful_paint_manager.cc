// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_manager.h"

#include "base/check.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"

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
  DOMWindowPerformance::performance(*window_.Get())
      ->timingForReporting()
      ->SetLargestContentfulPaintDetailsForMetrics(
          largest_contentful_paint_calculator_->LatestLcpDetails());
  paint_timing::NotifyLoaderPerformanceTimingChanged(window_);
}

void LargestContentfulPaintManager::OnFirstInputOrScroll() {
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
}

void LargestContentfulPaintManager::InitializePaintTracking(
    ImageRecord* record) {
  CHECK(largest_contentful_paint_calculator_);
  contains_full_viewport_image_ |=
      record->GetEffectiveVisualSizeResult().is_viewport_covered;
  if (largest_contentful_paint_calculator_->ShouldTrackForPaintTiming(
          *record)) {
    record->SetIsNeededForLargestContentfulPaint(true);
    if (IgnorePaintTimingScope::IgnoreDepth() == 0) {
      largest_contentful_paint_calculator_->OnImageFirstPaint(record);
    }
  }
}

void LargestContentfulPaintManager::InitializePaintTracking(
    TextRecord* record) {
  CHECK(largest_contentful_paint_calculator_);
  // Note: unlike images, this tracks any records that are eligible for LCP,
  // even if they're not larger than the current candidate. This affects the
  // HUD, but doesn't affect LCP.
  if (largest_contentful_paint_calculator_->IsEligibleForLcp(*record)) {
    record->SetIsNeededForLargestContentfulPaint(true);
  }
}

void LargestContentfulPaintManager::OnPendingImageRemoved(ImageRecord* record) {
  CHECK(largest_contentful_paint_calculator_);
  if (record->IsNeededForLargestContentfulPaint()) {
    largest_contentful_paint_calculator_->OnPendingImageRemoved(record);
  }
}

void LargestContentfulPaintManager::OnFramePresented(
    const HeapVector<Member<ImageRecord>>& image_records,
    const HeapVector<Member<TextRecord>>& text_records) {
  // `largest_contentful_paint_calculator_` can be null if input arrived between
  // paint and presentation time.
  // TODO(crbug.com/454082773): These values should count towards LCP.
  if (!largest_contentful_paint_calculator_) {
    return;
  }
  largest_contentful_paint_calculator_->OnFramePresented(image_records,
                                                         text_records);
}

}  // namespace blink
