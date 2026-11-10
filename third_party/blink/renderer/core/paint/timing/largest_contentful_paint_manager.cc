// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_manager.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "cc/layers/heads_up_display_layer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_utils.h"
#include "third_party/blink/renderer/core/paint/timing/web_vitals_hud_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/largest_contentful_paint.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/platform/graphics/paint/ignore_paint_timing_scope.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/media_timing.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

template <IsDerivedFromPaintTimingRecord T>
void ProcessLcpCandidates(
    const HeapVector<Member<T>>& records,
    LargestContentfulPaintCalculator::LcpCandidates* candidates) {
  for (const auto& record : records) {
    // Filter out anything that wasn't a valid candidate for LCP but might have
    // been valid for other PaintTiming clients.
    if (!record->IsNeededForLargestContentfulPaint()) {
      continue;
    }
    // Hard navigation LCP does not consider records whose node has been removed
    // from the DOM.
    //
    // TODO(crbug.com/454082773): we should consider allowing these to be LCP
    // candidates since they would have been shown to the user, and since it
    // better matches the LCP spec.
    if (record->WasNodeRemoved()) {
      continue;
    }
    candidates->MaybeUpdateCandidate(record.Get());
  }
}

}  // namespace

LargestContentfulPaintManager::LargestContentfulPaintManager(
    LocalDOMWindow* window)
    : window_(window),
      largest_contentful_paint_calculator_(
          MakeGarbageCollected<LargestContentfulPaintCalculator>(
              DOMWindowPerformance::performance(*window),
              this)) {}

void LargestContentfulPaintManager::OnLcpMetricsForReportingChanged() {
  const LargestContentfulPaintDetails& lcp_details =
      largest_contentful_paint_calculator_->LatestLcpDetails();
  DOMWindowPerformance::performance(*window_.Get())
      ->timingForReporting()
      ->SetLargestContentfulPaintDetailsForMetrics(lcp_details);
  paint_timing::NotifyLoaderPerformanceTimingChanged(window_);

  // Notify the browser of the updated largest contentful paint candidate so
  // that startup profiling can observe it (mirrors the FCP notification), and
  // pass the candidate's renderer-side presentation timestamp so the browser
  // does not have to approximate the timing on IPC arrival.
  // LocalFrame::OnLargestContentfulPaint() filters to the outermost main frame.
  if (LocalFrame* frame = window_->GetFrame()) {
    frame->OnLargestContentfulPaint(lcp_details.PresentationTime());
  }
}

