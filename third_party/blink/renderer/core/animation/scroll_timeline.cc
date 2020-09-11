// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

#include <tuple>

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_offset.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {
using ScrollTimelineSet =
    HeapHashMap<WeakMember<Node>,
                Member<HeapHashSet<WeakMember<ScrollTimeline>>>>;
ScrollTimelineSet& GetScrollTimelineSet() {
  DEFINE_STATIC_LOCAL(Persistent<ScrollTimelineSet>, set,
                      (MakeGarbageCollected<ScrollTimelineSet>()));
  return *set;
}

using ActiveScrollTimelineSet = HeapHashCountedSet<WeakMember<Node>>;
ActiveScrollTimelineSet& GetActiveScrollTimelineSet() {
  DEFINE_STATIC_LOCAL(Persistent<ActiveScrollTimelineSet>, set,
                      (MakeGarbageCollected<ActiveScrollTimelineSet>()));
  return *set;
}

bool StringToScrollDirection(String scroll_direction,
                             ScrollTimeline::ScrollDirection& result) {
  if (scroll_direction == "block") {
    result = ScrollTimeline::Block;
    return true;
  }
  if (scroll_direction == "inline") {
    result = ScrollTimeline::Inline;
    return true;
  }
  if (scroll_direction == "horizontal") {
    result = ScrollTimeline::Horizontal;
    return true;
  }
  if (scroll_direction == "vertical") {
    result = ScrollTimeline::Vertical;
    return true;
  }
  return false;
}

ScrollOrientation ToPhysicalScrollOrientation(
    ScrollTimeline::ScrollDirection direction,
    const LayoutBox& source_box) {
  bool is_horizontal = source_box.IsHorizontalWritingMode();
  switch (direction) {
    case ScrollTimeline::Block:
      return is_horizontal ? kVerticalScroll : kHorizontalScroll;
    case ScrollTimeline::Inline:
      return is_horizontal ? kHorizontalScroll : kVerticalScroll;
    case ScrollTimeline::Horizontal:
      return kHorizontalScroll;
    case ScrollTimeline::Vertical:
      return kVerticalScroll;
  }
}

// Note that the resolution process may trigger document lifecycle to clean
// style and layout.
Node* ResolveScrollSource(Element* scroll_source) {
  // When in quirks mode we need the style to be clean, so we don't use
  // |ScrollingElementNoLayout|.
  if (scroll_source &&
      scroll_source == scroll_source->GetDocument().scrollingElement()) {
    return &scroll_source->GetDocument();
  }
  return scroll_source;
}
}  // namespace

ScrollTimeline* ScrollTimeline::Create(Document& document,
                                       ScrollTimelineOptions* options,
                                       ExceptionState& exception_state) {
  Element* scroll_source = options->hasScrollSource()
                               ? options->scrollSource()
                               : document.scrollingElement();

  ScrollDirection orientation;
  if (!StringToScrollDirection(options->orientation(), orientation)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid orientation");
    return nullptr;
  }

  ScrollTimelineOffset* start_scroll_offset =
      ScrollTimelineOffset::Create(options->startScrollOffset());
  if (!start_scroll_offset) {
    exception_state.ThrowTypeError("Invalid start offset.");
    return nullptr;
  }

  ScrollTimelineOffset* end_scroll_offset =
      ScrollTimelineOffset::Create(options->endScrollOffset());
  if (!end_scroll_offset) {
    exception_state.ThrowTypeError("Invalid end offset");
    return nullptr;
  }

  // TODO(crbug.com/1094014): Either scroll offsets or start/end offsets can
  // be specified.
  if (!options->scrollOffsets().IsEmpty() &&
      (!start_scroll_offset->IsDefaultValue() ||
       !end_scroll_offset->IsDefaultValue())) {
    exception_state.ThrowTypeError(
        "Either scrollOffsets or start/end offsets can be specified.");
    return nullptr;
  }

  HeapVector<Member<ScrollTimelineOffset>>* scroll_offsets =
      MakeGarbageCollected<HeapVector<Member<ScrollTimelineOffset>>>();
  if (options->scrollOffsets().IsEmpty()) {
    // TODO(crbug.com/1094014): scroll_offsets will replace start and end
    // offsets once spec decision on multiple scroll offsets is finalized.
    // https://github.com/w3c/csswg-drafts/issues/4912
    if (!start_scroll_offset->IsDefaultValue())
      scroll_offsets->push_back(start_scroll_offset);
    if (!end_scroll_offset->IsDefaultValue() ||
        !start_scroll_offset->IsDefaultValue())
      scroll_offsets->push_back(end_scroll_offset);
  } else {
    for (auto& offset : options->scrollOffsets()) {
      ScrollTimelineOffset* scroll_offset =
          ScrollTimelineOffset::Create(offset);
      if (!scroll_offset) {
        exception_state.ThrowTypeError("Invalid scroll offset");
        return nullptr;
      }
      if (scroll_offset->IsDefaultValue() &&
          (options->scrollOffsets().size() == 1 ||
           (scroll_offsets->size() + 1) < options->scrollOffsets().size())) {
        exception_state.ThrowTypeError(
            "Invalid scrollOffsets: 'auto' can only be set as an end "
            "offset when start offset presents.");
        return nullptr;
      }
      scroll_offsets->push_back(scroll_offset);
    }
  }

  // TODO(crbug.com/1097041): Support 'auto' value.
  if (options->timeRange().IsScrollTimelineAutoKeyword()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "'auto' value for timeRange not yet supported");
    return nullptr;
  }

  return MakeGarbageCollected<ScrollTimeline>(
      &document, scroll_source, orientation, scroll_offsets,
      options->timeRange().GetAsDouble());
}

