// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/view_timeline.h"

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalueorstringsequence_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline_options.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

using InsetValueSequence =
    const HeapVector<Member<V8UnionCSSNumericValueOrString>>;

namespace {

bool IsBlockDirection(ViewTimeline::ScrollAxis axis, WritingMode writing_mode) {
  switch (axis) {
    case ViewTimeline::ScrollAxis::kBlock:
      return true;
    case ViewTimeline::ScrollAxis::kInline:
      return false;
    case ViewTimeline::ScrollAxis::kX:
      return !blink::IsHorizontalWritingMode(writing_mode);
    case ViewTimeline::ScrollAxis::kY:
      return blink::IsHorizontalWritingMode(writing_mode);
  }
}

// ResolveAuto replaces any value 'auto' with the value of the corresponding
// scroll-padding-* property. Note that 'auto' is a valid value for
// scroll-padding-*, and therefore 'auto' (the "pointer" to the scroll-padding
// value) may resolve to 'auto' (the actual value of the scroll-padding
// property).
//
// https://drafts.csswg.org/scroll-animations-1/#valdef-view-timeline-inset-auto
TimelineInset ResolveAuto(const TimelineInset& inset,
                          Element& source,
                          ViewTimeline::ScrollAxis axis) {
  const ComputedStyle* style = source.GetComputedStyle();
  if (!style)
    return inset;

  const Length& start = inset.GetStart();
  const Length& end = inset.GetEnd();

  if (IsBlockDirection(axis, style->GetWritingMode())) {
    return TimelineInset(
        start.IsAuto() ? style->ScrollPaddingBlockStart() : start,
        end.IsAuto() ? style->ScrollPaddingBlockEnd() : end);
  }
  return TimelineInset(
      start.IsAuto() ? style->ScrollPaddingInlineStart() : start,
      end.IsAuto() ? style->ScrollPaddingInlineEnd() : end);
}

LayoutUnit ComputeInset(const Length& inset, LayoutUnit viewport_size) {
  return MinimumValueForLength(inset, viewport_size);
}

const CSSValue* ParseInset(const InsetValueSequence& array,
                           wtf_size_t index,
                           ExceptionState& exception_state) {
  if (index >= array.size())
    return nullptr;

  V8UnionCSSNumericValueOrString* value = array[index];
  if (value->IsString()) {
    if (value->GetAsString() != "auto")
      exception_state.ThrowTypeError("inset must be CSSNumericValue or auto");

    return CSSIdentifierValue::Create(Length(Length::Type::kAuto));
  }

  CSSNumericValue* numeric_value = value->GetAsCSSNumericValue();
  const CSSPrimitiveValue* css_value =
      DynamicTo<CSSPrimitiveValue>(numeric_value->ToCSSValue());
  if (!css_value || (!css_value->IsLength() && !css_value->IsPercentage())) {
    exception_state.ThrowTypeError("Invalid inset");
    return nullptr;
  }

  return css_value;
}

const CSSValuePair* ParseInsetPair(Document& document, const String str_value) {
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kViewTimelineInset, str_value,
      document.ElementSheet().Contents()->ParserContext());

  auto* value_list = DynamicTo<CSSValueList>(value);
  if (!value_list || value_list->length() != 1)
    return nullptr;

  return &To<CSSValuePair>(value_list->Item(0));
}

bool IsStyleDependent(const CSSValue* value) {
  if (!value) {
    return false;
  }

  if (const CSSPrimitiveValue* css_primitive_value =
          DynamicTo<CSSPrimitiveValue>(value)) {
    if (!value->IsNumericLiteralValue()) {
      // Err on the side of caution with a math expression. No strict guarantee
      // that we can extract a style-invariant length.
      return true;
    }

    return !css_primitive_value->IsPx() && !css_primitive_value->IsPercentage();
  }

  return false;
}

