// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_timing.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/interactive_detector.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/document_resource_coordinator.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

WindowPerformance* GetPerformanceInstance(LocalFrame* frame) {
  WindowPerformance* performance = nullptr;
  if (frame && frame->DomWindow()) {
    performance = DOMWindowPerformance::performance(*frame->DomWindow());
  }
  return performance;
}

}  // namespace

// static
const char PaintTiming::kSupplementName[] = "PaintTiming";

// static
PaintTiming& PaintTiming::From(Document& document) {
  PaintTiming* timing = Supplement<Document>::From<PaintTiming>(document);
  if (!timing) {
    timing = MakeGarbageCollected<PaintTiming>(document);
    ProvideTo(document, timing);
  }
  return *timing;
}

void PaintTiming::MarkFirstPaint() {
  // Test that |first_paint_| is non-zero here, as well as in setFirstPaint, so
  // we avoid invoking monotonicallyIncreasingTime() on every call to
  // markFirstPaint().
  if (!first_paint_.is_null())
    return;
  SetFirstPaint(clock_->NowTicks());
}

void PaintTiming::MarkFirstContentfulPaint() {
  // Test that |first_contentful_paint_| is non-zero here, as well as in
  // setFirstContentfulPaint, so we avoid invoking
  // monotonicallyIncreasingTime() on every call to
  // markFirstContentfulPaint().
  if (!first_contentful_paint_.is_null())
    return;
  SetFirstContentfulPaint(clock_->NowTicks());
}

void PaintTiming::MarkFirstImagePaint() {
  if (!first_image_paint_.is_null())
    return;
  first_image_paint_ = clock_->NowTicks();
  SetFirstContentfulPaint(first_image_paint_);
  RegisterNotifySwapTime(PaintEvent::kFirstImagePaint);
}

void PaintTiming::MarkFirstEligibleToPaint() {
  if (!first_eligible_to_paint_.is_null())
    return;

  first_eligible_to_paint_ = clock_->NowTicks();
  NotifyPaintTimingChanged();
}

// We deliberately use |first_paint_| here rather than |first_paint_swap_|,
// because |first_paint_swap_| is set asynchronously and we need to be able to
// rely on a synchronous check that SetFirstPaintSwap hasn't been scheduled or
// run.
void PaintTiming::MarkIneligibleToPaint() {
  if (first_eligible_to_paint_.is_null() || !first_paint_.is_null())
    return;

  first_eligible_to_paint_ = base::TimeTicks();
  NotifyPaintTimingChanged();
}

void PaintTiming::SetFirstMeaningfulPaintCandidate(base::TimeTicks timestamp) {
  if (!first_meaningful_paint_candidate_.is_null())
    return;
  first_meaningful_paint_candidate_ = timestamp;
  if (GetFrame() && GetFrame()->View() && !GetFrame()->View()->IsAttached()) {
    GetFrame()->GetFrameScheduler()->OnFirstMeaningfulPaint();
  }
}

void PaintTiming::SetFirstMeaningfulPaint(
    base::TimeTicks swap_stamp,
    FirstMeaningfulPaintDetector::HadUserInput had_input) {
  DCHECK(first_meaningful_paint_swap_.is_null());
  DCHECK(!swap_stamp.is_null());

  TRACE_EVENT_MARK_WITH_TIMESTAMP2(
      "loading,rail,devtools.timeline", "firstMeaningfulPaint", swap_stamp,
      "frame", ToTraceValue(GetFrame()), "afterUserInput", had_input);

  // Notify FMP for UMA only if there's no user input before FMP, so that layout
  // changes caused by user interactions wouldn't be considered as FMP.
  if (had_input == FirstMeaningfulPaintDetector::kNoUserInput) {
    first_meaningful_paint_swap_ = swap_stamp;
    NotifyPaintTimingChanged();
  }
}

void PaintTiming::NotifyPaint(bool is_first_paint,
                              bool text_painted,
                              bool image_painted) {
  if (is_first_paint)
    MarkFirstPaint();
  if (text_painted)
    MarkFirstContentfulPaint();
  if (image_painted)
    MarkFirstImagePaint();
  fmp_detector_->NotifyPaint();
}

void PaintTiming::OnPortalActivate() {
  last_portal_activated_swap_ = base::TimeTicks();
  RegisterNotifySwapTime(PaintEvent::kPortalActivatedPaint);
}

