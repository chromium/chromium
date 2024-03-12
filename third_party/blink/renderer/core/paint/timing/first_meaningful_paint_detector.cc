// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/first_meaningful_paint_detector.h"

#include "base/time/default_tick_clock.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
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
  provisional_first_meaningful_paint_presentation_ = base::TimeTicks();
  RegisterNotifyPresentationTime(PaintEvent::kProvisionalFirstMeaningfulPaint);
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
      paint_timing_
          ->FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime()
          .is_null())
    return;
  network_quiet_reached_ = true;

  if (!provisional_first_meaningful_paint_.is_null()) {
    base::TimeTicks first_meaningful_paint_presentation;
    // Enforce FirstContentfulPaint <= FirstMeaningfulPaint.
    if (provisional_first_meaningful_paint_ <
        paint_timing_
            ->FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime()) {
      first_meaningful_paint_ =
          paint_timing_
              ->FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime();
      first_meaningful_paint_presentation =
          paint_timing_->FirstContentfulPaintIgnoringSoftNavigations();
      // It's possible that this timer fires between when the first contentful
      // paint is set and its presentation promise is fulfilled. If this
      // happens, defer until NotifyFirstContentfulPaint() is called.
      if (first_meaningful_paint_presentation.is_null())
        defer_first_meaningful_paint_ = kDeferFirstContentfulPaintNotSet;
    } else {
      first_meaningful_paint_ = provisional_first_meaningful_paint_;
      first_meaningful_paint_presentation =
          provisional_first_meaningful_paint_presentation_;
      // We might still be waiting for one or more presentation promises, in
      // which case we want to defer reporting first meaningful paint until they
      // complete. Otherwise, we would either report the wrong presentation
      // timestamp or none at all.
      if (outstanding_presentation_promise_count_ > 0)
        defer_first_meaningful_paint_ = kDeferOutstandingPresentationPromises;
    }
    if (defer_first_meaningful_paint_ == kDoNotDefer) {
      // Report FirstMeaningfulPaint when the page reached network 2-quiet if
      // we aren't waiting for a presentation timestamp.
      SetFirstMeaningfulPaint(first_meaningful_paint_presentation);
    }
  }
}

bool FirstMeaningfulPaintDetector::SeenFirstMeaningfulPaint() const {
  return !first_meaningful_paint_.is_null();
}

void FirstMeaningfulPaintDetector::RegisterNotifyPresentationTime(
    PaintEvent event) {
  ++outstanding_presentation_promise_count_;
  paint_timing_->RegisterNotifyPresentationTime(
      CrossThreadBindOnce(&FirstMeaningfulPaintDetector::ReportPresentationTime,
                          WrapCrossThreadWeakPersistent(this), event));
}

void FirstMeaningfulPaintDetector::ReportPresentationTime(
    PaintEvent event,
    const viz::FrameTimingDetails& presentation_details) {
  base::TimeTicks timestamp =
      presentation_details.presentation_feedback.timestamp;
  DCHECK(event == PaintEvent::kProvisionalFirstMeaningfulPaint);
  DCHECK_GT(outstanding_presentation_promise_count_, 0U);
  --outstanding_presentation_promise_count_;

  provisional_first_meaningful_paint_presentation_ = timestamp;

  probe::PaintTiming(GetDocument(), "firstMeaningfulPaintCandidate",
                     timestamp.since_origin().InSecondsF());

  // Ignore the first meaningful paint candidate as this generally is the first
  // contentful paint itself.
  if (!seen_first_meaningful_paint_candidate_) {
    seen_first_meaningful_paint_candidate_ = true;
  } else {
    paint_timing_->SetFirstMeaningfulPaintCandidate(
        provisional_first_meaningful_paint_presentation_);
  }

  if (defer_first_meaningful_paint_ == kDeferOutstandingPresentationPromises &&
      outstanding_presentation_promise_count_ == 0) {
    DCHECK(!first_meaningful_paint_.is_null());
    SetFirstMeaningfulPaint(provisional_first_meaningful_paint_presentation_);
  }
}

void FirstMeaningfulPaintDetector::NotifyFirstContentfulPaint(
    base::TimeTicks presentation_time) {
  if (defer_first_meaningful_paint_ != kDeferFirstContentfulPaintNotSet)
    return;
  SetFirstMeaningfulPaint(presentation_time);
}

void FirstMeaningfulPaintDetector::SetFirstMeaningfulPaint(
    base::TimeTicks presentation_time) {
  DCHECK(paint_timing_->FirstMeaningfulPaint().is_null());
  DCHECK(!presentation_time.is_null());
  DCHECK(network_quiet_reached_);

  double presentation_time_seconds =
      presentation_time.since_origin().InSecondsF();
  probe::PaintTiming(GetDocument(), "firstMeaningfulPaint",
                     presentation_time_seconds);

  // If there's only been one contentful paint, then there won't have been
  // a meaningful paint signalled to the Scheduler, so mark one now.
  // This is a no-op if a FMPC has already been marked.
  paint_timing_->SetFirstMeaningfulPaintCandidate(presentation_time);

  paint_timing_->SetFirstMeaningfulPaint(
      presentation_time,
      had_user_input_before_provisional_first_meaningful_paint_);
}

// static
void FirstMeaningfulPaintDetector::SetTickClockForTesting(
    const base::TickClock* clock) {
  g_clock = clock;
}

void FirstMeaningfulPaintDetector::Trace(Visitor* visitor) const {
  visitor->Trace(paint_timing_);
}

}  // namespace blink
