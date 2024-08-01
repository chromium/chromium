// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/scroll_timeline.h"

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_timeline_options.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
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
  std::optional<Element*> source = options->hasSource()
                                       ? std::make_optional(options->source())
                                       : std::nullopt;

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
    : ScrollSnapshotTimeline(document),
      reference_type_(reference_type),
      reference_element_(reference),
      axis_(axis) {}

Element* ScrollTimeline::RetainingElement() const {
  return reference_element_.Get();
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
  LayoutBox* scroll_container = ComputeScrollContainer(state.resolved_source);
  if (!scroll_container) {
    return state;
  }

  // The scrollable area must exist since the timeline is active.
  DCHECK(scroll_container->GetScrollableArea());

  // Depending on the writing-mode and direction, the scroll origin shifts and
  // the scroll offset may be negative. The easiest way to deal with this is to
  // use only the magnitude of the scroll offset, and compare it to (max_offset
  // - min_offset).
  PaintLayerScrollableArea* scrollable_area =
      scroll_container->GetScrollableArea();
  // Scrollable area must exist since the timeline is active.
  DCHECK(scrollable_area);

  // Using the absolute value of the scroll offset only makes sense if either
  // the max or min scroll offset for a given axis is 0. This should be
  // guaranteed by the scroll origin code, but these DCHECKs ensure that.
  DCHECK(scrollable_area->MaximumScrollOffset().y() == 0 ||
         scrollable_area->MinimumScrollOffset().y() == 0);
  DCHECK(scrollable_area->MaximumScrollOffset().x() == 0 ||
         scrollable_area->MinimumScrollOffset().x() == 0);

  ScrollOffset scroll_offset = scrollable_area->GetScrollOffset();
  auto physical_orientation =
      ToPhysicalScrollOrientation(GetAxis(), *scroll_container);
  double current_offset = (physical_orientation == kHorizontalScroll)
                              ? scroll_offset.x()
                              : scroll_offset.y();
  // When using a rtl direction, current_offset grows correctly from 0 to
  // max_offset, but is negative. Since our offsets are all just deltas along
  // the orientation direction, we can just take the absolute current_offset and
  // use that everywhere.
  current_offset = std::abs(current_offset);

  CalculateOffsets(scrollable_area, physical_orientation, &state);
  if (!state.scroll_offsets) {
    // Scroll Offsets may be null if the type of subject element is not
    // supported.
    return state;
  }

  state.zoom = scroll_container->StyleRef().EffectiveZoom();
  // Timeline is inactive unless the scroll offset range is positive.
  // github.com/w3c/csswg-drafts/issues/7401
  if (state.scroll_offsets->end - state.scroll_offsets->start > 0) {
    state.phase = TimelinePhase::kActive;
    double offset = current_offset - state.scroll_offsets->start;
    double range = state.scroll_offsets->end - state.scroll_offsets->start;
    double duration_in_microseconds =
        range * kScrollTimelineMicrosecondsPerPixel;
    state.duration = std::make_optional(ANIMATION_TIME_DELTA_FROM_MILLISECONDS(
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
  state->scroll_offsets = std::make_optional<ScrollOffsets>(0, end_offset);
}

Element* ScrollTimeline::source() const {
  return ComputeSource();
}

Element* ScrollTimeline::ComputeSource() const {
  if (reference_type_ == ReferenceType::kNearestAncestor &&
      reference_element_) {
    reference_element_->GetDocument().UpdateStyleAndLayout(
        DocumentUpdateReason::kJavaScript);
  }
  return ComputeSourceNoLayout();
}

Element* ScrollTimeline::ComputeSourceNoLayout() const {
  if (reference_type_ == ReferenceType::kSource) {
    return reference_element_.Get();
  }
  DCHECK_EQ(ReferenceType::kNearestAncestor, reference_type_);

  if (!reference_element_) {
    return nullptr;
  }

  LayoutObject* layout_object = reference_element_->GetLayoutObject();
  if (!layout_object) {
    return nullptr;
  }

  const LayoutBox* scroll_container =
      layout_object->ContainingScrollContainer();
  if (!scroll_container) {
    return reference_element_->GetDocument().ScrollingElementNoLayout();
  }

  Node* node = scroll_container->GetNode();
  DCHECK(node || scroll_container->IsAnonymous());
  if (!node) {
    // The content scroller for a FieldSet is an anonymous block.  In this case,
    // the parent's node is the fieldset element.
    const LayoutBox* parent = DynamicTo<LayoutBox>(scroll_container->Parent());
    if (parent && parent->StyleRef().IsScrollContainer()) {
      node = parent->GetNode();
    }
  }

  if (!node) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  if (node->IsElementNode()) {
    return DynamicTo<Element>(node);
  }
  if (node->IsDocumentNode()) {
    return DynamicTo<Document>(node)->ScrollingElementNoLayout();
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
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
  return ResolveSource(ComputeSourceNoLayout());
}

void ScrollTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(reference_element_);
  ScrollSnapshotTimeline::Trace(visitor);
}

bool ScrollTimeline::Matches(ReferenceType reference_type,
                             Element* reference_element,
                             ScrollAxis axis) const {
  return (reference_type_ == reference_type) &&
         (reference_element_ == reference_element) && (axis_ == axis);
}

ScrollAxis ScrollTimeline::GetAxis() const {
  return axis_;
}

std::optional<double> ScrollTimeline::GetMaximumScrollPosition() const {
  std::optional<ScrollOffsets> scroll_offsets = GetResolvedScrollOffsets();
  if (!scroll_offsets) {
    return std::nullopt;
  }
  LayoutBox* scroll_container = ScrollContainer();
  if (!scroll_container) {
    return std::nullopt;
  }

  PaintLayerScrollableArea* scrollable_area =
      scroll_container->GetScrollableArea();
  if (!scrollable_area) {
    return std::nullopt;
  }
  ScrollOffset scroll_dimensions = scrollable_area->MaximumScrollOffset() -
                                   scrollable_area->MinimumScrollOffset();
  auto physical_orientation =
      ToPhysicalScrollOrientation(GetAxis(), *scroll_container);
  return physical_orientation == kHorizontalScroll ? scroll_dimensions.x()
                                                   : scroll_dimensions.y();
}

}  // namespace blink
