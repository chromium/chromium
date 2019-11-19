// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"

#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/paint/paint_timing.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

// Web fonts that laid out more than this number of characters block First
// Meaningful Paint.
const int kBlankCharactersThreshold = 200;

const base::TickClock* g_clock = nullptr;
}  // namespace

FirstMeaningfulPaintDetector& FirstMeaningfulPaintDetector::From(
    Document& document) {
  return PaintTiming::From(document).GetFirstMeaningfulPaintDetector();
}

FirstMeaningfulPaintDetector::FirstMeaningfulPaintDetector(
    PaintTiming* paint_timing)
    : paint_timing_(paint_timing) {
  if (!g_clock)
    g_clock = base::DefaultTickClock::GetInstance();
}

Document* FirstMeaningfulPaintDetector::GetDocument() {
  return paint_timing_->GetSupplementable();
}

// Computes "layout significance" (http://goo.gl/rytlPL) of a layout operation.
// Significance of a layout is the number of layout objects newly added to the
// layout tree, weighted by page height (before and after the layout).
// A paint after the most significance layout during page load is reported as
// First Meaningful Paint.
void FirstMeaningfulPaintDetector::MarkNextPaintAsMeaningfulIfNeeded(
    const LayoutObjectCounter& counter,
    double contents_height_before_layout,
    double contents_height_after_layout,
    int visible_height) {
  if (network_quiet_reached_)
    return;

  unsigned delta = counter.Count() - prev_layout_object_count_;
  prev_layout_object_count_ = counter.Count();

  if (visible_height == 0)
    return;

  double ratio_before =
      std::max(1.0, contents_height_before_layout / visible_height);
  double ratio_after =
      std::max(1.0, contents_height_after_layout / visible_height);
  double significance = delta / ((ratio_before + ratio_after) / 2);

  // If the page has many blank characters, the significance value is
  // accumulated until the text become visible.
  size_t approximate_blank_character_count =
      FontFaceSetDocument::ApproximateBlankCharacterCount(*GetDocument());
  if (approximate_blank_character_count > kBlankCharactersThreshold) {
    accumulated_significance_while_having_blank_text_ += significance;
  } else {
    significance += accumulated_significance_while_having_blank_text_;
    accumulated_significance_while_having_blank_text_ = 0;
    if (significance > max_significance_so_far_) {
      next_paint_is_meaningful_ = true;
      max_significance_so_far_ = significance;
    }
  }
}

void FirstMeaningfulPaintDetector::NotifyPaint() {
  if (!next_paint_is_meaningful_)
    return;

  // Skip document background-only paints.
  if (paint_timing_->FirstPaintRendered().is_null())
    return;
  provisional_first_meaningful_paint_ = g_clock->NowTicks();
  next_paint_is_meaningful_ = false;

  if (network_quiet_reached_)
    return;

  had_user_input_before_provisional_first_meaningful_paint_ = had_user_input_;
  provisional_first_meaningful_paint_swap_ = base::TimeTicks();
  RegisterNotifySwapTime(PaintEvent::kProvisionalFirstMeaningfulPaint);
}

// This is called only on FirstMeaningfulPaintDetector for main frame.
void FirstMeaningfulPaintDetector::NotifyInputEvent() {
  // Ignore user inputs before first paint.
  if (paint_timing_->FirstPaintRendered().is_null())
    return;
  had_user_input_ = kHadUserInput;
}