Length InsetValueToLength(const CSSValue* inset_value,
                          Element* subject,
                          Length default_value) {
  if (!inset_value)
    return default_value;

  if (!subject)
    return Length(Length::Type::kAuto);

  if (inset_value->IsIdentifierValue()) {
    DCHECK_EQ(To<CSSIdentifierValue>(inset_value)->GetValueID(),
              CSSValueID::kAuto);
    return Length(Length::Type::kAuto);
  }

  // If the subject is detached from the document, we cannot resolve the style,
  // and thus cannot construct length conversion data. Nonetheless, we can
  // evaluate the length in trivial cases and rely on the inset value being
  // marked as style dependent otherwise.
  if (!subject->GetComputedStyle()) {
    if (const CSSNumericLiteralValue* literal_value =
            DynamicTo<CSSNumericLiteralValue>(inset_value)) {
      if (literal_value->IsPx()) {
        return Length(literal_value->DoubleValue(), Length::Type::kFixed);
      } else if (literal_value->IsPercentage()) {
        return Length(literal_value->DoubleValue(), Length::Type::kPercent);
      }
    }
    DCHECK(IsStyleDependent(inset_value));
    return Length(Length::Type::kAuto);
  }

  if (inset_value->IsPrimitiveValue()) {
    ElementResolveContext element_resolve_context(*subject);
    Document& document = subject->GetDocument();
    // Flags can be ignored, because we re-resolve any value that's not px or
    // percentage, see IsStyleDependent.
    CSSToLengthConversionData::Flags ignored_flags = 0;
    CSSToLengthConversionData length_conversion_data(
        subject->ComputedStyleRef(), element_resolve_context.ParentStyle(),
        element_resolve_context.RootElementStyle(),
        CSSToLengthConversionData::ViewportSize(document.GetLayoutView()),
        CSSToLengthConversionData::ContainerSizes(subject),
        CSSToLengthConversionData::AnchorData(),
        subject->GetComputedStyle()->EffectiveZoom(), ignored_flags);

    return DynamicTo<CSSPrimitiveValue>(inset_value)
        ->ConvertToLength(length_conversion_data);
  }

  NOTREACHED_IN_MIGRATION();
  return Length(Length::Type::kAuto);
}

enum class StickinessRange {
  kBeforeEntry,
  kDuringEntry,
  kWhileContained,
  kWhileCovering,
  kDuringExit,
  kAfterExit
};

StickinessRange ComputeStickinessRange(
    LayoutUnit sticky_box_stuck_pos_in_viewport,
    LayoutUnit sticky_box_static_pos,
    double viewport_size,
    double target_size,
    double target_pos) {
  // Need to know: when the sticky box is stuck, where is the view-timeline
  // target in relation to the scroller's viewport?
  double target_pos_in_viewport = sticky_box_stuck_pos_in_viewport +
                                  target_pos - sticky_box_static_pos.ToDouble();

  if (target_pos_in_viewport < 0 &&
      target_pos_in_viewport + target_size > viewport_size) {
    return StickinessRange::kWhileCovering;
  }

  if (target_pos_in_viewport > viewport_size) {
    return StickinessRange::kBeforeEntry;
  } else if (target_pos_in_viewport + target_size > viewport_size) {
    return StickinessRange::kDuringEntry;
  }

  if (target_pos_in_viewport + target_size < 0) {
    return StickinessRange::kAfterExit;
  } else if (target_pos_in_viewport < 0) {
    return StickinessRange::kDuringExit;
  }

  return StickinessRange::kWhileContained;
}

}  // end namespace

