// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_

#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {
class PaintLayer;
class IntRect;
class LayoutObject;
class TracedValue;
class LocalFrameView;

struct TextRecord {
  DOMNodeId node_id = kInvalidDOMNodeId;
  double first_size = 0.0;
  base::TimeTicks first_paint_time = base::TimeTicks();
  String text = "";
};

// TextPaintTimingDetector contains Largest Text Paint and Last Text Paint.
//
// Largest Text Paint timing measures when the largest text element gets painted
// within viewport. Last Text Paint timing measures when the last text element
// gets painted within viewport. Specifically, they:
// 1. Tracks all texts' first invalidation, recording their visual size, paint
// time.
// 2. Every 1 second after the first text pre-paint, the algorithm starts an
// analysis. In the analysis:
// 2.1 Largest Text Paint finds the text with the
// largest first visual size, reports its first paint time as a candidate
// result.
// 2.2 Last Text Paint finds the text with the largest first paint time,
// report its first paint time as a candidate result.
//
// For all these candidate results, Telemetry picks the lastly reported
// Largest Text Paint candidate and Last Text Paint candidate respectively as
// their final result.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit#heading=h.lvno2v283uls
class CORE_EXPORT TextPaintTimingDetector final
    : public GarbageCollectedFinalized<TextPaintTimingDetector> {
  using ReportTimeCallback =
      WTF::CrossThreadFunction<void(WebLayerTreeView::SwapResult,
                                    base::TimeTicks)>;
  friend class TextPaintTimingDetectorTest;

 public:
  TextPaintTimingDetector(LocalFrameView* frame_view);
  void RecordText(const LayoutObject& object, const PaintLayer& painting_layer);
  TextRecord* FindLargestPaintCandidate();
  TextRecord* FindLastPaintCandidate();
  void OnPrePaintFinished();
  void NotifyNodeRemoved(DOMNodeId);
  void Dispose() { timer_.Stop(); }
  base::TimeTicks LargestTextPaint() const { return largest_text_paint_; }
  base::TimeTicks LastTextPaint() const { return last_text_paint_; }
  void Trace(blink::Visitor*);

 private:
  void PopulateTraceValue(TracedValue& value,
                          const TextRecord& first_text_paint,
                          unsigned candidate_index) const;
  IntRect CalculateTransformedRect(LayoutRect& visual_rect,
                                   const PaintLayer& painting_layer) const;
  void TimerFired(TimerBase*);
  void Analyze();

  void ReportSwapTime(WebLayerTreeView::SwapResult result,
                      base::TimeTicks timestamp);
  void RegisterNotifySwapTime(ReportTimeCallback callback);
  void OnLargestTextDetected(const TextRecord&);
  void OnLastTextDetected(const TextRecord&);

  HashSet<DOMNodeId> recorded_text_node_ids_;
  HashSet<DOMNodeId> size_zero_node_ids_;
  std::priority_queue<std::unique_ptr<TextRecord>,
                      std::vector<std::unique_ptr<TextRecord>>,
                      bool (*)(const std::unique_ptr<TextRecord>&,
                               const std::unique_ptr<TextRecord>&)>
      largest_text_heap_;
  std::priority_queue<std::unique_ptr<TextRecord>,
                      std::vector<std::unique_ptr<TextRecord>>,
                      bool (*)(const std::unique_ptr<TextRecord>&,
                               const std::unique_ptr<TextRecord>&)>
      latest_text_heap_;
  std::vector<TextRecord> texts_to_record_swap_time_;

  // Make sure that at most one swap promise is ongoing.
  bool awaiting_swap_promise_ = false;
  unsigned recorded_node_count_ = 0;
  unsigned largest_text_candidate_index_max_ = 0;
  unsigned last_text_candidate_index_max_ = 0;

  base::TimeTicks largest_text_paint_;
  base::TimeTicks last_text_paint_;
  TaskRunnerTimer<TextPaintTimingDetector> timer_;
  Member<LocalFrameView> frame_view_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TEXT_PAINT_TIMING_DETECTOR_H_
