// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_SHIFT_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_SHIFT_TRACKER_H_

#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_shift_region.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/geometry/region.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class IntRect;
class LayoutObject;
class LocalFrameView;
class PropertyTreeState;
class TracedValue;
class WebInputEvent;

// Tracks "layout shifts" from layout objects changing their visual location
// between animation frames. See https://github.com/WICG/layout-instability.
class CORE_EXPORT LayoutShiftTracker {
  USING_FAST_MALLOC(LayoutShiftTracker);

 public:
  LayoutShiftTracker(LocalFrameView*);
  ~LayoutShiftTracker() {}
  // |paint_offset_diff| is an additional amount by which the paint offset
  // shifted that is not tracked in visual rects. Visual rects are in the
  // local transform space of the LayoutObject. Any time the transform space is
  // changed, the offset of that rect to the "origin" is reset. This offset
  // is also known as the paint offset.
  // In cases where we can communicate paint offset diffs across transform
  // space change boundaries, |paint_offset_diff| is how to do it. In
  // particular, many transform spaces are artificial and are used as an
  // implementation detail of compositing to make it easier to isolate state for
  // composited layers. We can easily pass the paint offset diff across such
  // boundaries.
  void NotifyObjectPrePaint(const LayoutObject& object,
                            const PropertyTreeState& property_tree_state,
                            const IntRect& old_visual_rect,
                            const IntRect& new_visual_rect,
                            FloatSize paint_offset_delta);
  void NotifyPrePaintFinished();
  void NotifyInput(const WebInputEvent&);
  void NotifyScroll(ScrollType, ScrollOffset delta);
  void NotifyViewportSizeChanged();
  bool IsActive();
  double Score() const { return score_; }
  double WeightedScore() const { return weighted_score_; }
  float OverallMaxDistance() const { return overall_max_distance_; }
  bool ObservedInputOrScroll() const { return observed_input_or_scroll_; }
  void Dispose() { timer_.Stop(); }
  base::TimeTicks MostRecentInputTimestamp() {
    return most_recent_input_timestamp_;
  }

 private:
  void ObjectShifted(const LayoutObject&,
                     const PropertyTreeState&,
                     FloatRect old_rect,
                     FloatRect new_rect,
                     FloatSize paint_offset_diff);
  void ReportShift(double score_delta, double weighted_score_delta);
  void TimerFired(TimerBase*) {}
  std::unique_ptr<TracedValue> PerFrameTraceData(double score_delta,
                                                 bool input_detected) const;
  double SubframeWeightingFactor() const;
  void SetLayoutShiftRects(const Vector<IntRect>& int_rects);
  void UpdateInputTimestamp(base::TimeTicks timestamp);

  // This owns us.
  UntracedMember<LocalFrameView> frame_view_;

  // The document cumulative layout shift (DCLS) score for this LocalFrame,
  // unweighted, with move distance applied.
  double score_;

  // The cumulative layout shift score for this LocalFrame, with each increase
  // weighted by the extent to which the LocalFrame visibly occupied the main
  // frame at the time the shift occurred, e.g. x0.5 if the subframe occupied
  // half of the main frame's reported size; see SubframeWeightingFactor().
  double weighted_score_;

  // Stores information related to buffering layout shifts after pointerdown.
  // We accumulate score deltas in this object until we know whether the
  // pointerdown should be treated as a tap (triggering layout shift exclusion)
  // or a scroll (not triggering layout shift exclusion).  Once the correct
  // treatment is known, the pending layout shifts are reported appropriately
  // and the PointerdownPendingData object is reset.
  struct PointerdownPendingData {
    PointerdownPendingData()
        : saw_pointerdown(false), score_delta(0), weighted_score_delta(0) {}
    bool saw_pointerdown;
    double score_delta;
    double weighted_score_delta;
  };

  PointerdownPendingData pointerdown_pending_data_;

  // The per-animation-frame impact region.
  LayoutShiftRegion region_;

  // Tracks the short period after an input event during which we ignore shifts
  // for the purpose of cumulative scoring, and report them to the web perf API
  // with hadRecentInput == true.
  TaskRunnerTimer<LayoutShiftTracker> timer_;

  // The maximum distance any layout object has moved in the current animation
  // frame.
  float frame_max_distance_;

  // The maximum distance any layout object has moved, across all animation
  // frames.
  float overall_max_distance_;

  // Sum of all scroll deltas that occurred in the current animation frame.
  ScrollOffset frame_scroll_delta_;

  // Whether either a user input or document scroll have been observed during
  // the session. (This is only tracked so UkmPageLoadMetricsObserver to report
  // LayoutInstability.CumulativeShiftScore.MainFrame.BeforeInputOrScroll. It's
  // not related to input exclusion or the LayoutShift::had_recent_input_ bit.)
  bool observed_input_or_scroll_;

  // Most recent timestamp of a user input event that has been observed.
  // User input includes window resizing but not scrolling.
  base::TimeTicks most_recent_input_timestamp_;
  bool most_recent_input_timestamp_initialized_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_SHIFT_TRACKER_H_