void LargestContentfulPaintManager::OnInputOrScroll() {
  // `PaintTiming` is only expected to call this once.
  CHECK(IsRecordingLcp());
  is_recording_lcp_ = false;

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

PaintTimingClient::Type LargestContentfulPaintManager::GetType() const {
  return Type::kLargestContentfulPaint;
}

void LargestContentfulPaintManager::OnElementFirstContentfulPaint(
    ImageRecord* record) {
  CHECK(IsRecordingLcp());
  // Inform the `largest_contentful_paint_calculator_` so it can update the
  // largest pending image if needed.
  largest_contentful_paint_calculator_->OnImageFirstPaint(record);
}

OptionalPaintTimingCallback LargestContentfulPaintManager::OnPaintFinished(
    const HeapVector<Member<ImageRecord>>& image_records,
    const HeapVector<Member<TextRecord>>& text_records) {
  CHECK(IsRecordingLcp());
  if (image_records.empty() && text_records.empty()) {
    return std::nullopt;
  }

  LocalFrame* frame = window_->GetFrame();
  CHECK(frame);
  WebVitalsHudHelper hud_helper(
      cc::WebVitalMetricType::kNavigationContentfulPaint, frame->View());

  for (const auto& record : image_records) {
    contains_full_viewport_image_ |=
        record->GetEffectiveVisualSizeResult().is_viewport_covered;
    if (!largest_contentful_paint_calculator_->IsEligibleForLcp(*record)) {
      continue;
    }
    hud_helper.AddWebVitalsDebugRect(*record);
    if (largest_contentful_paint_calculator_->IsNewLargestCandidate(*record)) {
      record->SetIsNeededForLargestContentfulPaint(true);
    }
  }

  for (const auto& record : text_records) {
    if (record->WasPreviouslyReported() ||
        !largest_contentful_paint_calculator_->IsEligibleForLcp(*record)) {
      continue;
    }
    hud_helper.AddWebVitalsDebugRect(*record);
    if (largest_contentful_paint_calculator_->IsNewLargestCandidate(*record)) {
      record->SetIsNeededForLargestContentfulPaint(true);
    }
  }
  return std::nullopt;
}

void LargestContentfulPaintManager::OnFramePresented(
    const HeapVector<Member<ImageRecord>>& image_records,
    const HeapVector<Member<TextRecord>>& text_records,
    const base::TimeTicks& presentation_timestamp,
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id) {
  if (!IsRecordingLcp()) {
    return;
  }

  auto* candidates =
      MakeGarbageCollected<LargestContentfulPaintCalculator::LcpCandidates>();
  ProcessLcpCandidates(image_records, candidates);
  ProcessLcpCandidates(text_records, candidates);

  if (OptionalPaintTimingCallback callback =
          MaybeUpdateLargestContentfulPaint(candidates)) {
    std::move(*callback).Run(presentation_timestamp, paint_timing_info,
                             navigation_id);
  }
}

OptionalPaintTimingCallback
LargestContentfulPaintManager::MaybeUpdateLargestContentfulPaint(
    LargestContentfulPaintCalculator::LcpCandidates* candidates) {
  LargestContentfulPaintCalculator::PendingCandidateData candidate_data =
      largest_contentful_paint_calculator_->MaybeUpdateLargestContentfulPaint(
          candidates);
  if (!candidate_data.lcp_entry && !candidate_data.metrics_callback) {
    return std::nullopt;
  }

  return blink::BindOnce(
      &LargestContentfulPaintManager::FlushCandidateAndMetricsOnFramePresented,
      WrapWeakPersistent(this), std::move(candidate_data.metrics_callback),
      WrapPersistent(candidate_data.lcp_entry),
      std::move(candidate_data.lcp_trace_data));
}

void LargestContentfulPaintManager::FlushCandidateAndMetricsOnFramePresented(
    OptionalPaintTimingCallback metrics_callback,
    LargestContentfulPaint* lcp_entry,
    std::unique_ptr<TracedValue> trace_data,
    const base::TimeTicks& presentation_timestamp,
    const DOMPaintTimingInfo& paint_timing_info,
    uint64_t navigation_id) {
  // Input or scroll can arrive between paint and presentation time.
  // TODO(crbug.com/454082773): Candidates presented before input should count
  // towards LCP.
  if (!IsRecordingLcp()) {
    return;
  }

  if (metrics_callback) {
    std::move(*metrics_callback)
        .Run(presentation_timestamp, paint_timing_info, navigation_id);
  }

  // `lcp_entry` can be null if there is a `metrics_callback` if the largest
  // text or image was updated, but that element was not larger than the current
  // global LCP candidate.
  if (!lcp_entry) {
    return;
  }

  lcp_entry->FinalizeEntry(paint_timing_info, navigation_id);
  DOMWindowPerformance::performance(*window_.Get())
      ->OnLargestContentfulPaintUpdated(lcp_entry);

  if (trace_data) {
    TRACE_EVENT_MARK_WITH_TIMESTAMP2(
        paint_timing::kWebExposedLcpTraceCategories,
        "largestContentfulPaint::Candidate", presentation_timestamp, "data",
        std::move(trace_data), "frame",
        GetFrameIdForTracing(window_->GetFrame()));
  }
}

void LargestContentfulPaintManager::OnImageRemoved(const LayoutObject& object,
                                                   const MediaTiming* timing) {
  // Notify the lcp calculator so it can clear the largest pending image, if
  // that was removed.
  CHECK(IsRecordingLcp());
  largest_contentful_paint_calculator_->OnImageRemoved(object, timing);
  // Also check if the `largest_ignored_image_` was removed. Compare
  // `LayoutObject`s as well since the `MediaTiming` can be shared.
  ImageRecord* record = GetLargestIgnoredImageIfNotRemoved();
  if (!record || (record->GetMediaTiming() == timing &&
                  record->ImageGeneratingLayoutObject() == &object)) {
    largest_ignored_image_ = nullptr;
  }
}

TextRecord* LargestContentfulPaintManager::TakeLargestIgnoredText() {
  CHECK(IsRecordingLcp());

  TextRecord* record = GetLargestIgnoredTextIfNotRemoved();
  largest_ignored_text_ = {nullptr, nullptr};
  return record;
}

ImageRecord* LargestContentfulPaintManager::TakeLargestIgnoredImage() {
  CHECK(IsRecordingLcp());

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
  CHECK(IsRecordingLcp());

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
  CHECK(IsRecordingLcp());

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