void FirstMeaningfulPaintDetector::OnNetwork2Quiet() {
  if (!GetDocument() || network_quiet_reached_ ||
      paint_timing_->FirstContentfulPaintRendered().is_null())
    return;
  network_quiet_reached_ = true;

  if (!provisional_first_meaningful_paint_.is_null()) {
    base::TimeTicks first_meaningful_paint_swap;
    // Enforce FirstContentfulPaint <= FirstMeaningfulPaint.
    if (provisional_first_meaningful_paint_ <
        paint_timing_->FirstContentfulPaintRendered()) {
      first_meaningful_paint_ = paint_timing_->FirstContentfulPaintRendered();
      first_meaningful_paint_swap = paint_timing_->FirstContentfulPaint();
      // It's possible that this timer fires between when the first contentful
      // paint is set and its SwapPromise is fulfilled. If this happens, defer
      // until NotifyFirstContentfulPaint() is called.
      if (first_meaningful_paint_swap.is_null())
        defer_first_meaningful_paint_ = kDeferFirstContentfulPaintNotSet;
    } else {
      first_meaningful_paint_ = provisional_first_meaningful_paint_;
      first_meaningful_paint_swap = provisional_first_meaningful_paint_swap_;
      // We might still be waiting for one or more swap promises, in which case
      // we want to defer reporting first meaningful paint until they complete.
      // Otherwise, we would either report the wrong swap timestamp or none at
      // all.
      if (outstanding_swap_promise_count_ > 0)
        defer_first_meaningful_paint_ = kDeferOutstandingSwapPromises;
    }
    if (defer_first_meaningful_paint_ == kDoNotDefer) {
      // Report FirstMeaningfulPaint when the page reached network 2-quiet if
      // we aren't waiting for a swap timestamp.
      SetFirstMeaningfulPaint(first_meaningful_paint_swap);
    }
  }
}

void FirstMeaningfulPaintDetector::RegisterNotifySwapTime(PaintEvent event) {
  ++outstanding_swap_promise_count_;
  paint_timing_->RegisterNotifySwapTime(
      event, CrossThreadBindOnce(&FirstMeaningfulPaintDetector::ReportSwapTime,
                                 WrapCrossThreadWeakPersistent(this), event));
}

void FirstMeaningfulPaintDetector::ReportSwapTime(
    PaintEvent event,
    WebWidgetClient::SwapResult result,
    base::TimeTicks timestamp) {
  DCHECK(event == PaintEvent::kProvisionalFirstMeaningfulPaint);
  DCHECK_GT(outstanding_swap_promise_count_, 0U);
  --outstanding_swap_promise_count_;

  // If the swap fails for any reason, we use the timestamp when the SwapPromise
  // was broken. |result| == WebWidgetClient::SwapResult::kDidNotSwapSwapFails
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
  paint_timing_->ReportSwapResultHistogram(result);
  provisional_first_meaningful_paint_swap_ = timestamp;

  probe::PaintTiming(GetDocument(), "firstMeaningfulPaintCandidate",
                     timestamp.since_origin().InSecondsF());

  // Ignore the first meaningful paint candidate as this generally is the first
  // contentful paint itself.
  if (!seen_first_meaningful_paint_candidate_) {
    seen_first_meaningful_paint_candidate_ = true;
  } else {
    paint_timing_->SetFirstMeaningfulPaintCandidate(
        provisional_first_meaningful_paint_swap_);
  }

  if (defer_first_meaningful_paint_ == kDeferOutstandingSwapPromises &&
      outstanding_swap_promise_count_ == 0) {
    DCHECK(!first_meaningful_paint_.is_null());
    SetFirstMeaningfulPaint(provisional_first_meaningful_paint_swap_);
  }
}

void FirstMeaningfulPaintDetector::NotifyFirstContentfulPaint(
    base::TimeTicks swap_stamp) {
  if (defer_first_meaningful_paint_ != kDeferFirstContentfulPaintNotSet)
    return;
  SetFirstMeaningfulPaint(swap_stamp);
}

void FirstMeaningfulPaintDetector::SetFirstMeaningfulPaint(
    base::TimeTicks swap_stamp) {
  DCHECK(paint_timing_->FirstMeaningfulPaint().is_null());
  DCHECK(!swap_stamp.is_null());
  DCHECK(network_quiet_reached_);

  double swap_time_seconds = swap_stamp.since_origin().InSecondsF();
  probe::PaintTiming(GetDocument(), "firstMeaningfulPaint", swap_time_seconds);

  // If there's only been one contentful paint, then there won't have been
  // a meaningful paint signalled to the Scheduler, so mark one now.
  // This is a no-op if a FMPC has already been marked.
  paint_timing_->SetFirstMeaningfulPaintCandidate(swap_stamp);

  paint_timing_->SetFirstMeaningfulPaint(
      swap_stamp, had_user_input_before_provisional_first_meaningful_paint_);
}

// static
void FirstMeaningfulPaintDetector::SetTickClockForTesting(
    const base::TickClock* clock) {
  g_clock = clock;
}

void FirstMeaningfulPaintDetector::Trace(blink::Visitor* visitor) {
  visitor->Trace(paint_timing_);
}

}  // namespace blink
