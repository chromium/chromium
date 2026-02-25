// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timeline_trigger_range.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_trigger_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_timelinerangeoffset_undefined.h"
#include "third_party/blink/renderer/core/animation/animation_timeline.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/view_timeline.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

namespace {

constexpr double kTimelineTriggerBoundaryTolerance =
    1.f / LayoutUnit::kFixedPointDenominator;

constexpr char kActivationBoundaryDefault[] = "normal";
constexpr char kActiveBoundaryDefault[] = "auto";

bool ValidateBoundary(ExecutionContext* execution_context,
                      const TimelineTriggerRange::Boundary* boundary,
                      ExceptionState& exception_state,
                      double default_percent,
                      bool allow_auto) {
  if (boundary->IsString()) {
    CSSParserTokenStream stream(boundary->GetAsString());
    CSSParserLocalContext local_context =
        CSSParserLocalContext::CreateWithoutPropertyForAnimations();
    const CSSValue* value = css_parsing_utils::ConsumeAnimationRange(
        stream,
        *To<LocalDOMWindow>(execution_context)
             ->document()
             ->ElementSheet()
             .Contents()
             ->ParserContext(),
        local_context,
        /* default_offset_percent */ default_percent, allow_auto);
    if (!value || !stream.AtEnd()) {
      exception_state.ThrowTypeError(
          "TimelineTrigger range must be a name <length-percent> pair");
      return false;
    }
  } else {
    TimelineRangeOffset* value = boundary->GetAsTimelineRangeOffset();
    if (value->hasOffset()) {
      CSSNumericValue* offset = value->offset();
      const CSSPrimitiveValue* css_value =
          DynamicTo<CSSPrimitiveValue>(offset->ToCSSValue());

      if (!css_value) {
        exception_state.ThrowTypeError(
            "CSSNumericValue must be a length or percentage for "
            "TimelineTrigger range.");
        return false;
      }
    }
  }
  return true;
}

bool LessThanOrEqualWithinTolerance(double a, double b) {
  return a <= b + kTimelineTriggerBoundaryTolerance;
}

// Fuzzy matching is needed at the boundary of the scroll container so that we
// don't fail to detect entering a range due to round-off error.
bool WithinRange(double offset, double range_start, double range_end) {
  return LessThanOrEqualWithinTolerance(range_start, offset) &&
         LessThanOrEqualWithinTolerance(offset, range_end);
}

double ComputeTriggerBoundary(std::optional<TimelineOffset> offset,
                              double default_value,
                              const ScrollTimeline& timeline,
                              const TimelineRange::ScrollOffsets& range_offsets,
                              const Node& timeline_source) {
  if (offset) {
    // |range_offsets| is in physical pixels. Get the range values in CSS
    // pixels.
    double range_start_in_css = AdjustForAbsoluteZoom::AdjustScroll(
        range_offsets.start, *timeline_source.GetLayoutBox());
    double range_in_css = AdjustForAbsoluteZoom::AdjustScroll(
        range_offsets.end - range_offsets.start,
        *timeline_source.GetLayoutBox());

    LayoutUnit range_offset_in_css;
    if (timeline.IsViewTimeline()) {
      // |offset| is in CSS pixels but ToFractionalOffset works with Physical
      // pixels, adjust to physical pixels to get the fraction of the timeline
      // range.
      TimelineOffset offset_in_physical(
          offset->name,
          offset->offset.Zoom(
              timeline_source.GetLayoutBox()->StyleRef().EffectiveZoom()),
          offset->style_dependent_offset);

      double fraction =
          To<ViewTimeline>(timeline).ToFractionalOffset(offset_in_physical);
      range_offset_in_css = LayoutUnit(fraction * range_in_css);
    } else {
      range_offset_in_css =
          MinimumValueForLength(offset->offset, LayoutUnit(range_in_css));
    }

    return range_start_in_css + range_offset_in_css;
  }

  return default_value;
}

void InitializeUnsetBoundaryValues(
    V8UnionStringOrTimelineRangeOffsetOrUndefined* activation,
    V8UnionStringOrTimelineRangeOffsetOrUndefined* active,
    TimelineTriggerRange::Boundary** activation_boundary_out,
    TimelineTriggerRange::Boundary** active_boundary_out) {
  using Boundary = TimelineTriggerRange::Boundary;

  bool activation_undefined = !activation || activation->IsUndefined();
  bool active_undefined = !active || active->IsUndefined();

  const String activation_default(kActivationBoundaryDefault);
  const String active_default(kActiveBoundaryDefault);

  if (activation_undefined && active_undefined) {
    // Both are undefined: normal auto.
    *activation_boundary_out =
        MakeGarbageCollected<Boundary>(activation_default);
    *active_boundary_out = MakeGarbageCollected<Boundary>(active_default);
  } else if (activation_undefined) {
    // The activation boundary is undefined. If the active boundary is its
    // default value (auto), the activation boundary should also be its default
    // value (normal). Otherwise, it should be whatever the active boundary is.
    if (active->IsString()) {
      *active_boundary_out =
          MakeGarbageCollected<Boundary>(active->GetAsString());
      *activation_boundary_out =
          active->GetAsString() == active_default
              ? MakeGarbageCollected<Boundary>(activation_default)
              : *active_boundary_out;
    } else {
      *active_boundary_out =
          MakeGarbageCollected<Boundary>(active->GetAsTimelineRangeOffset());
      *activation_boundary_out = *active_boundary_out;
    }
  } else if (active_undefined) {
    // The active boundary is undefined. If the activation boundary is its
    // default value (normal), the active boundary should also be its default
    // value (auto). Otherwise, it should be whatever the activation boundary
    // is.
    if (activation->IsString()) {
      *activation_boundary_out =
          MakeGarbageCollected<Boundary>(activation->GetAsString());
      *active_boundary_out =
          activation->GetAsString() == activation_default
              ? MakeGarbageCollected<Boundary>(active_default)
              : *activation_boundary_out;
    } else {
      *activation_boundary_out = MakeGarbageCollected<Boundary>(
          activation->GetAsTimelineRangeOffset());
      *active_boundary_out = *activation_boundary_out;
    }
  } else {
    // Both values are set. They should be constructed from their respective
    // values.
    *activation_boundary_out =
        activation->IsString()
            ? MakeGarbageCollected<Boundary>(activation->GetAsString())
            : MakeGarbageCollected<Boundary>(
                  activation->GetAsTimelineRangeOffset());
    *active_boundary_out =
        active->IsString()
            ? MakeGarbageCollected<Boundary>(active->GetAsString())
            : MakeGarbageCollected<Boundary>(
                  active->GetAsTimelineRangeOffset());
  }
}

void InitializeBoundaryValues(
    const TimelineTriggerOptions* options,
    TimelineTriggerRange::Boundary** activation_start_out,
    TimelineTriggerRange::Boundary** active_start_out,
    TimelineTriggerRange::Boundary** activation_end_out,
    TimelineTriggerRange::Boundary** active_end_out) {
  InitializeUnsetBoundaryValues(
      options->hasActivationRangeStart() ? options->activationRangeStart()
                                         : nullptr,
      options->hasActiveRangeStart() ? options->activeRangeStart() : nullptr,
      activation_start_out, active_start_out);
  InitializeUnsetBoundaryValues(
      options->hasActivationRangeEnd() ? options->activationRangeEnd()
                                       : nullptr,
      options->hasActiveRangeEnd() ? options->activeRangeEnd() : nullptr,
      activation_end_out, active_end_out);
}

}  // namespace