ScrollTimeline::ScrollTimeline(
    Document* document,
    Element* scroll_source,
    ScrollDirection orientation,
    HeapVector<Member<ScrollTimelineOffset>>* scroll_offsets,
    double time_range)
    : AnimationTimeline(document),
      scroll_source_(scroll_source),
      resolved_scroll_source_(ResolveScrollSource(scroll_source_)),
      orientation_(orientation),
      scroll_offsets_(scroll_offsets),
      time_range_(time_range) {
  if (resolved_scroll_source_) {
    ScrollTimelineSet& set = GetScrollTimelineSet();
    if (!set.Contains(resolved_scroll_source_)) {
      set.insert(
          resolved_scroll_source_,
          MakeGarbageCollected<HeapHashSet<WeakMember<ScrollTimeline>>>());
    }
    auto it = set.find(resolved_scroll_source_);
    it->value->insert(this);
  }
  SnapshotState();
}

bool ScrollTimeline::IsActive() const {
  return timeline_state_snapshotted_.phase != TimelinePhase::kInactive;
}

void ScrollTimeline::Invalidate() {
  ScheduleNextServiceInternal(/* time_check = */ false);
}

bool ScrollTimeline::ComputeIsActive() const {
  LayoutBox* layout_box = resolved_scroll_source_
                              ? resolved_scroll_source_->GetLayoutBox()
                              : nullptr;
  return layout_box && layout_box->HasNonVisibleOverflow() &&
         layout_box->GetScrollableArea();
}

ScrollTimelineOffset* ScrollTimeline::StartScrollOffset() const {
  // Single entry offset in scrollOffsets is considered as 'end'. Thus,
  // resolving start offset only if there is at least 2 offsets.
  return scroll_offsets_ && scroll_offsets_->size() >= 2
             ? scroll_offsets_->at(0)
             : nullptr;
}
ScrollTimelineOffset* ScrollTimeline::EndScrollOffset() const {
  // End offset is always the last offset in scrollOffsets if exists.
  return scroll_offsets_ && scroll_offsets_->size() >= 1
             ? scroll_offsets_->at(scroll_offsets_->size() - 1)
             : nullptr;
}

const std::vector<double> ScrollTimeline::GetResolvedScrollOffsets() const {
  std::vector<double> resolved_offsets;
  for (const auto& offset : timeline_state_snapshotted_.scroll_offsets)
    resolved_offsets.push_back(offset);
  return resolved_offsets;
}

