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
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

using InsetValueSequence =
    const HeapVector<Member<V8UnionCSSNumericValueOrString>>;

namespace {

double ComputeOffset(Element* source_element,
                     LayoutBox* subject_layout,
                     LayoutBox* source_layout,
                     ScrollOrientation physical_orientation) {
  MapCoordinatesFlags flags = kIgnoreScrollOffset;
  gfx::PointF point = gfx::PointF(subject_layout->LocalToAncestorPoint(
      PhysicalOffset(), source_layout, flags));

  // We can not call the regular clientLeft/Top functions here, because we
  // may reach this function during style resolution, and clientLeft/Top
  // also attempt to update style/layout.
  if (physical_orientation == kHorizontalScroll)
    return point.x() - source_element->ClientLeftNoLayout();
  else
    return point.y() - source_element->ClientTopNoLayout();
}

bool IsBlockDirection(ViewTimeline::ScrollAxis axis, WritingMode writing_mode) {
  switch (axis) {
    case ViewTimeline::ScrollAxis::kBlock:
      return true;
    case ViewTimeline::ScrollAxis::kInline:
      return false;
    case ViewTimeline::ScrollAxis::kHorizontal:
      return !blink::IsHorizontalWritingMode(writing_mode);
    case ViewTimeline::ScrollAxis::kVertical:
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
ViewTimeline::Inset ResolveAuto(const ViewTimeline::Inset& inset,
                                Element& source,
                                ViewTimeline::ScrollAxis axis) {
  const ComputedStyle* style = source.GetComputedStyle();
  if (!style)
    return inset;

  const Length& start = inset.start_side;
  const Length& end = inset.end_side;

  if (IsBlockDirection(axis, style->GetWritingMode())) {
    return ViewTimeline::Inset(
        start.IsAuto() ? style->ScrollPaddingBlockStart() : start,
        end.IsAuto() ? style->ScrollPaddingBlockEnd() : end);
  }
  return ViewTimeline::Inset(
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

  Inset inset;
  inset.start_side = InsetValueToLength(start_inset_value.value_or(nullptr),
                                        subject, Length(Length::Type::kFixed));
  inset.end_side = InsetValueToLength(end_inset_value.value_or(nullptr),
                                      subject, inset.start_side);

  ViewTimeline* view_timeline =
      MakeGarbageCollected<ViewTimeline>(&document, subject, axis, inset);

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
                           Inset inset)
    : ScrollTimeline(document, ReferenceType::kNearestAncestor, subject, axis),
      inset_(inset) {
  // Ensure that the timeline stays alive as long as the subject.
  if (subject)
    subject->RegisterScrollTimeline(this);
}

AnimationTimeDelta ViewTimeline::CalculateIntrinsicIterationDuration(
    const Animation* animation,
    const Timing& timing) {
  absl::optional<AnimationTimeDelta> duration = GetDuration();

  // Only run calculation for progress based scroll timelines
  if (duration && timing.iteration_count > 0) {
    double active_interval = 1;

    double start = animation->GetRangeStart()
                       ? ToFractionalOffset(animation->GetRangeStart().value())
                       : 0;
    double end = animation->GetRangeEnd()
                     ? ToFractionalOffset(animation->GetRangeEnd().value())
                     : 1;

    active_interval -= start;
    active_interval -= (1 - end);

    // Start and end delays are proportional to the active interval.
    double start_delay = timing.start_delay.relative_delay.value_or(0);
    double end_delay = timing.end_delay.relative_delay.value_or(0);
    double delay = start_delay + end_delay;

    if (delay >= 1) {
      return AnimationTimeDelta();
    }

    active_interval *= (1 - delay);
    return duration.value() * active_interval / timing.iteration_count;
  }
  return AnimationTimeDelta();
}

absl::optional<ScrollTimeline::ScrollOffsets> ViewTimeline::CalculateOffsets(
    PaintLayerScrollableArea* scrollable_area,
    ScrollOrientation physical_orientation) const {
  // Do not call this method with an inactive timeline.
  // Called from ScrollTimeline::ComputeTimelineState, which has safeguard.
  // Any new call sites will require a similar safeguard.
  DCHECK(ComputeIsActive());
  DCHECK(subject());
  LayoutBox* layout_box = subject()->GetLayoutBox();
  DCHECK(layout_box);
  Element* source = SourceInternal();
  Node* resolved_source = ResolvedSource();
  DCHECK(source);
  DCHECK(resolved_source);
  LayoutBox* source_layout = resolved_source->GetLayoutBox();
  DCHECK(source_layout);

  LayoutUnit viewport_size;

  target_offset_ =
      ComputeOffset(source, layout_box, source_layout, physical_orientation);

  if (physical_orientation == kHorizontalScroll) {
    target_size_ = layout_box->Size().Width().ToDouble();
    viewport_size = scrollable_area->LayoutContentRect().Width();
  } else {
    target_size_ = layout_box->Size().Height().ToDouble();
    viewport_size = scrollable_area->LayoutContentRect().Height();
  }

  viewport_size_ = viewport_size.ToDouble();

  Inset inset = ResolveAuto(inset_, *source, GetAxis());

  // Update inset lengths if style dependent.
  if (style_dependant_start_inset_) {
    inset.start_side = InsetValueToLength(style_dependant_start_inset_,
                                          subject(), Length::Fixed());
  }
  if (style_dependant_end_inset_) {
    inset.end_side = InsetValueToLength(style_dependant_end_inset_, subject(),
                                        Length::Fixed());
  }

  // Note that the end_side_inset is used to adjust the start offset,
  // and the start_side_inset is used to adjust the end offset.
  // This is because "start side" refers to logical start side [1] of the
  // source box, where as "start offset" refers to the start of the timeline,
  // and similarly for end side/offset.
  // [1] https://drafts.csswg.org/css-writing-modes-4/#css-start
  end_side_inset_ = ComputeInset(inset.end_side, viewport_size);
  start_side_inset_ = ComputeInset(inset.start_side, viewport_size);

  double start_offset = target_offset_ - viewport_size_ + end_side_inset_;
  double end_offset = target_offset_ + target_size_ - start_side_inset_;

  if (start_offset != start_offset_ || end_offset != end_offset_) {
    start_offset_ = start_offset;
    end_offset_ = end_offset;

    for (auto animation : GetAnimations())
      animation->InvalidateNormalizedTiming();
  }

  return absl::make_optional<ScrollOffsets>(start_offset, end_offset);
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

double ViewTimeline::ToFractionalOffset(
    const TimelineOffset& timeline_offset) const {
  // https://drafts.csswg.org/scroll-animations-1/#view-timelines-ranges
  double align_subject_start_view_end =
      target_offset_ - viewport_size_ + end_side_inset_;
  double align_subject_end_view_start =
      target_offset_ + target_size_ - start_side_inset_;
  double align_subject_start_view_start =
      align_subject_end_view_start - target_size_;
  double align_subject_end_view_end =
      align_subject_start_view_end + target_size_;
  // Timeline is inactive if scroll range is zero.
  double range = align_subject_end_view_start - align_subject_start_view_end;
  if (!range)
    return 0;

  double range_start = 0;
  double range_end = 0;
  switch (timeline_offset.name) {
    case TimelineOffset::NamedRange::kNone:
    case TimelineOffset::NamedRange::kCover:
      // Represents the full range of the view progress timeline:
      //   0% progress represents the position at which the start border edge of
      //   the element’s principal box coincides with the end edge of its view
      //   progress visibility range.
      //   100% progress represents the position at which the end border edge of
      //   the element’s principal box coincides with the start edge of its view
      //   progress visibility range.
      range_start = align_subject_start_view_end;
      range_end = align_subject_end_view_start;
      break;

    case TimelineOffset::NamedRange::kContain:
      // Represents the range during which the principal box is either fully
      // contained by, or fully covers, its view progress visibility range
      // within the scrollport.
      // 0% progress represents the earlier position at which:
      //   1. the start border edge of the element’s principal box coincides
      //      with the start edge of its view progress visibility range.
      //   2. the end border edge of the element’s principal box coincides with
      //      the end edge of its view progress visibility range.
      // 100% progress represents the later position at which:
      //   1. the start border edge of the element’s principal box coincides
      //      with the start edge of its view progress visibility range.
      //   2. the end border edge of the element’s principal box coincides with
      //      the end edge of its view progress visibility range.
      range_start =
          std::min(align_subject_start_view_start, align_subject_end_view_end);
      range_end =
          std::max(align_subject_start_view_start, align_subject_end_view_end);
      break;

    case TimelineOffset::NamedRange::kEntry:
      // Represents the range during which the principal box is entering the
      // view progress visibility range.
      //   0% is equivalent to 0% of the cover range.
      //   100% is equivalent to 0% of the contain range.
      range_start = align_subject_start_view_end;
      range_end =
          std::min(align_subject_start_view_start, align_subject_end_view_end);
      break;

    case TimelineOffset::NamedRange::kEntryCrossing:
      // Represents the range during which the principal box is crossing the
      // entry edge of the viewport.
      //   0% is equivalent to 0% of the cover range.
      range_start = align_subject_start_view_end;
      range_end = align_subject_end_view_end;
      break;

    case TimelineOffset::NamedRange::kExit:
      // Represents the range during which the principal box is exiting the view
      // progress visibility range.
      //   0% is equivalent to 100% of the contain range.
      //   100% is equivalent to 100% of the cover range.
      range_start =
          std::max(align_subject_start_view_start, align_subject_end_view_end);
      range_end = align_subject_end_view_start;
      break;

    case TimelineOffset::NamedRange::kExitCrossing:
      // Represents the range during which the principal box is exiting the view
      // progress visibility range.
      //   100% is equivalent to 100% of the cover range.
      range_start = align_subject_start_view_start;
      range_end = align_subject_end_view_start;
      break;
  }

  DCHECK(range_end >= range_start);
  DCHECK_GT(range, 0);

  double offset =
      range_start + MinimumValueForLength(timeline_offset.offset,
                                          LayoutUnit(range_end - range_start));
  return (offset - align_subject_start_view_end) / range;
}

AnimationTimeline::TimeDelayPair ViewTimeline::ComputeEffectiveAnimationDelays(
    const Animation* animation,
    const Timing& timing) const {
  absl::optional<AnimationTimeDelta> duration = GetDuration();
  if (!duration)
    return std::make_pair(AnimationTimeDelta(), AnimationTimeDelta());
  double range_start =
      animation->GetRangeStart()
          ? ToFractionalOffset(animation->GetRangeStart().value())
          : 0;
  double range_end = animation->GetRangeEnd()
                         ? ToFractionalOffset(animation->GetRangeEnd().value())
                         : 1;

  // Timeline range is relative to cover 0% to 100% range.
  double timeline_range = range_end - range_start;

  // Animation delays are effectively insets on the animation range.
  // Delays must be expressed as percentages. Time-based delays are ignored.
  double start_delay =
      timing.start_delay.relative_delay.value_or(0) * timeline_range;
  double end_delay =
      timing.end_delay.relative_delay.value_or(0) * timeline_range;

  // TODO(kevers): Check if additional safeguards are required for delays
  // summing > 100%.
  return std::make_pair((range_start + start_delay) * duration.value(),
                        (1 - range_end + end_delay) * duration.value());
}

CSSNumericValue* ViewTimeline::startOffset() const {
  absl::optional<ScrollOffsets> scroll_offsets = GetResolvedScrollOffsets();
  if (!scroll_offsets)
    return nullptr;

  return CSSUnitValues::px(scroll_offsets->start);
}

CSSNumericValue* ViewTimeline::endOffset() const {
  absl::optional<ScrollOffsets> scroll_offsets = GetResolvedScrollOffsets();
  if (!scroll_offsets)
    return nullptr;

  return CSSUnitValues::px(scroll_offsets->end);
}

void ViewTimeline::Trace(Visitor* visitor) const {
  visitor->Trace(style_dependant_start_inset_);
  visitor->Trace(style_dependant_end_inset_);
  ScrollTimeline::Trace(visitor);
}

}  // namespace blink
