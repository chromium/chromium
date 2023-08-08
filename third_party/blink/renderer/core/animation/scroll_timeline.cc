// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"

namespace blink {

namespace {

ScrollOrientation ToPhysicalScrollOrientation(ScrollAxis axis,
                                              const LayoutBox& source_box) {
  bool is_horizontal = source_box.IsHorizontalWritingMode();
  switch (axis) {
    case ScrollAxis::kBlock:
      return is_horizontal ? kVerticalScroll : kHorizontalScroll;
    case ScrollAxis::kInline:
      return is_horizontal ? kHorizontalScroll : kVerticalScroll;
    case ScrollAxis::kX:
      return kHorizontalScroll;
    case ScrollAxis::kY:
      return kVerticalScroll;
  }
}

Node* ResolveSource(Element* source) {
  if (source && source == source->GetDocument().ScrollingElementNoLayout()) {
    return &source->GetDocument();
  }
  return source;
}

}  // namespace

ScrollTimeline* ScrollTimeline::Create(Document& document,
                                       ScrollTimelineOptions* options,
                                       ExceptionState& exception_state) {
  absl::optional<Element*> source = options->hasSource()
                                        ? absl::make_optional(options->source())
                                        : absl::nullopt;

  ScrollAxis axis =
      options->hasAxis() ? options->axis().AsEnum() : ScrollAxis::kBlock;

  // The scrollingElement depends on style/layout-tree in quirks mode. Update
  // such that subsequent calls to ScrollingElementNoLayout returns up-to-date
  // information.
  if (document.InQuirksMode())
    document.UpdateStyleAndLayoutTree();

  return Create(&document, source.value_or(document.ScrollingElementNoLayout()),
                axis);
}

ScrollTimeline* ScrollTimeline::Create(Document* document,
                                       Element* source,
                                       ScrollAxis axis) {
  ScrollTimeline* scroll_timeline = MakeGarbageCollected<ScrollTimeline>(
      document, ReferenceType::kSource, source, axis);
  scroll_timeline->UpdateSnapshot();

  return scroll_timeline;
}

ScrollTimeline::ScrollTimeline(Document* document,
                               ReferenceType reference_type,
                               Element* reference,
                               ScrollAxis axis)
    : ScrollTimeline(
          document,
          MakeGarbageCollected<ScrollTimelineAttachment>(reference_type,
                                                         reference,
                                                         axis)) {}

ScrollTimeline::ScrollTimeline(Document* document,
                               ScrollTimelineAttachment* attachment)
    : ScrollSnapshotTimeline(document) {
  if (attachment) {
    attachments_.push_back(attachment);
  }
}

Element* ScrollTimeline::RetainingElement() const {
  return CurrentAttachment()->GetReferenceElement();
}

// TODO(crbug.com/1060384): This section is missing from the spec rewrite.
// Resolved to remove the before and after phases in
// https://github.com/w3c/csswg-drafts/issues/7240.
// https://drafts.csswg.org/scroll-animations-1/#current-time-algorithm
ScrollTimeline::TimelineState ScrollTimeline::ComputeTimelineState() const {
  TimelineState state;
  state.resolved_source = ComputeResolvedSource();

  // 1. If scroll timeline is inactive, return an unresolved time value.
  // https://github.com/WICG/scroll-animations/issues/31
  // https://wicg.github.io/scroll-animations/#current-time-algorithm
  if (!ComputeIsResolved(state.resolved_source)) {
    return state;
  }
  DCHECK(state.resolved_source);
  LayoutBox* layout_box = state.resolved_source->GetLayoutBox();

  // Layout box and scrollable area must exist since the timeline is active.
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
  DCHECK(scrollable_area->MaximumScrollOffset().y() == 0 ||
         scrollable_area->MinimumScrollOffset().y() == 0);
  DCHECK(scrollable_area->MaximumScrollOffset().x() == 0 ||
         scrollable_area->MinimumScrollOffset().x() == 0);

  ScrollOffset scroll_offset = scrollable_area->GetScrollOffset();
  auto physical_orientation =
      ToPhysicalScrollOrientation(GetAxis(), *layout_box);
  double current_offset = (physical_orientation == kHorizontalScroll)
                              ? scroll_offset.x()
                              : scroll_offset.y();
  // When using a rtl direction, current_offset grows correctly from 0 to
  // max_offset, but is negative. Since our offsets are all just deltas along
  // the orientation direction, we can just take the absolute current_offset and
  // use that everywhere.
  current_offset = std::abs(current_offset);

  CalculateOffsets(scrollable_area, physical_orientation, &state);
  DCHECK(state.scroll_offsets);

  state.zoom = layout_box->StyleRef().EffectiveZoom();
  // Timeline is inactive unless the scroll offset range is positive.
  // github.com/w3c/csswg-drafts/issues/7401
  if (std::abs(state.scroll_offsets->end - state.scroll_offsets->start) > 0) {
    state.phase = TimelinePhase::kActive;
    double offset = current_offset - state.scroll_offsets->start;
    double range = state.scroll_offsets->end - state.scroll_offsets->start;
    double duration_in_microseconds =
        range * kScrollTimelineMicrosecondsPerPixel;
    state.duration = absl::make_optional(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(
        duration_in_microseconds / 1000));
    state.current_time =
        base::Microseconds(offset * kScrollTimelineMicrosecondsPerPixel);
  }
  return state;
}

void ScrollTimeline::CalculateOffsets(PaintLayerScrollableArea* scrollable_area,
                                      ScrollOrientation physical_orientation,
                                      TimelineState* state) const {
  ScrollOffset scroll_dimensions = scrollable_area->MaximumScrollOffset() -
                                   scrollable_area->MinimumScrollOffset();
  double end_offset = physical_orientation == kHorizontalScroll
                          ? scroll_dimensions.x()
                          : scroll_dimensions.y();
  state->scroll_offsets = absl::make_optional<ScrollOffsets>(0, end_offset);
}

Element* ScrollTimeline::source() const {
  return CurrentAttachment() ? CurrentAttachment()->ComputeSource() : nullptr;
}

void ScrollTimeline::AnimationAttached(Animation* animation) {
  if (RetainingElement() && !HasAnimations()) {
    RetainingElement()->RegisterScrollTimeline(this);
  }

  AnimationTimeline::AnimationAttached(animation);
}

void ScrollTimeline::AnimationDetached(Animation* animation) {
  AnimationTimeline::AnimationDetached(animation);

  if (RetainingElement() && !HasAnimations()) {
    RetainingElement()->UnregisterScrollTimeline(this);
  }
}

Node* ScrollTimeline::ComputeResolvedSource() const {
  if (!CurrentAttachment()) {
    return nullptr;
  }
  return ResolveSource(CurrentAttachment()->ComputeSourceNoLayout());
}

void ScrollTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(attachments_);
  ScrollSnapshotTimeline::Trace(visitor);
}