// Resolves scroll offsets and stores them into resolved_offsets argument.
// Returns true if the offsets are resolved.
bool ScrollTimeline::ResolveScrollOffsets(
    WTF::Vector<double>& resolved_offsets) const {
  DCHECK(resolved_offsets.IsEmpty());
  DCHECK(ComputeIsActive());
  LayoutBox* layout_box = resolved_scroll_source_->GetLayoutBox();
  DCHECK(layout_box);

  double current_offset;
  double max_offset;
  GetCurrentAndMaxOffset(layout_box, current_offset, max_offset);

  auto orientation = ToPhysicalScrollOrientation(orientation_, *layout_box);

  if (scroll_offsets_->size() == 0) {
    // Start and end offsets resolve to 'auto'.
    resolved_offsets.push_back(0);
    resolved_offsets.push_back(max_offset);
    return true;
  }
  // Single entry offset in scrollOffsets is considered as 'end'.
  if (scroll_offsets_->size() == 1)
    resolved_offsets.push_back(0);
  for (auto& offset : *scroll_offsets_) {
    auto resolved_offset = offset->ResolveOffset(
        resolved_scroll_source_, orientation, max_offset, max_offset);
    if (!resolved_offset) {
      // Empty resolved offset if any of the offsets cannot be resolved.
      resolved_offsets.clear();
      return false;
    }
    resolved_offsets.push_back(resolved_offset.value());
  }
  // TODO(crbug.com/1094014): Implement clamping for overlapping offsets.
  DCHECK_GE(resolved_offsets.size(), 2u);
  return true;
}

AnimationTimeline::PhaseAndTime ScrollTimeline::CurrentPhaseAndTime() {
  return {timeline_state_snapshotted_.phase,
          timeline_state_snapshotted_.current_time};
}

ScrollTimeline::TimelineState ScrollTimeline::ComputeTimelineState() const {
  // 1. If scroll timeline is inactive, return an unresolved time value.
  // https://github.com/WICG/scroll-animations/issues/31
  // https://wicg.github.io/scroll-animations/#current-time-algorithm
  WTF::Vector<double> resolved_offsets;
  if (!ComputeIsActive()) {
    return {TimelinePhase::kInactive, /*current_time*/ base::nullopt,
            resolved_offsets};
  }
  LayoutBox* layout_box = resolved_scroll_source_->GetLayoutBox();
  // 2. Otherwise, let current scroll offset be the current scroll offset of
  // scrollSource in the direction specified by orientation.

  double current_offset;
  double max_offset;
  GetCurrentAndMaxOffset(layout_box, current_offset, max_offset);

  bool resolved = ResolveScrollOffsets(resolved_offsets);

  if (!resolved) {
    DCHECK(resolved_offsets.IsEmpty());
    return {TimelinePhase::kInactive, /*current_time*/ base::nullopt,
            resolved_offsets};
  }

  double start_offset = resolved_offsets[0];
  double end_offset = resolved_offsets[resolved_offsets.size() - 1];

  // TODO(crbug.com/1060384): Once the spec has been updated to state what the
  // expected result is when startScrollOffset >= endScrollOffset, we might need
  // to add a special case here. See
  // https://github.com/WICG/scroll-animations/issues/20

  // 3. If current scroll offset is less than startScrollOffset:
  if (current_offset < start_offset) {
    return {TimelinePhase::kBefore, base::TimeDelta(), resolved_offsets};
  }

  // 4. If current scroll offset is greater than or equal to endScrollOffset:
  if (current_offset >= end_offset) {
    // If end_offset is greater than or equal to the maximum scroll offset of
    // scrollSource in orientation then return active phase, otherwise return
    // after phase.
    TimelinePhase phase = end_offset >= max_offset ? TimelinePhase::kActive
                                                   : TimelinePhase::kAfter;
    return {phase, base::TimeDelta::FromMillisecondsD(time_range_),
            resolved_offsets};
  }

  // 5. Return the result of evaluating the following expression:
  //   ((current scroll offset - startScrollOffset) /
  //      (endScrollOffset - startScrollOffset)) * effective time range
  base::Optional<base::TimeDelta> calculated_current_time =
      base::TimeDelta::FromMillisecondsD(scroll_timeline_util::ComputeProgress(
                                             current_offset, resolved_offsets) *
                                         time_range_);
  return {TimelinePhase::kActive, calculated_current_time, resolved_offsets};
}