TimelineTriggerRange::TimelineTriggerRange(AnimationTimeline* timeline,
                                           Boundary* activation_range_start,
                                           Boundary* activation_range_end,
                                           Boundary* active_range_start,
                                           Boundary* active_range_end)
    : timeline_(timeline),
      activation_range_start_(activation_range_start),
      activation_range_end_(activation_range_end),
      active_range_start_(active_range_start),
      active_range_end_(active_range_end) {}

/* static */
TimelineTriggerRange* TimelineTriggerRange::Create(
    ExecutionContext* execution_context,
    const TimelineTriggerOptions* options,
    ExceptionState& exception_state) {
  Boundary* active_start = nullptr;
  Boundary* activation_start = nullptr;
  Boundary* activation_end = nullptr;
  Boundary* active_end = nullptr;

  InitializeBoundaryValues(options, &activation_start, &active_start,
                           &activation_end, &active_end);

  if (!ValidateBoundary(execution_context, activation_start, exception_state, 0,
                        /*allow_auto=*/false) ||
      !ValidateBoundary(execution_context, activation_end, exception_state, 100,
                        /*allow_auto=*/false) ||
      !ValidateBoundary(execution_context, active_start, exception_state, 0,
                        /*allow_auto=*/true) ||
      !ValidateBoundary(execution_context, active_end, exception_state, 100,
                        /*allow_auto=*/true)) {
    return nullptr;
  }

  AnimationTimeline* timeline =
      (options->hasTimeline() ? options->timeline() : nullptr);
  if (!timeline) {
    timeline = &To<LocalDOMWindow>(execution_context)->document()->Timeline();
  }
  return MakeGarbageCollected<TimelineTriggerRange>(
      timeline, activation_start, activation_end, active_start, active_end);
}