ViewTimeline* ViewTimeline::Create(Document& document,
                                   ViewTimelineOptions* options,
                                   ExceptionState& exception_state) {
  Element* subject = options->hasSubject() ? options->subject() : nullptr;

  ScrollAxis axis =
      options->hasAxis() ? options->axis().AsEnum() : ScrollAxis::kBlock;

  if (subject) {
    // This ensures that Client[Left,Top]NoLayout (reached via SnapshotState)
    // returns up-to-date information.
    document.UpdateStyleAndLayoutForNode(subject,
                                         DocumentUpdateReason::kJavaScript);
  }

  // Parse insets.
  const V8UnionCSSNumericValueOrStringSequenceOrString* v8_inset =
      options->inset();

  std::optional<const CSSValue*> start_inset_value;
  std::optional<const CSSValue*> end_inset_value;
  if (v8_inset && v8_inset->IsCSSNumericValueOrStringSequence()) {
    const InsetValueSequence inset_array =
        v8_inset->GetAsCSSNumericValueOrStringSequence();
    if (inset_array.size() > 2) {
      exception_state.ThrowTypeError("Invalid inset");
      return nullptr;
    }

    start_inset_value = ParseInset(inset_array, 0, exception_state);
    end_inset_value = ParseInset(inset_array, 1, exception_state);
  } else if (v8_inset && v8_inset->IsString()) {
    const CSSValuePair* value_pair =
        ParseInsetPair(document, v8_inset->GetAsString());
    if (!value_pair) {
      exception_state.ThrowTypeError("Invalid inset");
      return nullptr;
    }
    start_inset_value = &value_pair->First();
    end_inset_value = &value_pair->Second();
  }

  Length inset_start_side =
      InsetValueToLength(start_inset_value.value_or(nullptr), subject,
                         Length(Length::Type::kFixed));
  Length inset_end_side = InsetValueToLength(end_inset_value.value_or(nullptr),
                                             subject, inset_start_side);

  ViewTimeline* view_timeline = MakeGarbageCollected<ViewTimeline>(
      &document, subject, axis,
      TimelineInset(inset_start_side, inset_end_side));

  if (start_inset_value && IsStyleDependent(start_inset_value.value()))
    view_timeline->style_dependant_start_inset_ = start_inset_value.value();
  if (end_inset_value && IsStyleDependent(end_inset_value.value()))
    view_timeline->style_dependant_end_inset_ = end_inset_value.value();

  view_timeline->UpdateSnapshot();
  return view_timeline;
}

ViewTimeline::ViewTimeline(Document* document,
                           Element* subject,
                           ScrollAxis axis,
                           TimelineInset inset)
    : ScrollTimeline(document,
                     ReferenceType::kNearestAncestor,
                     /* reference_element */ subject,
                     axis),
      inset_(inset) {}

void ViewTimeline::CalculateOffsets(PaintLayerScrollableArea* scrollable_area,
                                    ScrollOrientation physical_orientation,
                                    TimelineState* state) const {
  // Do not call this method with an unresolved timeline.
  // Called from ScrollTimeline::ComputeTimelineState, which has safeguard.
  // Any new call sites will require a similar safeguard.
  LayoutBox* scroll_container = ComputeScrollContainer(state->resolved_source);
  DCHECK(scroll_container);
  DCHECK(subject());

  std::optional<gfx::SizeF> subject_size = SubjectSize();
  if (!subject_size) {
    // Subject size may be null if the type of subject element is not supported.
    return;
  }

  std::optional<gfx::PointF> subject_position =
      SubjectPosition(scroll_container);
  DCHECK(subject_position);

  // TODO(crbug.com/1448801): Handle nested sticky elements.
  double target_offset = physical_orientation == kHorizontalScroll
                             ? subject_position->x()
                             : subject_position->y();
  double target_size;
  LayoutUnit viewport_size;
  if (physical_orientation == kHorizontalScroll) {
    target_size = subject_size->width();
    viewport_size = scrollable_area->LayoutContentRect().Width();
  } else {
    target_size = subject_size->height();
    viewport_size = scrollable_area->LayoutContentRect().Height();
  }

  Element* source = ComputeSourceNoLayout();
  DCHECK(source);
  TimelineInset inset = ResolveAuto(GetInset(), *source, GetAxis());

  // Update inset lengths if style dependent.
  if (style_dependant_start_inset_ || style_dependant_end_inset_) {
    Length updated_start = inset.GetStart();
    Length updated_end = inset.GetEnd();
    if (style_dependant_start_inset_) {
      updated_start = InsetValueToLength(style_dependant_start_inset_,
                                         subject(), Length::Fixed());
    }
    if (style_dependant_end_inset_) {
      updated_end = InsetValueToLength(style_dependant_end_inset_, subject(),
                                       Length::Fixed());
    }
    inset = TimelineInset(updated_start, updated_end);
  }

  // Note that the end_side_inset is used to adjust the start offset,
  // and the start_side_inset is used to adjust the end offset.
  // This is because "start side" refers to the logical start side [1] of the
  // source box, whereas "start offset" refers to the start of the timeline,
  // and similarly for end side/offset.
  // [1] https://drafts.csswg.org/css-writing-modes-4/#css-start
  double end_side_inset = ComputeInset(inset.GetEnd(), viewport_size);
  double start_side_inset = ComputeInset(inset.GetStart(), viewport_size);

  double viewport_size_double = viewport_size.ToDouble();

  ScrollOffsets scroll_offsets = {
      target_offset - viewport_size_double + end_side_inset,
      target_offset + target_size - start_side_inset};
  ViewOffsets view_offsets = {target_size, target_size};
  ApplyStickyAdjustments(scroll_offsets, view_offsets, viewport_size_double,
                         target_size, target_offset, physical_orientation,
                         scroll_container);

  state->scroll_offsets = scroll_offsets;
  state->view_offsets = view_offsets;
}

