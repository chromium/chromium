// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_offset.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScrollTimelineOptions;
class WorkletAnimationBase;

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
    kBlock,
    kInline,
    kHorizontal,
    kVertical,
  };

  static ScrollTimeline* Create(Document&,
                                ScrollTimelineOptions*,
                                ExceptionState&);

  ScrollTimeline(Document*,
                 absl::optional<Element*> source,
                 ScrollDirection,
                 HeapVector<Member<ScrollTimelineOffset>>);

  bool IsScrollTimeline() const override { return true; }
  // ScrollTimeline is not active if source is null, does not currently
  // have a CSS layout box, or if its layout box is not a scroll container.
  // https://github.com/WICG/scroll-animations/issues/31
  bool IsActive() const override;
  absl::optional<base::TimeDelta> InitialStartTimeForAnimations() override;
  AnimationTimeDelta CalculateIntrinsicIterationDuration(
      const Timing&) override;
  AnimationTimeDelta ZeroTime() override { return AnimationTimeDelta(); }

  void ServiceAnimations(TimingUpdateReason) override;
  void ScheduleNextService() override;

  // IDL API implementation.
  Element* source() const;
  String orientation();
  const HeapVector<Member<V8ScrollTimelineOffset>> scrollOffsets() const;

  V8CSSNumberish* currentTime() override;
  V8CSSNumberish* duration() override;
  V8CSSNumberish* ConvertTimeToProgress(AnimationTimeDelta time) const;

  // Returns the Node that should actually have the ScrollableArea (if one
  // exists). This can differ from |source| when |source_| is the
  // Document's scrollingElement, and it may be null if the document was
  // removed before the ScrollTimeline was created.
  Node* ResolvedSource() const { return resolved_source_; }

  // Return the latest resolved scroll offsets. This will be empty when
  // timeline is inactive.
  const std::vector<double> GetResolvedScrollOffsets() const;

  ScrollDirection GetOrientation() const { return orientation_; }

  void GetCurrentAndMaxOffset(const LayoutBox*,
                              double& current_offset,
                              double& max_offset) const;
  // Invalidates scroll timeline as a result of scroller properties change.
  // This may lead the timeline to request a new animation frame.
  virtual void Invalidate();

  // Mark every effect target of every Animation attached to this timeline
  // for style recalc.
  void InvalidateEffectTargetStyle();

  // See DocumentAnimations::ValidateTimelines
  void ValidateState();

  CompositorAnimationTimeline* EnsureCompositorTimeline() override;
  void UpdateCompositorTimeline() override;

  // TODO(crbug.com/896249): This method is temporary and currently required
  // to support worklet animations. Once worklet animations become animations
  // these methods will not be longer needed. They are used to keep track of
  // the of worklet animations attached to the scroll timeline for updating
  // compositing state.
  void WorkletAnimationAttached(WorkletAnimationBase*);

  void AnimationAttached(Animation*) override;
  void AnimationDetached(Animation*) override;

  void Trace(Visitor*) const override;

  // Invalidates scroll timelines with a given scroller node.
  // Called when scroller properties, affecting scroll timeline state, change.
  // These properties are scroller offset, content size, viewport size,
  // overflow, adding and removal of scrollable area.
  static void Invalidate(Node* node);

  // A change in the compositing state of a ScrollTimeline's scroll source
  // can cause the compositor's view of the scroll source to become out of
  // date. We inform the WorkletAnimationController about any such changes
  // so that it can schedule a compositing animations update.
  static void InvalidateCompositingState(Node* node);

  // Duration is the maximum value a timeline may generate for current time.
  // Used to convert time values to proportional values.
  absl::optional<AnimationTimeDelta> GetDuration() const override {
    // Any arbitrary value should be able to be used here.
    return absl::make_optional(ANIMATION_TIME_DELTA_FROM_SECONDS(100));
  }

 protected:
  PhaseAndTime CurrentPhaseAndTime() override;
  bool ScrollOffsetsEqual(
      const HeapVector<Member<ScrollTimelineOffset>>& other) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ScrollTimelineTest, MultipleScrollOffsetsClamping);
  FRIEND_TEST_ALL_PREFIXES(ScrollTimelineTest, ResolveScrollOffsets);
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
  bool ResolveScrollOffsets(WTF::Vector<double>& resolved_offsets) const;

  struct TimelineState {
    TimelinePhase phase;
    absl::optional<base::TimeDelta> current_time;
    // The resolved version of scroll offset. The vector is empty
    // when timeline is inactive (e.g., when source does not overflow).
    WTF::Vector<double> scroll_offsets;

    bool operator==(const TimelineState& other) const {
      return phase == other.phase && current_time == other.current_time &&
             scroll_offsets == other.scroll_offsets;
    }
  };

  TimelineState ComputeTimelineState() const;
  ScrollTimelineOffset* StartScrollOffset() const;
  ScrollTimelineOffset* EndScrollOffset() const;

  // Use time_check true to request next service if time has changed.
  // false - regardless of time change.
  void ScheduleNextServiceInternal(bool time_check);

  // Use |source_| only to implement the web-exposed API but use
  // resolved_source_ to actually access the scroll related properties.
  Member<Element> source_;
  Member<Node> resolved_source_;
  ScrollDirection orientation_;
  HeapVector<Member<ScrollTimelineOffset>> scroll_offsets_;

  // Snapshotted value produced by the last SnapshotState call.
  TimelineState timeline_state_snapshotted_;

  HeapHashSet<WeakMember<WorkletAnimationBase>> attached_worklet_animations_;
};

template <>
struct DowncastTraits<ScrollTimeline> {
  static bool AllowFrom(const AnimationTimeline& value) {
    return value.IsScrollTimeline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_
