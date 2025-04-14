// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_trigger.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/deferred_timeline.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/animation/timeline_range.h"
#include "third_party/blink/renderer/core/animation/view_timeline.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

using RangeBoundary = AnimationTrigger::RangeBoundary;

bool ValidateBoundary(ExecutionContext* execution_context,
                      const RangeBoundary* boundary,
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
          "AnimationTrigger range must be a name <length-percent> pair");
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
            "CSSNumericValue must be a length or percentage for animation "
            " trigger range.");
        return false;
      }
    }
  }
  return true;
}

AnimationTrigger::AnimationTrigger(AnimationTimeline* timeline,
                                   Type type,
                                   RangeBoundary* range_start,
                                   RangeBoundary* range_end,
                                   RangeBoundary* exit_range_start,
                                   RangeBoundary* exit_range_end)
    : timeline_(timeline),
      type_(type),
      range_start_(range_start),
      range_end_(range_end),
      exit_range_start_(exit_range_start),
      exit_range_end_(exit_range_end) {}

/* static */
AnimationTrigger* AnimationTrigger::Create(ExecutionContext* execution_context,
                                           AnimationTriggerOptions* options,
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
  return MakeGarbageCollected<AnimationTrigger>(
      timeline, options->type(), options->rangeStart(), options->rangeEnd(),
      options->exitRangeStart(), options->exitRangeEnd());
}

const RangeBoundary* AnimationTrigger::rangeStart(
    ExecutionContext* execution_context) {
  return range_start_;
}
const RangeBoundary* AnimationTrigger::rangeEnd(
    ExecutionContext* execution_context) {
  return range_end_;
}
const RangeBoundary* AnimationTrigger::exitRangeStart(
    ExecutionContext* execution_context) {
  return exit_range_start_;
}
const RangeBoundary* AnimationTrigger::exitRangeEnd(
    ExecutionContext* execution_context) {
  return exit_range_end_;
}

double ComputeTriggerBoundary(
    std::optional<TimelineOffset> offset,
    double default_value,
    const ScrollTimeline& timeline,
    const TimelineRange::ScrollOffsets& range_offsets) {
  if (offset) {
    double range = range_offsets.end - range_offsets.start;
    return range_offsets.start +
           (timeline.IsViewTimeline()
                ? range * To<ViewTimeline>(timeline).ToFractionalOffset(*offset)
                : MinimumValueForLength(offset->offset, LayoutUnit(range)));
  }
  return default_value;
}

AnimationTrigger::TriggerBoundaries AnimationTrigger::ComputeTriggerBoundaries(
    Element& timeline_source,
    const ScrollTimeline& timeline) {
  using TriggerBoundaries = AnimationTrigger::TriggerBoundaries;

  const TimelineState timeline_state = timeline.ComputeTimelineState();

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
  const double default_start_position = timeline_state.scroll_offsets->start;
  const double default_end_position = timeline_state.scroll_offsets->end;

  boundaries.start =
      ComputeTriggerBoundary(trigger_start, default_start_position, timeline,
                             *timeline_state.scroll_offsets);
  boundaries.end =
      ComputeTriggerBoundary(trigger_end, default_end_position, timeline,
                             *timeline_state.scroll_offsets);

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
                               *timeline_state.scroll_offsets);
  }

  if (exit_end.IsAuto()) {
    boundaries.exit_end = boundaries.end;
  } else {
    std::optional<TimelineOffset> offset = exit_end.GetTimelineOffset();
    double default_exit_end_offset = timeline_state.scroll_offsets->end;
    boundaries.exit_end =
        ComputeTriggerBoundary(offset, default_exit_end_offset, timeline,
                               *timeline_state.scroll_offsets);
  }

  return boundaries;
}