void ViewTimeline::ApplyStickyAdjustments(ScrollOffsets& scroll_offsets,
                                          ViewOffsets& view_offsets,
                                          double viewport_size,
                                          double target_size,
                                          double target_offset,
                                          ScrollOrientation orientation,
                                          LayoutBox* scroll_container) const {
  if (!subject()) {
    return;
  }

  LayoutBox* subject_layout_box = subject()->GetLayoutBox();
  if (!subject_layout_box || !scroll_container) {
    return;
  }

  const LayoutBoxModelObject* sticky_container =
      subject_layout_box->FindFirstStickyContainer(scroll_container);
  if (!sticky_container) {
    return;
  }

  StickyPositionScrollingConstraints* constraints =
      sticky_container->StickyConstraints();
  if (!constraints) {
    return;
  }

  const PhysicalRect& container =
      constraints->scroll_container_relative_containing_block_rect;
  const PhysicalRect& sticky_rect =
      constraints->scroll_container_relative_sticky_box_rect;

  bool is_horizontal = orientation == kHorizontalScroll;

  // This is the sticky element's maximum forward displacement (from its static
  // position) due to having "left" or "top" set. It is based on the available
  // room for the sticky element to move within its containing block.
  double max_forward_adjust = 0;

  // This is the sticky element's maximum backward displacement from being
  // "right"- or "bottom"-stuck.
  double max_backward_adjust = 0;

  // These values indicate which view-timeline range we will be in (see
  // https://drafts.csswg.org/scroll-animations-1/#view-timelines-ranges)
  // when we become left/top-stuck (forward_stickiness) or right/bottom-stuck
  // (backward_stickiness).
  StickinessRange backward_stickiness = StickinessRange::kWhileContained;
  StickinessRange forward_stickiness = StickinessRange::kWhileContained;

  // The maximum adjustment from each offset property is the available room
  // from the opposite edge of the sticky element in its static position.
  if (is_horizontal) {
    if (constraints->left_inset) {
      max_forward_adjust = (container.Right() - sticky_rect.Right()).ToDouble();
      forward_stickiness =
          ComputeStickinessRange(*constraints->left_inset, sticky_rect.X(),
                                 viewport_size, target_size, target_offset);
    }
    if (constraints->right_inset) {
      max_backward_adjust = (container.X() - sticky_rect.X()).ToDouble();
      backward_stickiness = ComputeStickinessRange(
          LayoutUnit(viewport_size) - *constraints->right_inset -
              sticky_rect.Width(),
          sticky_rect.X(), viewport_size, target_size, target_offset);
    }
  } else {  // Vertical.
    if (constraints->top_inset) {
      max_forward_adjust =
          (container.Bottom() - sticky_rect.Bottom()).ToDouble();
      forward_stickiness =
          ComputeStickinessRange(*constraints->top_inset, sticky_rect.Y(),
                                 viewport_size, target_size, target_offset);
    }
    if (constraints->bottom_inset) {
      max_backward_adjust = (container.Y() - sticky_rect.Y()).ToDouble();
      backward_stickiness = ComputeStickinessRange(
          LayoutUnit(viewport_size) - *constraints->bottom_inset -
              sticky_rect.Height(),
          sticky_rect.Y(), viewport_size, target_size, target_offset);
    }
  }

  // Now apply the necessary adjustments to scroll_offsets and view_offsets.

  if (forward_stickiness == StickinessRange::kBeforeEntry) {
    scroll_offsets.start += max_forward_adjust;
  }
  if (backward_stickiness != StickinessRange::kBeforeEntry) {
    scroll_offsets.start += max_backward_adjust;
  }

  if (forward_stickiness == StickinessRange::kDuringEntry ||
      forward_stickiness == StickinessRange::kWhileCovering) {
    view_offsets.entry_crossing_distance += max_forward_adjust;
  }
  if (backward_stickiness == StickinessRange::kDuringEntry ||
      backward_stickiness == StickinessRange::kWhileCovering) {
    view_offsets.entry_crossing_distance -= max_backward_adjust;
  }

  if (forward_stickiness == StickinessRange::kDuringExit ||
      forward_stickiness == StickinessRange::kWhileCovering) {
    view_offsets.exit_crossing_distance += max_forward_adjust;
  }
  if (backward_stickiness == StickinessRange::kDuringExit ||
      backward_stickiness == StickinessRange::kWhileCovering) {
    view_offsets.exit_crossing_distance -= max_backward_adjust;
  }

  if (forward_stickiness != StickinessRange::kAfterExit) {
    scroll_offsets.end += max_forward_adjust;
  }
  if (backward_stickiness == StickinessRange::kAfterExit) {
    scroll_offsets.end += max_backward_adjust;
  }
}

