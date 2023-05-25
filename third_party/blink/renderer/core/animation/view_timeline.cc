// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/view_timeline.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalueorstringsequence_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline_options.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/animation/view_timeline_attachment.h"
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
  if (!value)
    return false;

  if (const CSSPrimitiveValue* css_primitive_value =
          DynamicTo<CSSPrimitiveValue>(value)) {
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

  if (inset_value->IsPrimitiveValue()) {
    ElementResolveContext element_resolve_context(*subject);
    Document& document = subject->GetDocument();
    // Flags can be ignored, because we re-resolve any value that's not px or
    // percentage, see IsStyleDependent.
    CSSToLengthConversionData::Flags ignored_flags = 0;
    CSSToLengthConversionData length_conversion_data(
        subject->ComputedStyleRef(), element_resolve_context.ParentStyle(),
        element_resolve_context.RootElementStyle(), document.GetLayoutView(),
        CSSToLengthConversionData::ContainerSizes(subject),
        subject->GetComputedStyle()->EffectiveZoom(), ignored_flags);

    return DynamicTo<CSSPrimitiveValue>(inset_value)
        ->ConvertToLength(length_conversion_data);
  }

  NOTREACHED();
  return Length(Length::Type::kAuto);
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

  absl::optional<const CSSValue*> start_inset_value;
  absl::optional<const CSSValue*> end_inset_value;
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
      &document, TimelineAttachment::kLocal, subject, axis,
      TimelineInset(inset_start_side, inset_end_side));

  if (start_inset_value && IsStyleDependent(start_inset_value.value()))
    view_timeline->style_dependant_start_inset_ = start_inset_value.value();
  if (end_inset_value && IsStyleDependent(end_inset_value.value()))
    view_timeline->style_dependant_end_inset_ = end_inset_value.value();

  view_timeline->UpdateSnapshot();
  return view_timeline;
}

ViewTimeline::ViewTimeline(Document* document,
                           TimelineAttachment attachment,
                           Element* subject,
                           ScrollAxis axis,
                           TimelineInset inset)
    : ScrollTimeline(
          document,
          attachment,
          attachment == TimelineAttachment::kDefer
              ? nullptr
              : MakeGarbageCollected<ViewTimelineAttachment>(subject,
                                                             axis,
                                                             inset)) {}

void ViewTimeline::CalculateOffsets(PaintLayerScrollableArea* scrollable_area,
                                    ScrollOrientation physical_orientation,
                                    TimelineState* state) const {
  // Do not call this method with an unresolved timeline.
  // Called from ScrollTimeline::ComputeTimelineState, which has safeguard.
  // Any new call sites will require a similar safeguard.
  DCHECK(state->resolved_source);
  DCHECK(ComputeIsResolved(state->resolved_source));
  DCHECK(subject());

  absl::optional<LayoutSize> subject_size = SubjectSize();
  absl::optional<gfx::PointF> subject_position =
      SubjectPosition(state->resolved_source);
  DCHECK(subject_position);
  DCHECK(subject_size);

  // TODO(crbug.com/1448294): Currently this only handles the case where
  // subject becomes stuck during the "contain" range, i.e. it is not stuck
  // while partially or fully outside the viewport (during entry/exit, or
  // before entry).

  // TODO(crbug.com/1448801): Handle nested sticky elements.

  LayoutUnit sticky_max_top;
  LayoutUnit sticky_max_right;
  LayoutUnit sticky_max_bottom;
  LayoutUnit sticky_max_left;
  GetSubjectMaxStickyOffsets(sticky_max_top, sticky_max_right,
                             sticky_max_bottom, sticky_max_left,
                             state->resolved_source);

  double target_offset_min = physical_orientation == kHorizontalScroll
                                 ? subject_position->x() + sticky_max_right
                                 : subject_position->y() + sticky_max_bottom;
  double target_offset_max = physical_orientation == kHorizontalScroll
                                 ? subject_position->x() + sticky_max_left
                                 : subject_position->y() + sticky_max_top;

  double target_size;
  LayoutUnit viewport_size;
  if (physical_orientation == kHorizontalScroll) {
    target_size = subject_size->Width().ToDouble();
    viewport_size = scrollable_area->LayoutContentRect().Width();
  } else {
    target_size = subject_size->Height().ToDouble();
    viewport_size = scrollable_area->LayoutContentRect().Height();
  }

  Element* source = CurrentAttachment()->ComputeSourceNoLayout();
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

  double start_offset =
      target_offset_min - viewport_size_double + end_side_inset;
  double end_offset = target_offset_max + target_size - start_side_inset;

  state->scroll_offsets =
      absl::make_optional<ScrollOffsets>(start_offset, end_offset);
  state->view_offsets = absl::make_optional<ScrollOffsets>(
      target_offset_min, target_offset_min + target_size);
}

