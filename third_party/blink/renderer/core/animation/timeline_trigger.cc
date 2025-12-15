// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timeline_trigger.h"

#include "cc/animation/animation_id_provider.h"
#include "cc/animation/timeline_trigger.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_animation_trigger_behavior.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/deferred_timeline.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/animation/timeline_range.h"
#include "third_party/blink/renderer/core/animation/view_timeline.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

namespace {

constexpr double kTimelineTriggerBoundaryTolerance =
    1.f / LayoutUnit::kFixedPointDenominator;

bool ValidateBoundary(ExecutionContext* execution_context,
                      const TimelineTrigger::RangeBoundary* boundary,
                      ExceptionState& exception_state,
                      double default_percent,
                      bool allow_auto) {
  if (boundary->IsString()) {
    CSSParserTokenStream stream(boundary->GetAsString());
    const CSSValue* value = css_parsing_utils::ConsumeAnimationRange(
        stream,
        *To<LocalDOMWindow>(execution_context)
             ->document()
             ->ElementSheet()
             .Contents()
             ->ParserContext(),
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
                              Element& timeline_source) {
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

}  // namespace

TimelineTrigger::TimelineTrigger(AnimationTimeline* timeline,
                                 RangeBoundary* range_start,
                                 RangeBoundary* range_end,
                                 RangeBoundary* exit_range_start,
                                 RangeBoundary* exit_range_end,
                                 Element* owning_element)
    : timeline_(timeline),
      range_start_(range_start),
      range_end_(range_end),
      exit_range_start_(exit_range_start),
      exit_range_end_(exit_range_end) {
  owning_element_ = owning_element;

  if (timeline_) {
    timeline_->GetDocument()->GetDocumentAnimations().AddAnimationTrigger(
        *this);
  }
  // A default trigger will need to trip immediately.
  Update();
}

/* static */
TimelineTrigger* TimelineTrigger::Create(ExecutionContext* execution_context,
                                         TimelineTriggerOptions* options,
                                         ExceptionState& exception_state) {
  if (!ValidateBoundary(execution_context, options->rangeStart(),
                        exception_state, 0, /*allow_auto=*/false) ||
      !ValidateBoundary(execution_context, options->rangeEnd(), exception_state,
                        100, /*allow_auto=*/false) ||
      !ValidateBoundary(execution_context, options->exitRangeStart(),
                        exception_state, 0, /*allow_auto=*/true) ||
      !ValidateBoundary(execution_context, options->exitRangeEnd(),
                        exception_state, 100, /*allow_auto=*/true)) {
    return nullptr;
  }
  AnimationTimeline* timeline =
      (options->hasTimeline() ? options->timeline() : nullptr);
  if (!timeline) {
    timeline = &To<LocalDOMWindow>(execution_context)->document()->Timeline();
  }
  return MakeGarbageCollected<TimelineTrigger>(
      timeline, options->rangeStart(), options->rangeEnd(),
      options->exitRangeStart(), options->exitRangeEnd());
}

AnimationTimeline* TimelineTrigger::timeline() {
  return timeline_.Get() ? timeline_.Get()->ExposedTimeline() : nullptr;
}
const TimelineTrigger::RangeBoundary* TimelineTrigger::rangeStart(
    ExecutionContext* execution_context) {
  return range_start_;
}
const TimelineTrigger::RangeBoundary* TimelineTrigger::rangeEnd(
    ExecutionContext* execution_context) {
  return range_end_;
}
const TimelineTrigger::RangeBoundary* TimelineTrigger::exitRangeStart(
    ExecutionContext* execution_context) {
  return exit_range_start_;
}
const TimelineTrigger::RangeBoundary* TimelineTrigger::exitRangeEnd(
    ExecutionContext* execution_context) {
  return exit_range_end_;
}

TimelineTrigger::TriggerBoundaries TimelineTrigger::ComputeTriggerBoundaries(
    double current_offset,
    Element& timeline_source,
    const ScrollTimeline& timeline) {
  const auto timeline_state = timeline.ComputeTimelineState();

  TriggerBoundaries boundaries;

  ExceptionState exception_state(nullptr);
  std::optional<TimelineOffset> trigger_start = TimelineOffset::Create(
      &timeline_source, range_start_, 0, ASSERT_NO_EXCEPTION);
  std::optional<TimelineOffset> trigger_end = TimelineOffset::Create(
      &timeline_source, range_end_, 1, ASSERT_NO_EXCEPTION);
  TimelineOffsetOrAuto exit_start = TimelineOffsetOrAuto::Create(
      &timeline_source, exit_range_start_, 0, ASSERT_NO_EXCEPTION);
  TimelineOffsetOrAuto exit_end = TimelineOffsetOrAuto::Create(
      &timeline_source, exit_range_end_, 1, ASSERT_NO_EXCEPTION);

  // For a ScrollTimeline, these correspond to the min and max scroll offsets of
  // the associated scroll container.
  // For a ViewTimeline, these correspond to the cover 0% and cover 100%
  // respectively.
  const double default_start_position = AdjustForAbsoluteZoom::AdjustScroll(
      timeline_state.scroll_offsets->start, *timeline_source.GetLayoutBox());
  const double default_end_position = AdjustForAbsoluteZoom::AdjustScroll(
      timeline_state.scroll_offsets->end, *timeline_source.GetLayoutBox());

  boundaries.start =
      ComputeTriggerBoundary(trigger_start, default_start_position, timeline,
                             *timeline_state.scroll_offsets, timeline_source);
  boundaries.end =
      ComputeTriggerBoundary(trigger_end, default_end_position, timeline,
                             *timeline_state.scroll_offsets, timeline_source);

  if (exit_start.IsAuto()) {
    // auto behavior: match the trigger range.
    boundaries.exit_start = boundaries.start;
  } else {
    // Note: a nullopt |offset| implies normal, which corresponds to the start
    // of the timeline's range: |timeline_state.scroll_offsets->start|.
    std::optional<TimelineOffset> offset = exit_start.GetTimelineOffset();
    double default_exit_start_offset = timeline_state.scroll_offsets->start;
    boundaries.exit_start =
        ComputeTriggerBoundary(offset, default_exit_start_offset, timeline,
                               *timeline_state.scroll_offsets, timeline_source);
  }

  if (exit_end.IsAuto()) {
    boundaries.exit_end = boundaries.end;
  } else {
    std::optional<TimelineOffset> offset = exit_end.GetTimelineOffset();
    double default_exit_end_offset = timeline_state.scroll_offsets->end;
    boundaries.exit_end =
        ComputeTriggerBoundary(offset, default_exit_end_offset, timeline,
                               *timeline_state.scroll_offsets, timeline_source);
  }

  boundaries.current_offset = current_offset;

  return boundaries;
}

std::optional<TimelineTriggerState> TimelineTrigger::ComputeState() {
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

    Node* timeline_source = timeline->ComputeResolvedSource();
    if (!timeline_source) {
      return std::nullopt;
    }

    current_offset = AdjustForAbsoluteZoom::AdjustScroll(
        *current_offset, *timeline_source->GetLayoutObject());

    if (IsA<LayoutView>(timeline_source->GetLayoutObject())) {
      // If the source is the root document, it isn't an "Element", so we need
      // to work with its scrollingElement
      timeline_source =
          To<Document>(timeline_source)->ScrollingElementNoLayout();
      if (!timeline_source) {
        return std::nullopt;
      }
    }

    boundaries = ComputeTriggerBoundaries(
        *current_offset, *To<Element>(timeline_source), *timeline);
  } else {
    // Only scroll-triggered animations are supported at the moment.
    // Return values that indicate that the a trigger with the document timeline
    // is always tripped.
    // return std::nullopt;
    boundaries = {.start = -std::numeric_limits<double>::infinity(),
                  .end = std::numeric_limits<double>::infinity(),
                  .current_offset = 0};
  }

  bool within_trigger_range =
      WithinRange(boundaries.current_offset, boundaries.start, boundaries.end);
  bool within_exit_range = WithinRange(
      boundaries.current_offset, boundaries.exit_start, boundaries.exit_end);

  State previous_state = state_;
  State new_state = previous_state;

  if (within_trigger_range) {
    new_state = State::kPrimary;
  } else if (!within_exit_range) {
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

void TimelineTrigger::Update() {
  std::optional<State> new_state = ComputeState();
  if (!new_state) {
    return;
  }

  State old_state = state_;
  if (new_state.value() == old_state) {
    return;
  }
  state_ = *new_state;

  switch (state_) {
    case State::kPrimary:
      PerformActivate();
      break;
    case State::kInverse:
      PerformDeactivate();
      break;
    default:
      NOTREACHED();
  }
}

void TimelineTrigger::Trace(Visitor* visitor) const {
  visitor->Trace(timeline_);
  visitor->Trace(range_start_);
  visitor->Trace(range_end_);
  visitor->Trace(exit_range_start_);
  visitor->Trace(exit_range_end_);
  AnimationTrigger::Trace(visitor);
}

void TimelineTrigger::HandlePostTripAdd(Animation* animation,
                                        Behavior activate_behavior,
                                        Behavior deactivate_behavior,
                                        ExceptionState& exception_state) {
  if (state_ == State::kIdle || HasPausedCSSPlayState(animation)) {
    return;
  }

  // If the trigger has already tripped, we want the animation to affected as if
  // it had been added when the tripping event occurred.
  std::optional<Behavior> old_behavior_for_current_state;
  std::optional<Behavior> new_behavior_for_current_state;
  auto old_data_it = BehaviorMap().find(animation);

  switch (state_) {
    case State::kPrimary:
      // We last tripped into "activate"; we might need to act.
      new_behavior_for_current_state = activate_behavior;
      if (old_data_it != BehaviorMap().end()) {
        old_behavior_for_current_state = old_data_it->value.first;
      }
      break;
    case State::kInverse:
      // We last tripped into "deactivate"; we might need to act.
      new_behavior_for_current_state = deactivate_behavior;
      if (old_data_it != BehaviorMap().end()) {
        old_behavior_for_current_state = old_data_it->value.second;
      }
      break;
    default:
      NOTREACHED();
  };

  if (old_behavior_for_current_state != new_behavior_for_current_state) {
    PerformBehavior(*animation, *new_behavior_for_current_state,
                    exception_state);
    animation->UpdateIfNecessary();
  }
}

void TimelineTrigger::WillAddAnimation(Animation* animation,
                                       Behavior activate_behavior,
                                       Behavior deactivate_behavior,
                                       ExceptionState& exception_state) {
  bool was_paused_for_trigger = animation->PausedForTrigger();
  if (animation->CalculateAnimationPlayState() ==
      V8AnimationPlayState::Enum::kIdle) {
    animation->PauseInternal(exception_state);
    if (exception_state.HadException()) {
      return;
    }
    animation->SetPausedForTrigger(true);
    animation->UpdateIfNecessary();
  }

  HandlePostTripAdd(animation, activate_behavior, deactivate_behavior,
                    exception_state);
  if (exception_state.HadException()) {
    animation->SetPausedForTrigger(was_paused_for_trigger);
  }
}

void TimelineTrigger::DidAddAnimation() {
  if (timeline_ && BehaviorMap().size() == 1) {
    timeline_->AddTrigger(this);
  }
}

void TimelineTrigger::DidRemoveAnimation(Animation* animation) {
  if (timeline_ && BehaviorMap().empty()) {
    timeline_->RemoveTrigger(this);
  }
}

bool TimelineTrigger::CanTrigger() const {
  return timeline_ && timeline_->IsActive();
}

bool TimelineTrigger::IsTimelineTrigger() const {
  return true;
}

void TimelineTrigger::CreateCompositorTrigger() {
  if (compositor_trigger_) {
    return;
  }

  if (!timeline_ || !timeline_->GetDocument()) {
    return;
  }

  cc::AnimationTimeline* cc_timeline = timeline_->EnsureCompositorTimeline();
  if (!cc_timeline) {
    return;
  }
  timeline_->GetDocument()->AttachCompositorTimeline(cc_timeline);
  cc::AnimationHost* host = cc_timeline->animation_host();
  CHECK(host);

  scoped_refptr<cc::AnimationTimeline> scopedref_cc_timeline =
      host->GetScopedRefTimelineById(cc_timeline->id());

  scoped_refptr<cc::TimelineTrigger> cc_trigger = cc::TimelineTrigger::Create(
      cc::AnimationIdProvider::NextAnimationTriggerId(), scopedref_cc_timeline);
  host->AddTrigger(cc_trigger);

  compositor_trigger_ =
      static_cast<scoped_refptr<cc::AnimationTrigger>>(cc_trigger);
}

void TimelineTrigger::DestroyCompositorTrigger() {
  if (compositor_trigger_) {
    cc::AnimationHost* host = compositor_trigger_->GetAnimationHost();
    if (host) {
      host->RemoveTrigger(compositor_trigger_.get());
    }
    compositor_trigger_ = nullptr;
  }
}

}  // namespace blink