void AnimationTrigger::ActionAnimation(Animation* animation) {
  CSSAnimation::ScopedResetIgnoreCSSProperties scoped_ignore_reset(
      DynamicTo<CSSAnimation>(animation));
  if (!timeline_ || !timeline_->IsActive() || !timeline_->IsProgressBased()) {
    // Only scroll-triggered animations are supported at the moment.
    return;
  }

  ScrollTimeline* timeline =
      DynamicTo<ScrollTimeline>(timeline_->ExposedTimeline());
  if (!timeline) {
    return;
  }

  std::optional<double> current_offset = timeline->GetCurrentScrollPosition();
  if (!current_offset) {
    return;
  }

  Node* timeline_source = timeline->ComputeResolvedSource();
  if (!timeline_source) {
    return;
  }
  if (IsA<LayoutView>(timeline_source->GetLayoutObject())) {
    // If the source is the root document, it isn't an "Element", so we need
    // to work with its scrollingElement
    timeline_source = To<Document>(timeline_source)->ScrollingElementNoLayout();
    if (!timeline_source) {
      return;
    }
  }

  AnimationTrigger::TriggerBoundaries boundaries =
      ComputeTriggerBoundaries(*To<Element>(timeline_source), *timeline);

  bool within_trigger =
      current_offset >= boundaries.start && current_offset <= boundaries.end;
  bool within_exit = current_offset >= boundaries.exit_start &&
                     current_offset <= boundaries.exit_end;

  if (ActionAnimationInternal(animation, within_trigger, within_exit)) {
    animation->SetPendingTriggerPlayStateUpdate(false);
  } else {
    ProcessPendingPlayStateUpdate(animation);
  }
}

bool AnimationTrigger::ActionAnimationInternal(Animation* animation,
                                               bool within_trigger_range,
                                               bool within_exit_range) {
  std::optional<bool> ignore_play_state;
  if (CSSAnimation* css_animation = DynamicTo<CSSAnimation>(animation)) {
    ignore_play_state = css_animation->GetIgnoreCSSPlayState();
  }
  std::optional<EAnimPlayState> css_play_state =
      animation->GetTriggerActionPlayState();
  // Whether we are to pause whatever action we choose to take.
  bool pause_action = ignore_play_state.has_value()
                          ? !ignore_play_state.value() &&
                                css_play_state == EAnimPlayState::kPaused
                          : false;

  TriggerState trigger_state = animation->GetTriggerState();
  bool did_action = false;
  if (within_trigger_range) {
    if (trigger_state != TriggerState::kPrimary) {
      // If AnimationTrigger.type ceases to be readonly, we'll need to
      // re-evaluate this DCHECK.
      DCHECK(type_ != Type::Enum::kOnce ||
             trigger_state == TriggerState::kIdle);

      if (trigger_state == TriggerState::kIdle) {
        animation->play();
      } else if (type_ == Type::Enum::kState) {
        animation->Unpause();
      } else if (type_ == Type::Enum::kAlternate) {
        animation->reverse();
      } else {
        // kRepeat
        animation->play();
      }

      animation->SetTriggerState(TriggerState::kPrimary);
      if (pause_action) {
        animation->pause();
      }
      did_action = true;
    }
  } else if (type_ != Type::Enum::kOnce && !within_exit_range) {
    if (trigger_state == TriggerState::kPrimary) {
      if (type_ == Type::Enum::kRepeat) {
        // If we cancel the animation, don't pause it.
        animation->cancel();
      } else if (type_ == Type::Enum::kAlternate) {
        animation->reverse();
        if (pause_action) {
          animation->pause();
        }
      } else if (type_ == Type::Enum::kState) {
        animation->pause();
      }

      animation->SetTriggerState(TriggerState::kInverse);
      did_action = true;
    }
  }

  return did_action;
}

void AnimationTrigger::ProcessPendingPlayStateUpdate(Animation* animation) {
  if (animation->PendingTriggerPlayStateUpdate()) {
    CSSAnimation* css_animation = DynamicTo<CSSAnimation>(animation);
    bool ignoring_play_state =
        !css_animation || css_animation->GetIgnoreCSSPlayState();
    TriggerState trigger_state = animation->GetTriggerState();

    // Do not respond to `animation-play-state` changes if any of the following
    // is true:
    // 1. The trigger is idle.
    // 2. The animation is ignoring `animation-play-state`.
    // 3. This is a repeat trigger outside its exit range. Repeat triggers reset
    //    their animations outside the exit range and should not be playing and
    //    pausing their animation(s).
    bool should_toggle =
        !(trigger_state == TriggerState::kIdle || ignoring_play_state ||
          (type_ == Type::Enum::kRepeat &&
           trigger_state == TriggerState::kInverse));

    if (should_toggle) {
      DCHECK(!ignoring_play_state);
      if (animation->GetTriggerActionPlayState() == EAnimPlayState::kPaused) {
        animation->pause();
      } else {
        animation->Unpause();
      }
    }

    animation->SetPendingTriggerPlayStateUpdate(false);
  }
}

}  // namespace blink