absl::optional<LayoutSize> ViewTimeline::SubjectSize() const {
  if (!subject()) {
    return absl::nullopt;
  }
  LayoutBox* subject_layout_box = subject()->GetLayoutBox();
  if (!subject_layout_box) {
    return absl::nullopt;
  }

  return subject_layout_box->Size();
}

absl::optional<gfx::PointF> ViewTimeline::SubjectPosition(
    Node* resolved_source) const {
  if (!subject() || !resolved_source) {
    return absl::nullopt;
  }
  LayoutBox* subject_layout_box = subject()->GetLayoutBox();
  LayoutBox* source_layout_box = resolved_source->GetLayoutBox();
  if (!subject_layout_box || !source_layout_box) {
    return absl::nullopt;
  }
  MapCoordinatesFlags flags =
      kIgnoreScrollOffset | kIgnoreStickyOffset | kIgnoreTransforms;
  gfx::PointF subject_pos =
      gfx::PointF(subject_layout_box->LocalToAncestorPoint(
          PhysicalOffset(), source_layout_box, flags));

  // We call LayoutObject::ClientLeft/Top directly and avoid
  // Element::clientLeft/Top because:
  //
  // - We may reach this function during style resolution,
  //   and clientLeft/Top also attempt to update style/layout.
  // - Those functions return the unzoomed values, and we require the zoomed
  //   values.
  return gfx::PointF(subject_pos.x() - source_layout_box->ClientLeft().Round(),
                     subject_pos.y() - source_layout_box->ClientTop().Round());
}

void ViewTimeline::GetSubjectMaxStickyOffsets(LayoutUnit& top,
                                              LayoutUnit& right,
                                              LayoutUnit& bottom,
                                              LayoutUnit& left,
                                              Node* resolved_source) const {
  if (!subject()) {
    return;
  }

  LayoutBox* subject_layout_box = subject()->GetLayoutBox();
  LayoutBox* source_layout_box = resolved_source->GetLayoutBox();
  if (!subject_layout_box || !source_layout_box) {
    return;
  }

  const LayoutBoxModelObject* sticky_container =
      subject_layout_box->FindFirstStickyContainer(source_layout_box);
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
  const PhysicalRect& sticky =
      constraints->scroll_container_relative_sticky_box_rect;

  // The maximum sticky offset from each offset property is the available room
  // from the opposite edge of the sticky element in its static position.
  if (constraints->is_anchored_top) {
    top = container.Bottom() - sticky.Bottom();
  }
  if (constraints->is_anchored_right) {
    right = container.X() - sticky.X();
  }
  if (constraints->is_anchored_bottom) {
    bottom = container.Y() - sticky.Y();
  }
  if (constraints->is_anchored_left) {
    left = container.Right() - sticky.Right();
  }
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

  absl::optional<base::TimeDelta> current_time = CurrentPhaseAndTime().time;
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
  return CurrentAttachment() ? CurrentAttachment()->GetReferenceElement()
                             : nullptr;
}

bool ViewTimeline::Matches(TimelineAttachment attachment_type,
                           Element* subject,
                           ScrollAxis axis,
                           const TimelineInset& inset) const {
  if (!ScrollTimeline::Matches(attachment_type, ReferenceType::kNearestAncestor,
                               /* reference_element */ subject, axis)) {
    return false;
  }
  if (GetTimelineAttachment() == TimelineAttachment::kDefer) {
    return attachment_type == TimelineAttachment::kDefer;
  }
  const auto* attachment =
      DynamicTo<ViewTimelineAttachment>(CurrentAttachment());
  DCHECK(attachment);
  return attachment->GetInset() == inset;
}

const TimelineInset& ViewTimeline::GetInset() const {
  if (const auto* attachment =
          DynamicTo<ViewTimelineAttachment>(CurrentAttachment())) {
    return attachment->GetInset();
  }

  DEFINE_STATIC_LOCAL(TimelineInset, default_inset, ());
  return default_inset;
}

double ViewTimeline::ToFractionalOffset(
    const TimelineOffset& timeline_offset) const {
  return GetTimelineRange().ToFractionalOffset(timeline_offset);
}

CSSNumericValue* ViewTimeline::startOffset() const {
  absl::optional<ScrollOffsets> scroll_offsets = GetResolvedScrollOffsets();
  if (!scroll_offsets)
    return nullptr;

  DCHECK(GetResolvedZoom());
  return CSSUnitValues::px(scroll_offsets->start / GetResolvedZoom());
}

CSSNumericValue* ViewTimeline::endOffset() const {
  absl::optional<ScrollOffsets> scroll_offsets = GetResolvedScrollOffsets();
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
