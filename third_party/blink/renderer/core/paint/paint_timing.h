// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/paint/first_meaningful_paint_detector.h"
#include "third_party/blink/renderer/core/paint/paint_event.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace base {
class TickClock;
}

namespace blink {

class LocalFrame;

// PaintTiming is responsible for tracking paint-related timings for a given
// document.
class CORE_EXPORT PaintTiming final : public GarbageCollected<PaintTiming>,
                                      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(PaintTiming);
  friend class FirstMeaningfulPaintDetector;
  using ReportTimeCallback =
      WTF::CrossThreadOnceFunction<void(WebWidgetClient::SwapResult,
                                        base::TimeTicks)>;

 public:
  static const char kSupplementName[];

  explicit PaintTiming(Document&);
  virtual ~PaintTiming() = default;

  static PaintTiming& From(Document&);

  // Mark*() methods record the time for the given paint event and queue a swap
  // promise to record the |first_*_swap_| timestamp. These methods do nothing
  // (early return) if a time has already been recorded for the given paint
  // event.
  void MarkFirstPaint();

  // MarkFirstImagePaint, and MarkFirstContentfulPaint
  // will also record first paint if first paint hasn't been recorded yet.
  void MarkFirstContentfulPaint();

  // MarkFirstImagePaint will also record first contentful paint if first
  // contentful paint hasn't been recorded yet.
  void MarkFirstImagePaint();

  void SetFirstMeaningfulPaintCandidate(base::TimeTicks timestamp);
  void SetFirstMeaningfulPaint(
      base::TimeTicks swap_stamp,
      FirstMeaningfulPaintDetector::HadUserInput had_input);
  void NotifyPaint(bool is_first_paint, bool text_painted, bool image_painted);

  // The getters below return monotonically-increasing seconds, or zero if the
  // given paint event has not yet occurred. See the comments for
  // monotonicallyIncreasingTime in wtf/Time.h for additional details.

  // FirstPaint returns the first time that anything was painted for the
  // current document.
  base::TimeTicks FirstPaint() const { return first_paint_swap_; }

  // FirstContentfulPaint returns the first time that 'contentful' content was
  // painted. For instance, the first time that text or image content was
  // painted.
  base::TimeTicks FirstContentfulPaint() const {
    return first_contentful_paint_swap_;
  }

  // FirstImagePaint returns the first time that image content was painted.
  base::TimeTicks FirstImagePaint() const { return first_image_paint_swap_; }

  // FirstMeaningfulPaint returns the first time that page's primary content
  // was painted.
  base::TimeTicks FirstMeaningfulPaint() const {
    return first_meaningful_paint_swap_;
  }

  // FirstMeaningfulPaintCandidate indicates the first time we considered a
  // paint to qualify as the potentially first meaningful paint. Unlike
  // firstMeaningfulPaint, this signal is available in real time, but it may be
  // an optimistic (i.e., too early) estimate.
  base::TimeTicks FirstMeaningfulPaintCandidate() const {
    return first_meaningful_paint_candidate_;
  }

  FirstMeaningfulPaintDetector& GetFirstMeaningfulPaintDetector() {
    return *fmp_detector_;
  }

  void RegisterNotifySwapTime(PaintEvent, ReportTimeCallback);
  void ReportSwapTime(PaintEvent,
                      WebWidgetClient::SwapResult,
                      base::TimeTicks timestamp);

  void ReportSwapResultHistogram(WebWidgetClient::SwapResult);

  // The caller owns the |clock| which must outlive the PaintTiming.
  void SetTickClockForTesting(const base::TickClock* clock);

  void Trace(blink::Visitor*) override;

 private:
  LocalFrame* GetFrame() const;
  void NotifyPaintTimingChanged();

  // Set*() set the timing for the given paint event to the given timestamp if
  // the value is currently zero, and queue a swap promise to record the
  // |first_*_swap_| timestamp. These methods can be invoked from other Mark*()
  // or Set*() methods to make sure that first paint is marked as part of
  // marking first contentful paint, or that first contentful paint is marked as
  // part of marking first text/image paint, for example.
  void SetFirstPaint(base::TimeTicks stamp);

  // setFirstContentfulPaint will also set first paint time if first paint
  // time has not yet been recorded.
  void SetFirstContentfulPaint(base::TimeTicks stamp);

  // Set*Swap() are called when the SwapPromise is fulfilled and the swap
  // timestamp is available. These methods will record trace events, update Web
  // Perf API (FP and FCP only), and notify that paint timing has changed, which
  // triggers UMAs and UKMS.
  // |stamp| is the swap timestamp used for tracing, UMA, UKM, and Web Perf API.
  void SetFirstPaintSwap(base::TimeTicks stamp);
  void SetFirstContentfulPaintSwap(base::TimeTicks stamp);
  void SetFirstImagePaintSwap(base::TimeTicks stamp);

  void RegisterNotifySwapTime(PaintEvent);

  base::TimeTicks FirstPaintRendered() const { return first_paint_; }

  base::TimeTicks FirstContentfulPaintRendered() const {
    return first_contentful_paint_;
  }

  // TODO(crbug/738235): Non first_*_swap_ variables are only being tracked to
  // compute deltas for reporting histograms and should be removed once we
  // confirm the deltas and discrepancies look reasonable.
  base::TimeTicks first_paint_;
  base::TimeTicks first_paint_swap_;
  base::TimeTicks first_image_paint_;
  base::TimeTicks first_image_paint_swap_;
  base::TimeTicks first_contentful_paint_;
  base::TimeTicks first_contentful_paint_swap_;
  base::TimeTicks first_meaningful_paint_swap_;
  base::TimeTicks first_meaningful_paint_candidate_;

  Member<FirstMeaningfulPaintDetector> fmp_detector_;

  const base::TickClock* clock_;

  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest, NoFirstPaint);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest, OneLayout);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           TwoLayoutsSignificantSecond);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           TwoLayoutsSignificantFirst);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           FirstMeaningfulPaintCandidate);
  FRIEND_TEST_ALL_PREFIXES(
      FirstMeaningfulPaintDetectorTest,
      OnlyOneFirstMeaningfulPaintCandidateBeforeNetworkStable);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           NetworkStableBeforeFirstContentfulPaint);
  FRIEND_TEST_ALL_PREFIXES(
      FirstMeaningfulPaintDetectorTest,
      FirstMeaningfulPaintShouldNotBeBeforeFirstContentfulPaint);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           Network2QuietThen0Quiet);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           Network0QuietThen2Quiet);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           Network0QuietTimer);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           Network2QuietTimer);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           FirstMeaningfulPaintAfterUserInteraction);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           UserInteractionBeforeFirstPaint);
  FRIEND_TEST_ALL_PREFIXES(
      FirstMeaningfulPaintDetectorTest,
      WaitForSingleOutstandingSwapPromiseAfterNetworkStable);
  FRIEND_TEST_ALL_PREFIXES(
      FirstMeaningfulPaintDetectorTest,
      WaitForMultipleOutstandingSwapPromisesAfterNetworkStable);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           WaitForFirstContentfulPaintSwapAfterNetworkStable);
  FRIEND_TEST_ALL_PREFIXES(
      FirstMeaningfulPaintDetectorTest,
      ProvisionalTimestampChangesAfterNetworkQuietWithOutstandingSwapPromise);

  DISALLOW_COPY_AND_ASSIGN(PaintTiming);
};

}  // namespace blink

#endif
