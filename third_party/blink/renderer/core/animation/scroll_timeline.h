// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_SCROLL_TIMELINE_H_

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "cc/animation/scroll_timeline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_axis.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/scroll/scroll_snapshot_client.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class PaintLayerScrollableArea;
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
class CORE_EXPORT ScrollTimeline : public AnimationTimeline,
                                   public ScrollSnapshotClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using ScrollOffsets = cc::ScrollTimeline::ScrollOffsets;
  using ScrollAxis = V8ScrollAxis::Enum;

  // Indicates the relation between the reference element and source of the
  // scroll timeline.
  enum class ReferenceType {
    kSource,          // The reference element matches the source.
    kNearestAncestor  // The source is the nearest scrollable ancestor to the
                      // reference element.
  };

  static ScrollTimeline* Create(Document&,
                                ScrollTimelineOptions*,
                                ExceptionState&);

  static ScrollTimeline* Create(Document* document,
                                Element* source,
                                ScrollAxis axis);

  // Construct ScrollTimeline objects through one of the Create methods, which
  // perform initial snapshots, as it can't be done during the constructor due
  // to possibly depending on overloaded functions.
  ScrollTimeline(Document*,
                 ReferenceType reference_type,
                 Element* reference,
                 ScrollAxis axis);

  bool IsScrollTimeline() const override { return true; }
  // ScrollTimeline is not active if source is null, does not currently
  // have a CSS layout box, or if its layout box is not a scroll container.
  // https://github.com/WICG/scroll-animations/issues/31
  bool IsActive() const override;
  absl::optional<base::TimeDelta> InitialStartTimeForAnimations() override;
  AnimationTimeDelta CalculateIntrinsicIterationDuration(
      const Animation*,
      const Timing&) override;
  AnimationTimeDelta ZeroTime() override { return AnimationTimeDelta(); }

  void ServiceAnimations(TimingUpdateReason) override;
  void ScheduleNextService() override;

  // IDL API implementation.
  Element* source() const;
  const V8ScrollAxis axis() const { return V8ScrollAxis(axis_); }

  V8CSSNumberish* currentTime() override;
  V8CSSNumberish* duration() override;
  V8CSSNumberish* ConvertTimeToProgress(AnimationTimeDelta time) const;

  // Returns the Node that should actually have the ScrollableArea (if one
  // exists). This can differ from |source| when defaulting to the
  // Document's scrollingElement, and it may be null if the document was
  // removed before the ScrollTimeline was created.
  Node* ResolvedSource() const { return resolved_source_; }

  // Return the latest resolved scroll offsets. This will be empty when
  // timeline is inactive.
  absl::optional<ScrollOffsets> GetResolvedScrollOffsets() const;

  ScrollAxis GetAxis() const { return axis_; }

  void GetCurrentAndMaxOffset(const LayoutBox*,
                              double& current_offset,
                              double& max_offset) const;

  // Mark every effect target of every Animation attached to this timeline
  // for style recalc.
  void InvalidateEffectTargetStyle();

  cc::AnimationTimeline* EnsureCompositorTimeline() override;
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

  // Duration is the maximum value a timeline may generate for current time.
  // Used to convert time values to proportional values.
  absl::optional<AnimationTimeDelta> GetDuration() const override {
    // Any arbitrary value should be able to be used here.
    return absl::make_optional(ANIMATION_TIME_DELTA_FROM_SECONDS(100));
  }

 protected:
  PhaseAndTime CurrentPhaseAndTime() override;

  virtual Element* ReferenceElement() const { return reference_element_.Get(); }

  // Determines the source for the scroll timeline. It may be the reference
  // element or its nearest scrollable ancestor, depending on |souce_type|.
  // This version does not force a style update and is therefore safe to call
  // during lifecycle update.
  Element* SourceInternal() const;

  ReferenceType GetReferenceType() const { return reference_type_; }

  bool HasExplicitSource() const {
    return reference_type_ == ReferenceType::kSource;
  }

  void UpdateResolvedSource();

  // Scroll offsets corresponding to 0% and 100% progress. By default, these
  // correspond to the scroll range of the container.
  virtual absl::optional<ScrollOffsets> CalculateOffsets(
      PaintLayerScrollableArea* scrollable_area,
      ScrollOrientation physical_orientation) const;

  // ScrollSnapshotClient:
  void UpdateSnapshot() override;
  bool ValidateSnapshot() override;
  bool ShouldScheduleNextService() override;

  // https://wicg.github.io/scroll-animations/#avoiding-cycles
  // Snapshots scroll timeline current time and phase.
  // Called once per animation frame.
  bool ComputeIsActive() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ScrollTimelineTest, MultipleScrollOffsetsClamping);
  FRIEND_TEST_ALL_PREFIXES(ScrollTimelineTest, ResolveScrollOffsets);

  struct TimelineState {
    // TODO(crbug.com/1338167): Remove phase as it can be inferred from
    // current_time.
    TimelinePhase phase = TimelinePhase::kInactive;
    absl::optional<base::TimeDelta> current_time;
    absl::optional<ScrollOffsets> scroll_offsets;

    bool operator==(const TimelineState& other) const {
      return phase == other.phase && current_time == other.current_time &&
             scroll_offsets == other.scroll_offsets;
    }
  };

  TimelineState ComputeTimelineState();

  ReferenceType reference_type_;
  Member<Element> reference_element_;
  Member<Node> resolved_source_;
  ScrollAxis axis_;

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