bool ScrollTimeline::Matches(ReferenceType reference_type,
                             Element* reference_element,
                             ScrollAxis axis) const {
  const ScrollTimelineAttachment* attachment = CurrentAttachment();
  DCHECK(attachment);
  return (attachment->GetReferenceType() == reference_type) &&
         (attachment->GetReferenceElement() == reference_element) &&
         (attachment->GetAxis() == axis);
}

ScrollAxis ScrollTimeline::GetAxis() const {
  if (const ScrollTimelineAttachment* attachment = CurrentAttachment()) {
    return attachment->GetAxis();
  }
  return ScrollAxis::kBlock;
}

absl::optional<double> ScrollTimeline::GetMaximumScrollPosition() const {
  absl::optional<ScrollOffsets> scroll_offsets = GetResolvedScrollOffsets();
  if (!scroll_offsets) {
    return absl::nullopt;
  }
  LayoutBox* layout_box = ResolvedSource()->GetLayoutBox();
  if (!layout_box) {
    return absl::nullopt;
  }

  PaintLayerScrollableArea* scrollable_area = layout_box->GetScrollableArea();
  if (!scrollable_area) {
    return absl::nullopt;
  }
  ScrollOffset scroll_dimensions = scrollable_area->MaximumScrollOffset() -
                                   scrollable_area->MinimumScrollOffset();
  auto physical_orientation =
      ToPhysicalScrollOrientation(GetAxis(), *layout_box);
  return physical_orientation == kHorizontalScroll ? scroll_dimensions.x()
                                                   : scroll_dimensions.y();
}

}  // namespace blink
