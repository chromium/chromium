// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_

#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_offset.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DoubleOrScrollTimelineAutoKeyword;
class ScrollTimelineOptions;

// Implements the ScrollTimeline concept from the Scroll-linked Animations spec.
//
// A ScrollTimeline is a special form of AnimationTimeline whose time values are
// not determined by wall-clock time but instead the progress of scrolling in a
// scroll container. The user is able to specify which scroll container to
// track, the direction of scroll they care about, and various attributes to
// control the conversion of scroll amount to time output.
//
// Spec: https://wicg.github.io/scroll-animations/#scroll-timelines
class CORE_EXPORT ScrollTimeline : public AnimationTimeline {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum ScrollDirection {
    Block,
    Inline,
    Horizontal,
    Vertical,
  };

  static ScrollTimeline* Create(Document&,
                                ScrollTimelineOptions*,
                                ExceptionState&);

  ScrollTimeline(Document*,
                 Element*,
                 ScrollDirection,
                 ScrollTimelineOffset*,
                 ScrollTimelineOffset*,
                 double);

  // AnimationTimeline implementation.
  bool IsScrollTimeline() const override { return true; }
  // ScrollTimeline is not active if scrollSource is null, does not currently
  // have a CSS layout box, or if its layout box is not a scroll container.
  // https://github.com/WICG/scroll-animations/issues/31
  bool IsActive() const override;
  base::Optional<base::TimeDelta> InitialStartTimeForAnimations() override;
  double ZeroTimeInSeconds() override { return 0; }

  void ServiceAnimations(TimingUpdateReason) override;
  void ScheduleNextService() override;

  // IDL API implementation.
  Element* scrollSource();
  String orientation();
  void startScrollOffset(
      StringOrScrollTimelineElementBasedOffset& result) const;
  void endScrollOffset(StringOrScrollTimelineElementBasedOffset& result) const;

  void timeRange(DoubleOrScrollTimelineAutoKeyword&);

  // Returns the Node that should actually have the ScrollableArea (if one
  // exists). This can differ from |scrollSource| when |scroll_source_| is the
  // Document's scrollingElement, and it may be null if the document was
  // removed before the ScrollTimeline was created.
  Node* ResolvedScrollSource() const { return resolved_scroll_source_; }

  // Return the latest resolved start scroll offset. This will be nullopt when
  // timeline is inactive.
  base::Optional<double> GetResolvedStartScrollOffset() const {
    return timeline_state_snapshotted_.start_offset;
  }

  // Return the latest resolved end scroll offset. This will be nullopt when
  // timeline is inactive.
  base::Optional<double> GetResolvedEndScrollOffset() const {
    return timeline_state_snapshotted_.end_offset;
  }

  ScrollDirection GetOrientation() const { return orientation_; }

  void GetCurrentAndMaxOffset(const LayoutBox*,
                              double& current_offset,
                              double& max_offset) const;
  // Invalidates scroll timeline as a result of scroller properties change.
  // This may lead the timeline to request a new animation frame.
  virtual void Invalidate();

  CompositorAnimationTimeline* EnsureCompositorTimeline() override;
  void UpdateCompositorTimeline() override;

  // TODO(crbug.com/896249): These methods are temporary and currently required
  // to support worklet animations. Once worklet animations become animations
  // these methods will not be longer needed. They are used to keep track of
  // number of worklet animations attached to the scroll timeline for updating
  // compositing state.
  void WorkletAnimationAttached();
  void WorkletAnimationDetached();

  void AnimationAttached(Animation*) override;
  void AnimationDetached(Animation*) override;

  void Trace(Visitor*) const override;

  static bool HasActiveScrollTimeline(Node* node);
  // Invalidates scroll timelines with a given scroller node.
  // Called when scroller properties, affecting scroll timeline state, change.
  // These properties are scroller offset, content size, viewport size,
  // overflow, adding and removal of scrollable area.
  static void Invalidate(Node* node);

 protected:
  PhaseAndTime CurrentPhaseAndTime() override;

 private:
  // https://wicg.github.io/scroll-animations/#avoiding-cycles
  // Snapshots scroll timeline current time and phase.
  // Called once per animation frame.
  void SnapshotState();
  bool ComputeIsActive() const;
  PhaseAndTime ComputeCurrentPhaseAndTime() const;

  // Resolve scroll offsets The resolution process turns length-based values
  // into concrete length values resolving percentages and zoom factor. For
  // element-based values it computes the corresponding length value that maps
  // to the particular element intersection. See
  // |ScrollTimelineOffset::ResolveOffset()| for more details.
  std::tuple<base::Optional<double>, base::Optional<double>>
  ResolveScrollOffsets() const;

  struct TimelineState {
    TimelinePhase phase;
    base::Optional<base::TimeDelta> current_time;
    // The resolved version of start and end offset. These values are nullopts
    // when timeline is inactive (e.g., when source does not overflow).
    base::Optional<double> start_offset;
    base::Optional<double> end_offset;

    bool operator==(const TimelineState& other) const {
      return phase == other.phase && current_time == other.current_time &&
             start_offset == other.start_offset &&
             end_offset == other.end_offset;
    }
  };

  TimelineState ComputeTimelineState() const;

  // Use time_check true to request next service if time has changed.
  // false - regardless of time change.
  void ScheduleNextServiceInternal(bool time_check);

  // Use |scroll_source_| only to implement the web-exposed API but use
  // resolved_scroll_source_ to actually access the scroll related properties.
  Member<Element> scroll_source_;
  Member<Node> resolved_scroll_source_;
  ScrollDirection orientation_;

  // These define the total range of the scroller that the ScrollTimeline is
  // active within.
  Member<ScrollTimelineOffset> start_scroll_offset_;
  Member<ScrollTimelineOffset> end_scroll_offset_;

  double time_range_;

  // Snapshotted value produced by the last SnapshotState call.
  TimelineState timeline_state_snapshotted_;

  // The only purpose of scroll_animations_ is keeping strong references to
  // attached animations. This is required to keep attached animations alive
  // as long as the timeline is alive. Scroll timeline is alive as long as its
  // scroller is alive.
  HeapHashSet<Member<Animation>> scroll_animations_;
};

template <>
struct DowncastTraits<ScrollTimeline> {
  static bool AllowFrom(const AnimationTimeline& value) {
    return value.IsScrollTimeline();
  }
};

}  // namespace blink

#endif