void PaintTiming::SetPortalActivatedPaint(base::TimeTicks stamp) {
  DCHECK(last_portal_activated_swap_.is_null());
  last_portal_activated_swap_ = stamp;
  NotifyPaintTimingChanged();
}

void PaintTiming::SetTickClockForTesting(const base::TickClock* clock) {
  clock_ = clock;
}

void PaintTiming::Trace(Visitor* visitor) const {
  visitor->Trace(fmp_detector_);
  Supplement<Document>::Trace(visitor);
}

PaintTiming::PaintTiming(Document& document)
    : Supplement<Document>(document),
      fmp_detector_(MakeGarbageCollected<FirstMeaningfulPaintDetector>(this)),
      clock_(base::DefaultTickClock::GetInstance()) {}

LocalFrame* PaintTiming::GetFrame() const {
  return GetSupplementable()->GetFrame();
}

void PaintTiming::NotifyPaintTimingChanged() {
  if (GetSupplementable()->Loader())
    GetSupplementable()->Loader()->DidChangePerformanceTiming();
}

void PaintTiming::SetFirstPaint(base::TimeTicks stamp) {
  if (!first_paint_.is_null())
    return;

  LocalFrame* frame = GetFrame();
  if (frame && frame->GetDocument()) {
    Document* document = frame->GetDocument();
    document->MarkFirstPaint();
    if (frame->IsMainFrame())
      document->Fetcher()->MarkFirstPaint();
  }

  first_paint_ = stamp;
  RegisterNotifySwapTime(PaintEvent::kFirstPaint);
}

void PaintTiming::SetFirstContentfulPaint(base::TimeTicks stamp) {
  if (!first_contentful_paint_.is_null())
    return;
  SetFirstPaint(stamp);
  first_contentful_paint_ = stamp;
  RegisterNotifySwapTime(PaintEvent::kFirstContentfulPaint);

  // Restart commits that may have been deferred.
  LocalFrame* frame = GetFrame();
  if (!frame || !frame->IsMainFrame())
    return;
  frame->View()->OnFirstContentfulPaint();

  if (frame->GetDocument() && frame->GetDocument()->Fetcher())
    frame->GetDocument()->Fetcher()->MarkFirstContentfulPaint();

  if (frame->GetFrameScheduler())
    frame->GetFrameScheduler()->OnFirstContentfulPaint();
}

void PaintTiming::RegisterNotifySwapTime(PaintEvent event) {
  RegisterNotifySwapTime(
      CrossThreadBindOnce(&PaintTiming::ReportSwapTime,
                          WrapCrossThreadWeakPersistent(this), event));
}

void PaintTiming::RegisterNotifyFirstPaintAfterBackForwardCacheRestoreSwapTime(
    size_t index) {
  RegisterNotifySwapTime(CrossThreadBindOnce(
      &PaintTiming::ReportFirstPaintAfterBackForwardCacheRestoreSwapTime,
      WrapCrossThreadWeakPersistent(this), index));
}

void PaintTiming::RegisterNotifySwapTime(ReportTimeCallback callback) {
  // ReportSwapTime will queue a swap-promise, the callback is called when the
  // compositor submission of the current render frame completes or fails to
  // happen.
  if (!GetFrame() || !GetFrame()->GetPage())
    return;
  GetFrame()->GetPage()->GetChromeClient().NotifySwapTime(*GetFrame(),
                                                          std::move(callback));
}

void PaintTiming::ReportSwapTime(PaintEvent event,
                                 WebSwapResult result,
                                 base::TimeTicks timestamp) {
  DCHECK(IsMainThread());
  // If the swap fails for any reason, we use the timestamp when the SwapPromise
  // was broken. |result| == WebSwapResult::kDidNotSwapSwapFails
  // usually means the compositor decided not swap because there was no actual
  // damage, which can happen when what's being painted isn't visible. In this
  // case, the timestamp will be consistent with the case where the swap
  // succeeds, as they both capture the time up to swap. In other failure cases
  // (aborts during commit), this timestamp is an improvement over the blink
  // paint time, but does not capture some time we're interested in, e.g.  image
  // decoding.
  //
  // TODO(crbug.com/738235): Consider not reporting any timestamp when failing
  // for reasons other than kDidNotSwapSwapFails.
  ReportSwapResultHistogram(result);
  switch (event) {
    case PaintEvent::kFirstPaint:
      SetFirstPaintSwap(timestamp);
      return;
    case PaintEvent::kFirstContentfulPaint:
      SetFirstContentfulPaintSwap(timestamp);
      return;
    case PaintEvent::kFirstImagePaint:
      SetFirstImagePaintSwap(timestamp);
      return;
    case PaintEvent::kPortalActivatedPaint:
      SetPortalActivatedPaint(timestamp);
      return;
    default:
      NOTREACHED();
  }
}

