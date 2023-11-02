// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/view_timeline.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_cssnumericvalue_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline_options.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

using InsetValueSequence =
    const HeapVector<Member<V8UnionCSSNumericValueOrString>>;

namespace {

double ComputeOffset(LayoutBox* subject,
                     LayoutBox* source,
                     ScrollOrientation physical_orientation) {
  Element* source_element = DynamicTo<Element>(source->GetNode());
  MapCoordinatesFlags flags = kIgnoreScrollOffset;
  gfx::PointF point = gfx::PointF(
      subject->LocalToAncestorPoint(PhysicalOffset(), source, flags));

  // We can not call the regular clientLeft/Top functions here, because we
  // may reach this function during style resolution, and clientLeft/Top
  // also attempt to update style/layout.
  if (physical_orientation == kHorizontalScroll)
    return point.x() - source_element->ClientLeftNoLayout();
  else
    return point.y() - source_element->ClientTopNoLayout();
}

bool IsBlockDirection(ScrollTimeline::ScrollDirection direction,
                      WritingMode writing_mode) {
  using ScrollDirection = ScrollTimeline::ScrollDirection;
  switch (direction) {
    case ScrollDirection::kBlock:
      return true;
    case ScrollDirection::kInline:
      return false;
    case ScrollDirection::kHorizontal:
      return !blink::IsHorizontalWritingMode(writing_mode);
    case ScrollDirection::kVertical:
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
                                ScrollTimeline::ScrollDirection direction) {
  const ComputedStyle* style = source.GetComputedStyle();
  if (!style)
    return inset;

  const Length& start = inset.start_side;
  const Length& end = inset.end_side;

  if (IsBlockDirection(direction, style->GetWritingMode())) {
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

Length ParseLength(const InsetValueSequence& array,
                   wtf_size_t index,
                   Length default_value,
                   ExceptionState& exception_state) {
  if (index >= array.size())
    return default_value;

  V8UnionCSSNumericValueOrString* value = array[index];
  if (value->IsString()) {
    if (value->GetAsString() != "auto") {
      exception_state.ThrowTypeError("inset must be CSSNumericValue or auto");
    }
    // TODO(crbug.com/1317765): Not the correct default value.
    // "auto" indicates to use the value of scroll-padding.
    // Resolve once handled in pure CSS implementation.
    return default_value;
  } else {
    CSSNumericValue* numeric_value = value->GetAsCSSNumericValue();
    CSSUnitValue* value_as_percentage =
        numeric_value->to(CSSPrimitiveValue::UnitType::kPercentage);
    if (value_as_percentage)
      return Length(value_as_percentage->value(), Length::Type::kPercent);

    CSSUnitValue* value_as_px =
        numeric_value->to(CSSPrimitiveValue::UnitType::kPixels);
    if (value_as_px)
      return Length(value_as_px->value(), Length::Type::kFixed);

    // TODO(crbug.com/1317765): Support other length units and calc?
    // Resolve once handled in pure CSS implementation.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Unsupported inset: value must be percent or px");
    return default_value;
  }
}

}  // end namespace

ViewTimeline* ViewTimeline::Create(Document& document,
                                   ViewTimelineOptions* options,
                                   ExceptionState& exception_state) {
  Element* subject = options->subject();

  ScrollDirection orientation;
  if (!StringToScrollDirection(options->axis(), orientation)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid axis");
    return nullptr;
  }
  if (subject) {
    // This ensures that Client[Left,Top]NoLayout (reached via SnapshotState)
    // returns up-to-date information.
    document.UpdateStyleAndLayoutForNode(subject,
                                         DocumentUpdateReason::kJavaScript);
  }

  // Parse insets.
  const InsetValueSequence inset_array = options->inset();
  if (inset_array.size() > 2) {
    exception_state.ThrowTypeError("Invalid inset");
    return nullptr;
  }
  Inset inset;
  inset.start_side = ParseLength(inset_array, 0, Length(Length::Type::kFixed),
                                 exception_state);
  inset.end_side =
      ParseLength(inset_array, 1, inset.start_side, exception_state);

  ViewTimeline* view_timeline = MakeGarbageCollected<ViewTimeline>(
      &document, subject, orientation, inset);
  view_timeline->SnapshotState();
  return view_timeline;
}

ViewTimeline::ViewTimeline(Document* document,
                           Element* subject,
                           ScrollDirection orientation,
                           Inset inset)
    : ScrollTimeline(document,
                     ReferenceType::kNearestAncestor,
                     subject,
                     orientation),
      inset_(inset) {
  // Ensure that the timeline stays alive as long as the subject.
  if (subject)
    subject->RegisterScrollTimeline(this);
}

AnimationTimeDelta ViewTimeline::CalculateIntrinsicIterationDuration(
    const Timing& timing) {
  absl::optional<AnimationTimeDelta> duration = GetDuration();

  // Only run calculation for progress based scroll timelines
  if (duration && timing.iteration_count > 0) {
    double active_interval = 1;
    absl::optional<double> start_delay = ToFractionalOffset(timing.start_delay);
    if (start_delay)
      active_interval -= start_delay.value();
    absl::optional<double> end_delay = ToFractionalOffset(timing.end_delay);
    if (end_delay)
      active_interval -= (1 - end_delay.value());
    return duration.value() * active_interval / timing.iteration_count;
  }
  return AnimationTimeDelta();
}

absl::optional<ScrollTimeline::ScrollOffsets> ViewTimeline::CalculateOffsets(
    PaintLayerScrollableArea* scrollable_area,
    ScrollOrientation physical_orientation) const {
  DCHECK(subject());
  LayoutBox* layout_box = subject()->GetLayoutBox();
  DCHECK(layout_box);
  Element* source = SourceInternal();
  DCHECK(source);
  LayoutBox* source_layout = source->GetLayoutBox();
  DCHECK(source_layout);

  LayoutUnit viewport_size;

  target_offset_ =
      ComputeOffset(layout_box, source_layout, physical_orientation);
  if (physical_orientation == kHorizontalScroll) {
    target_size_ = layout_box->Size().Width().ToDouble();
    viewport_size = scrollable_area->LayoutContentRect().Width();
  } else {
    target_size_ = layout_box->Size().Height().ToDouble();
    viewport_size = scrollable_area->LayoutContentRect().Height();
  }

  viewport_size_ = viewport_size.ToDouble();

  Inset inset = ResolveAuto(inset_, *source, GetOrientation());
  // Note that the end_side_inset is used to adjust the start offset,
  // and the start_side_inset is used to adjust the end offset.
  // This is because "start side" refers to logical start side [1] of the
  // source box, where as "start offset" refers to the start of the timeline,
  // and similarly for end side/offset.
  // [1] https://drafts.csswg.org/css-writing-modes-4/#css-start
  end_side_inset_ = ComputeInset(inset.end_side, viewport_size);
  start_side_inset_ = ComputeInset(inset.start_side, viewport_size);

  // TODO(crbug.com/1329159): If any of the sizes change we need to invalidate
  // timing normalization.
  double start_offset = target_offset_ - viewport_size_ + end_side_inset_;
  double end_offset = target_offset_ + target_size_ - start_side_inset_;
  return absl::make_optional<ScrollOffsets>(start_offset, end_offset);
}

absl::optional<double> ViewTimeline::ToFractionalOffset(
    const Timing::Delay& delay) const {
  absl::optional<double> result;
  if (delay.phase == Timing::TimelineNamedPhase::kNone)
    return result;

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

  double phase_start = 0;
  double phase_end = 0;
  switch (delay.phase) {
    case Timing::TimelineNamedPhase::kCover:
      // Represents the full range of the view progress timeline:
      //   0% progress represents the position at which the start border edge of
      //   the element’s principal box coincides with the end edge of its view
      //   progress visibility range.
      //   100% progress represents the position at which the end border edge of
      //   the element’s principal box coincides with the start edge of its view
      //   progress visibility range.
      phase_start = align_subject_start_view_end;
      phase_end = align_subject_end_view_start;
      break;

    case Timing::TimelineNamedPhase::kContain:
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
      phase_start =
          std::min(align_subject_start_view_start, align_subject_end_view_end);
      phase_end =
          std::max(align_subject_start_view_start, align_subject_end_view_end);
      break;

    case Timing::TimelineNamedPhase::kEnter:
      // Represents the range during which the principal box is entering the
      // view progress visibility range.
      //   0% is equivalent to 0% of the cover range.
      //   100% is equivalent to 0% of the contain range.
      phase_start = align_subject_start_view_end;
      phase_end =
          std::min(align_subject_start_view_start, align_subject_end_view_end);
      break;

    case Timing::TimelineNamedPhase::kExit:
      // Represents the range during which the principal box is exiting the view
      // progress visibility range.
      //   0% is equivalent to 100% of the contain range.
      //   100% is equivalent to 100% of the cover range.
      phase_start =
          std::max(align_subject_start_view_start, align_subject_end_view_end);
      phase_end = align_subject_end_view_start;
      break;

    case Timing::TimelineNamedPhase::kNone:
      NOTREACHED();
  }

  DCHECK(phase_end >= phase_start);
  DCHECK_GT(range, 0);
  double offset =
      phase_start + (phase_end - phase_start) * delay.relative_offset;
  return (offset - align_subject_start_view_end) / range;
}

AnimationTimeline::TimeDelayPair ViewTimeline::TimelineOffsetsToTimeDelays(
    const Timing& timing) const {
  absl::optional<AnimationTimeDelta> duration = GetDuration();
  if (!duration)
    return std::make_pair(AnimationTimeDelta(), AnimationTimeDelta());

  absl::optional<double> start_fraction =
      ToFractionalOffset(timing.start_delay);
  absl::optional<double> end_fraction = ToFractionalOffset(timing.end_delay);
  return std::make_pair(start_fraction.value_or(0) * duration.value(),
                        (1 - end_fraction.value_or(1)) * duration.value());
}

}  // namespace blink