std::optional<gfx::SizeF> ViewTimeline::SubjectSize() const {
  if (!subject()) {
    return std::nullopt;
  }
  const LayoutObject* subject_layout_object = subject()->GetLayoutObject();
  if (!subject_layout_object) {
    return std::nullopt;
  }

  if (subject_layout_object->IsSVGChild()) {
    // Find the outermost SVG root.
    const LayoutObject* svg_root = subject_layout_object->Parent();
    while (svg_root && !svg_root->IsSVGRoot()) {
      svg_root = svg_root->Parent();
    }
    // Map the bounds of the element into the (border-box relative) coordinate
    // space of the CSS box of the outermost SVG root.
    const gfx::QuadF local_bounds(
        subject_layout_object->DecoratedBoundingBox());
    return subject_layout_object
        ->LocalToAncestorQuad(local_bounds, To<LayoutSVGRoot>(svg_root))
        .BoundingBox()
        .size();
  }

  if (auto* layout_box = DynamicTo<LayoutBox>(subject_layout_object)) {
    return gfx::SizeF(layout_box->Size());
  }

  if (auto* layout_inline = DynamicTo<LayoutInline>(subject_layout_object)) {
    return layout_inline->LocalBoundingBoxRectF().size();
  }

  return std::nullopt;
}

std::optional<gfx::PointF> ViewTimeline::SubjectPosition(
    LayoutBox* scroll_container) const {
  if (!subject() || !scroll_container) {
    return std::nullopt;
  }
  LayoutObject* subject_layout_object = subject()->GetLayoutObject();
  if (!subject_layout_object || !scroll_container) {
    return std::nullopt;
  }
  MapCoordinatesFlags flags =
      kIgnoreScrollOffset | kIgnoreStickyOffset | kIgnoreTransforms;
  gfx::PointF subject_pos = subject_layout_object->LocalToAncestorPoint(
      gfx::PointF(), scroll_container, flags);

  // We call LayoutObject::ClientLeft/Top directly and avoid
  // Element::clientLeft/Top because:
  //
  // - We may reach this function during style resolution,
  //   and clientLeft/Top also attempt to update style/layout.
  // - Those functions return the unzoomed values, and we require the zoomed
  //   values.

  return gfx::PointF(
      subject_pos.x() - scroll_container->ClientLeft().ToDouble(),
      subject_pos.y() - scroll_container->ClientTop().ToDouble());
}