// Scroll-linked animations are initialized with the start time of zero.
base::Optional<base::TimeDelta>
ScrollTimeline::InitialStartTimeForAnimations() {
  return base::TimeDelta();
}

void ScrollTimeline::ServiceAnimations(TimingUpdateReason reason) {
  // Snapshot timeline state once at top of animation frame.
  if (reason == kTimingUpdateForAnimationFrame)
    SnapshotState();
  // When scroll timeline goes from inactive to active the animations may need
  // to be started and possibly composited.
  bool was_active =
      last_current_phase_and_time_ &&
      last_current_phase_and_time_.value().phase == TimelinePhase::kActive;
  if (!was_active && IsActive())
    MarkAnimationsCompositorPending();

  AnimationTimeline::ServiceAnimations(reason);
}

void ScrollTimeline::ScheduleNextServiceInternal(bool time_check) {
  if (AnimationsNeedingUpdateCount() == 0)
    return;

  if (time_check) {
    auto state = ComputeTimelineState();
    PhaseAndTime current_phase_and_time{state.phase, state.current_time};
    if (current_phase_and_time == last_current_phase_and_time_)
      return;
  }
  ScheduleServiceOnNextFrame();
}

void ScrollTimeline::ScheduleNextService() {
  ScheduleNextServiceInternal(/* time_check = */ true);
}

void ScrollTimeline::SnapshotState() {
  timeline_state_snapshotted_ = ComputeTimelineState();
}

Element* ScrollTimeline::scrollSource() {
  return scroll_source_.Get();
}

String ScrollTimeline::orientation() {
  switch (orientation_) {
    case Block:
      return "block";
    case Inline:
      return "inline";
    case Horizontal:
      return "horizontal";
    case Vertical:
      return "vertical";
    default:
      NOTREACHED();
      return "";
  }
}

// TODO(crbug.com/1094014): scrollOffsets will replace start and end
// offsets once spec decision on multiple scroll offsets is finalized.
// https://github.com/w3c/csswg-drafts/issues/4912
void ScrollTimeline::startScrollOffset(ScrollTimelineOffsetValue& out) const {
  if (StartScrollOffset()) {
    out = StartScrollOffset()->ToScrollTimelineOffsetValue();
  } else {
    ScrollTimelineOffset scrollOffset;
    out = scrollOffset.ToScrollTimelineOffsetValue();
  }
}

void ScrollTimeline::endScrollOffset(ScrollTimelineOffsetValue& out) const {
  if (EndScrollOffset()) {
    out = EndScrollOffset()->ToScrollTimelineOffsetValue();
  } else {
    ScrollTimelineOffset scrollOffset;
    out = scrollOffset.ToScrollTimelineOffsetValue();
  }
}

const HeapVector<ScrollTimelineOffsetValue> ScrollTimeline::scrollOffsets()
    const {
  HeapVector<ScrollTimelineOffsetValue> scroll_offsets;

  if (!scroll_offsets_)
    return scroll_offsets;

  for (auto& offset : *scroll_offsets_) {
    scroll_offsets.push_back(offset->ToScrollTimelineOffsetValue());
    // 'auto' can only be the end offset.
    DCHECK(!offset->IsDefaultValue() || scroll_offsets.size() == 2);
  }
  return scroll_offsets;
}

void ScrollTimeline::timeRange(DoubleOrScrollTimelineAutoKeyword& result) {
  result.SetDouble(time_range_);
}