void PaintTiming::ReportFirstPaintAfterBackForwardCacheRestoreSwapTime(
    size_t index,
    WebSwapResult result,
    base::TimeTicks timestamp) {
  DCHECK(IsMainThread());
  ReportSwapResultHistogram(result);
  SetFirstPaintAfterBackForwardCacheRestoreSwap(timestamp, index);
}

void PaintTiming::SetFirstPaintSwap(base::TimeTicks stamp) {
  DCHECK(first_paint_swap_.is_null());
  first_paint_swap_ = stamp;
  probe::PaintTiming(GetSupplementable(), "firstPaint",
                     first_paint_swap_.since_origin().InSecondsF());
  WindowPerformance* performance = GetPerformanceInstance(GetFrame());
  if (performance)
    performance->AddFirstPaintTiming(first_paint_swap_);
  NotifyPaintTimingChanged();
}

void PaintTiming::SetFirstContentfulPaintSwap(base::TimeTicks stamp) {
  DCHECK(first_contentful_paint_swap_.is_null());
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0("loading", "FirstContentfulPaint",
                                      TRACE_EVENT_SCOPE_GLOBAL, stamp);
  first_contentful_paint_swap_ = stamp;
  probe::PaintTiming(GetSupplementable(), "firstContentfulPaint",
                     first_contentful_paint_swap_.since_origin().InSecondsF());
  WindowPerformance* performance = GetPerformanceInstance(GetFrame());
  if (performance)
    performance->AddFirstContentfulPaintTiming(first_contentful_paint_swap_);
  if (GetFrame())
    GetFrame()->Loader().Progress().DidFirstContentfulPaint();
  NotifyPaintTimingChanged();
  fmp_detector_->NotifyFirstContentfulPaint(first_contentful_paint_swap_);
  InteractiveDetector* interactive_detector =
      InteractiveDetector::From(*GetSupplementable());
  if (interactive_detector) {
    interactive_detector->OnFirstContentfulPaint(first_contentful_paint_swap_);
  }
  auto* coordinator = GetSupplementable()->GetResourceCoordinator();
  if (coordinator && GetFrame() && GetFrame()->IsMainFrame()) {
    PerformanceTiming* timing = performance->timing();
    base::TimeDelta fcp = stamp - timing->NavigationStartAsMonotonicTime();
    coordinator->OnFirstContentfulPaint(fcp);
  }
}

void PaintTiming::SetFirstImagePaintSwap(base::TimeTicks stamp) {
  DCHECK(first_image_paint_swap_.is_null());
  first_image_paint_swap_ = stamp;
  probe::PaintTiming(GetSupplementable(), "firstImagePaint",
                     first_image_paint_swap_.since_origin().InSecondsF());
  NotifyPaintTimingChanged();
}

void PaintTiming::SetFirstPaintAfterBackForwardCacheRestoreSwap(
    base::TimeTicks stamp,
    size_t index) {
  // The elements are allocated when the page is restored from the cache.
  DCHECK_GE(first_paints_after_back_forward_cache_restore_swap_.size(), index);
  DCHECK(first_paints_after_back_forward_cache_restore_swap_[index].is_null());
  first_paints_after_back_forward_cache_restore_swap_[index] = stamp;
  NotifyPaintTimingChanged();
}

void PaintTiming::ReportSwapResultHistogram(WebSwapResult result) {
  UMA_HISTOGRAM_ENUMERATION("PageLoad.Internal.Renderer.PaintTiming.SwapResult",
                            result);
}

void PaintTiming::OnRestoredFromBackForwardCache() {
  // Allocate the last element with 0, which indicates that the first paint
  // after this navigation doesn't happen yet.
  size_t index = first_paints_after_back_forward_cache_restore_swap_.size();
  first_paints_after_back_forward_cache_restore_swap_.push_back(
      base::TimeTicks());
  RegisterNotifyFirstPaintAfterBackForwardCacheRestoreSwapTime(index);
}

}  // namespace blink