// https://www.w3.org/TR/scroll-animations-1/#named-range-getTime
CSSNumericValue* ViewTimeline::getCurrentTime(const String& rangeName) {
  if (!IsActive())
    return nullptr;

  TimelineOffset range_start;
  TimelineOffset range_end;
  if (rangeName == "cover") {
    range_start.name = TimelineOffset::NamedRange::kCover;
  } else if (rangeName == "contain") {
    range_start.name = TimelineOffset::NamedRange::kContain;
  } else if (rangeName == "entry") {
    range_start.name = TimelineOffset::NamedRange::kEntry;
  } else if (rangeName == "entry-crossing") {
    range_start.name = TimelineOffset::NamedRange::kEntryCrossing;
  } else if (rangeName == "exit") {
    range_start.name = TimelineOffset::NamedRange::kExit;
  } else if (rangeName == "exit-crossing") {
    range_start.name = TimelineOffset::NamedRange::kExitCrossing;
  } else {
    return nullptr;
  }

  range_start.offset = Length::Percent(0);
  range_end.name = range_start.name;
  range_end.offset = Length::Percent(100);

  double relative_start_offset = ToFractionalOffset(range_start);
  double relative_end_offset = ToFractionalOffset(range_end);
  double range = relative_end_offset - relative_start_offset;

  // TODO(https://github.com/w3c/csswg-drafts/issues/8114): Update and add tests
  // once ratified in the spec.
  if (range == 0)
    return nullptr;

  std::optional<base::TimeDelta> current_time = CurrentPhaseAndTime().time;
  // If current time is null then the timeline must be inactive, which is
  // handled above.
  DCHECK(current_time);
  DCHECK(GetDuration());

  double timeline_progress =
      CurrentPhaseAndTime().time.value().InMillisecondsF() /
      GetDuration().value().InMillisecondsF();

  double named_range_progress =
      (timeline_progress - relative_start_offset) / range;

  return CSSUnitValues::percent(named_range_progress * 100);
}

Element* ViewTimeline::subject() const {
  return GetReferenceElement();
}

bool ViewTimeline::Matches(Element* subject,
                           ScrollAxis axis,
                           const TimelineInset& inset) const {
  if (!ScrollTimeline::Matches(ReferenceType::kNearestAncestor,
                               /* reference_element */ subject, axis)) {
    return false;
  }
  return inset_ == inset;
}

const TimelineInset& ViewTimeline::GetInset() const {
  return inset_;
}

double ViewTimeline::ToFractionalOffset(
    const TimelineOffset& timeline_offset) const {
  return GetTimelineRange().ToFractionalOffset(timeline_offset);
}

CSSNumericValue* ViewTimeline::startOffset() const {
  std::optional<ScrollOffsets> scroll_offsets = GetResolvedScrollOffsets();
  if (!scroll_offsets)
    return nullptr;

  DCHECK(GetResolvedZoom());
  return CSSUnitValues::px(scroll_offsets->start / GetResolvedZoom());
}

CSSNumericValue* ViewTimeline::endOffset() const {
  std::optional<ScrollOffsets> scroll_offsets = GetResolvedScrollOffsets();
  if (!scroll_offsets)
    return nullptr;

  DCHECK(GetResolvedZoom());
  return CSSUnitValues::px(scroll_offsets->end / GetResolvedZoom());
}

void ViewTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(style_dependant_start_inset_);
  visitor->Trace(style_dependant_end_inset_);
  ScrollTimeline::Trace(visitor);
}

}  // namespace blink