void ScrollTimeline::GetCurrentAndMaxOffset(const LayoutBox* layout_box,
                                            double& current_offset,
                                            double& max_offset) const {
  DCHECK(layout_box);
  DCHECK(layout_box->GetScrollableArea());

  // Depending on the writing-mode and direction, the scroll origin shifts and
  // the scroll offset may be negative. The easiest way to deal with this is to
  // use only the magnitude of the scroll offset, and compare it to (max_offset
  // - min_offset).
  PaintLayerScrollableArea* scrollable_area = layout_box->GetScrollableArea();

  // Using the absolute value of the scroll offset only makes sense if either
  // the max or min scroll offset for a given axis is 0. This should be
  // guaranteed by the scroll origin code, but these DCHECKs ensure that.
  DCHECK(scrollable_area->MaximumScrollOffset().Height() == 0 ||
         scrollable_area->MinimumScrollOffset().Height() == 0);
  DCHECK(scrollable_area->MaximumScrollOffset().Width() == 0 ||
         scrollable_area->MinimumScrollOffset().Width() == 0);
  ScrollOffset scroll_offset = scrollable_area->GetScrollOffset();
  ScrollOffset scroll_dimensions = scrollable_area->MaximumScrollOffset() -
                                   scrollable_area->MinimumScrollOffset();

  auto physical_orientation =
      ToPhysicalScrollOrientation(orientation_, *layout_box);

  if (physical_orientation == kHorizontalScroll) {
    current_offset = scroll_offset.Width();
    max_offset = scroll_dimensions.Width();
  } else {
    current_offset = scroll_offset.Height();
    max_offset = scroll_dimensions.Height();
  }
  // When using a rtl direction, current_offset grows correctly from 0 to
  // max_offset, but is negative. Since our offsets are all just deltas along
  // the orientation direction, we can just take the absolute current_offset and
  // use that everywhere.
  current_offset = std::abs(current_offset);
}

void ScrollTimeline::AnimationAttached(Animation* animation) {
  AnimationTimeline::AnimationAttached(animation);
  if (resolved_scroll_source_ && scroll_animations_.IsEmpty())
    resolved_scroll_source_->RegisterScrollTimeline(this);

  scroll_animations_.insert(animation);
}

void ScrollTimeline::AnimationDetached(Animation* animation) {
  AnimationTimeline::AnimationDetached(animation);
  scroll_animations_.erase(animation);
  if (resolved_scroll_source_ && scroll_animations_.IsEmpty())
    resolved_scroll_source_->UnregisterScrollTimeline(this);
}

void ScrollTimeline::WorkletAnimationAttached() {
  if (!resolved_scroll_source_)
    return;
  GetActiveScrollTimelineSet().insert(resolved_scroll_source_);
}

void ScrollTimeline::WorkletAnimationDetached() {
  if (!resolved_scroll_source_)
    return;
  GetActiveScrollTimelineSet().erase(resolved_scroll_source_);
}

void ScrollTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(scroll_animations_);
  visitor->Trace(scroll_source_);
  visitor->Trace(resolved_scroll_source_);
  visitor->Trace(scroll_offsets_);
  AnimationTimeline::Trace(visitor);
}

bool ScrollTimeline::HasActiveScrollTimeline(Node* node) {
  ActiveScrollTimelineSet& worklet_animations_set =
      GetActiveScrollTimelineSet();
  auto worklet_animations_it = worklet_animations_set.find(node);
  if (worklet_animations_it != worklet_animations_set.end() &&
      worklet_animations_it->value > 0)
    return true;

  ScrollTimelineSet& set = GetScrollTimelineSet();
  auto it = set.find(node);
  if (it == set.end())
    return false;

  for (auto& timeline : *it->value) {
    if (timeline->HasAnimations())
      return true;
  }
  return false;
}

void ScrollTimeline::Invalidate(Node* node) {
  ScrollTimelineSet& set = GetScrollTimelineSet();
  auto it = set.find(node);

  if (it == set.end())
    return;

  for (auto& timeline : *it->value) {
    timeline->Invalidate();
  }
}

CompositorAnimationTimeline* ScrollTimeline::EnsureCompositorTimeline() {
  if (compositor_timeline_)
    return compositor_timeline_.get();

  compositor_timeline_ = std::make_unique<CompositorAnimationTimeline>(
      scroll_timeline_util::ToCompositorScrollTimeline(this));
  return compositor_timeline_.get();
}

void ScrollTimeline::UpdateCompositorTimeline() {
  if (!compositor_timeline_)
    return;
  compositor_timeline_->UpdateCompositorTimeline(
      scroll_timeline_util::GetCompositorScrollElementId(
          resolved_scroll_source_),
      GetResolvedScrollOffsets());
}

}  // namespace blink