AnimationTimeline* TimelineTriggerRange::timeline() {
  return timeline_.Get() ? timeline_.Get()->ExposedTimeline() : nullptr;
}
const TimelineTriggerRange::Boundary*
TimelineTriggerRange::activationRangeStart(
    ExecutionContext* execution_context) {
  return activation_range_start_;
}
const TimelineTriggerRange::Boundary* TimelineTriggerRange::activationRangeEnd(
    ExecutionContext* execution_context) {
  return activation_range_end_;
}
const TimelineTriggerRange::Boundary* TimelineTriggerRange::activeRangeStart(
    ExecutionContext* execution_context) {
  return active_range_start_;
}
const TimelineTriggerRange::Boundary* TimelineTriggerRange::activeRangeEnd(
    ExecutionContext* execution_context) {
  return active_range_end_;
}

Node* TimelineTriggerRange::ComputeBoundariesSource(
    const ScrollTimeline& timeline) {
  Node* timeline_source = timeline.ComputeResolvedSource();
  if (!timeline_source) {
    return nullptr;
  }

  if (IsA<LayoutView>(timeline_source->GetLayoutObject())) {
    // If the source is the root document, it isn't an "Element", so we need
    // to work with its scrollingElement
    timeline_source = To<Document>(timeline_source)->ScrollingElementNoLayout();
  }

  return timeline_source;
}

TimelineTriggerRange::TriggerBoundaries
TimelineTriggerRange::ComputeTriggerBoundaries(double current_offset,
                                               Element& timeline_source,
                                               const ScrollTimeline& timeline) {
  const auto timeline_state = timeline.ComputeTimelineState();

  TriggerBoundaries boundaries;

  ExceptionState exception_state(nullptr);
  std::optional<TimelineOffset> activation_start = TimelineOffset::Create(
      &timeline_source, activation_range_start_, 0, ASSERT_NO_EXCEPTION);
  std::optional<TimelineOffset> activation_end = TimelineOffset::Create(
      &timeline_source, activation_range_end_, 1, ASSERT_NO_EXCEPTION);
  TimelineOffsetOrAuto active_start = TimelineOffsetOrAuto::Create(
      &timeline_source, active_range_start_, 0, ASSERT_NO_EXCEPTION);
  TimelineOffsetOrAuto active_end = TimelineOffsetOrAuto::Create(
      &timeline_source, active_range_end_, 1, ASSERT_NO_EXCEPTION);

  // For a ScrollTimeline, these correspond to the min and max scroll offsets of
  // the associated scroll container.
  // For a ViewTimeline, these correspond to the cover 0% and cover 100%
  // respectively.
  const double default_start_position = AdjustForAbsoluteZoom::AdjustScroll(
      timeline_state.scroll_offsets->start, *timeline_source.GetLayoutBox());
  const double default_end_position = AdjustForAbsoluteZoom::AdjustScroll(
      timeline_state.scroll_offsets->end, *timeline_source.GetLayoutBox());

  boundaries.activation_start =
      ComputeTriggerBoundary(activation_start, default_start_position, timeline,
                             *timeline_state.scroll_offsets, timeline_source);
  boundaries.activation_end =
      ComputeTriggerBoundary(activation_end, default_end_position, timeline,
                             *timeline_state.scroll_offsets, timeline_source);

  if (active_start.IsAuto()) {
    // auto behavior: match the trigger range.
    boundaries.active_start = boundaries.activation_start;
  } else {
    // Note: a nullopt |offset| implies normal, which corresponds to the start
    // of the timeline's range: |timeline_state.scroll_offsets->start|.
    std::optional<TimelineOffset> offset = active_start.GetTimelineOffset();
    double default_active_start_offset = timeline_state.scroll_offsets->start;
    boundaries.active_start =
        ComputeTriggerBoundary(offset, default_active_start_offset, timeline,
                               *timeline_state.scroll_offsets, timeline_source);
  }

  if (active_end.IsAuto()) {
    boundaries.active_end = boundaries.activation_end;
  } else {
    std::optional<TimelineOffset> offset = active_end.GetTimelineOffset();
    double default_active_end_offset = timeline_state.scroll_offsets->end;
    boundaries.active_end =
        ComputeTriggerBoundary(offset, default_active_end_offset, timeline,
                               *timeline_state.scroll_offsets, timeline_source);
  }

  boundaries.current_offset = current_offset;

  return boundaries;
}

std::optional<TimelineTriggerState> TimelineTriggerRange::UpdateState() {
  last_snapshot_state_ = ComputeState().value_or(last_snapshot_state_);
  return last_snapshot_state_;
}

std::optional<TimelineTriggerState> TimelineTriggerRange::ComputeState() {
  if (!timeline_ || !timeline_->IsActive()) {
    return std::nullopt;
  }

  TriggerBoundaries boundaries;
  if (timeline_->IsProgressBased()) {
    ScrollTimeline* timeline =
        DynamicTo<ScrollTimeline>(timeline_->ExposedTimeline());
    if (!timeline) {
      return std::nullopt;
    }

    std::optional<double> current_offset = timeline->GetCurrentScrollPosition();
    if (!current_offset) {
      return std::nullopt;
    }

    Node* timeline_source = ComputeBoundariesSource(*timeline);

    current_offset = AdjustForAbsoluteZoom::AdjustScroll(
        *current_offset, *timeline_source->GetLayoutObject());

    boundaries = ComputeTriggerBoundaries(
        *current_offset, *To<Element>(timeline_source), *timeline);
  } else {
    // Only scroll-triggered animations are supported at the moment.
    // Return values that indicate that the a trigger with the document timeline
    // is always tripped.
    // return std::nullopt;
    boundaries = {.activation_start = -std::numeric_limits<double>::infinity(),
                  .activation_end = std::numeric_limits<double>::infinity(),
                  .current_offset = 0};
  }

  bool within_activation_range =
      WithinRange(boundaries.current_offset, boundaries.activation_start,
                  boundaries.activation_end);
  bool within_active_range =
      WithinRange(boundaries.current_offset, boundaries.active_start,
                  boundaries.active_end);

  State previous_state = last_snapshot_state_;
  State new_state = previous_state;

  if (within_activation_range) {
    new_state = State::kPrimary;
  } else if (!within_active_range) {
    new_state = State::kInverse;
  }

  if (new_state == previous_state) {
    return new_state;
  }

  if (previous_state == State::kIdle && new_state == State::kInverse) {
    // The first transition must be to the primary state.
    return previous_state;
  }

  return new_state;
}

std::optional<TimelineTriggerRange::CcBoundaries>
TimelineTriggerRange::ComputeCcBoundaries(cc::AnimationTimeline* cc_timeline) {
  if (!timeline_->IsProgressBased()) {
    // Non progress based triggers are always in a tripped state,
    // no need to composite the trigger so we should not be in this function.
    return std::nullopt;
  }

  ScrollTimeline* timeline =
      DynamicTo<ScrollTimeline>(timeline_->ExposedTimeline());
  if (!timeline) {
    return std::nullopt;
  }

  Node* timeline_source = ComputeBoundariesSource(*timeline);
  if (!timeline_source) {
    return std::nullopt;
  }

  TriggerBoundaries boundaries = ComputeTriggerBoundaries(
      /*(unused) current_offset=*/0, *To<Element>(timeline_source), *timeline);

  // Blink boundaries are calculated in CSS pixels, but cc ScrollTimeline
  // offsets are in physical pixels. So, scale the blink boundaries into
  // physical pixels.
  // TODO(451238244): Update boundaries whenever zoom factor changes.
  double zoom_factor =
      timeline_source->GetLayoutBox()->StyleRef().EffectiveZoom();
  boundaries.activation_start = boundaries.activation_start * zoom_factor;
  boundaries.activation_end = boundaries.activation_end * zoom_factor;
  boundaries.active_start = boundaries.active_start * zoom_factor;
  boundaries.active_end = boundaries.active_end * zoom_factor;

  const double ms_per_pixel_multiplier =
      cc::ScrollTimeline::kScrollTimelineMicrosecondsPerPixel;

  // In cc, we evaluate "enter" and "exit" using the current time of the
  // timeline. The current time of the timeline is measured relative to the
  // start of the timeline's range which, in the case of a view timeline, is the
  // start of its cover range.
  double timeline_start_offset =
      static_cast<cc::ScrollTimeline*>(cc_timeline)->pending_offsets()->start;

  CcBoundaries cc_boundaries;
  cc_boundaries.activation_start_time =
      base::TimeTicks() +
      base::Microseconds((boundaries.activation_start - timeline_start_offset) *
                         ms_per_pixel_multiplier);
  cc_boundaries.activation_end_time =
      base::TimeTicks() +
      base::Microseconds((boundaries.activation_end - timeline_start_offset) *
                         ms_per_pixel_multiplier);
  cc_boundaries.active_start_time =
      base::TimeTicks() +
      base::Microseconds((boundaries.active_start - timeline_start_offset) *
                         ms_per_pixel_multiplier);
  cc_boundaries.active_end_time =
      base::TimeTicks() +
      base::Microseconds((boundaries.active_end - timeline_start_offset) *
                         ms_per_pixel_multiplier);

  return cc_boundaries;
}

void TimelineTriggerRange::Trace(Visitor* visitor) const {
  visitor->Trace(timeline_);
  visitor->Trace(activation_range_start_);
  visitor->Trace(activation_range_end_);
  visitor->Trace(active_range_start_);
  visitor->Trace(active_range_end_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
