// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_trigger.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/deferred_timeline.h"
#include "third_party/blink/renderer/core/animation/document_animations.h"
#include "third_party/blink/renderer/core/animation/document_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline_util.h"
#include "third_party/blink/renderer/core/animation/timeline_range.h"
#include "third_party/blink/renderer/core/animation/view_timeline.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"

namespace blink {

namespace {

void UpdateAnimation(Animation* animation,
                     AnimationTrigger::UpdateType update_type) {
  switch (update_type) {
    case AnimationTrigger::UpdateType::kPlay:
      animation->PlayInternal(Animation::AutoRewind::kEnabled,
                              ASSERT_NO_EXCEPTION);
      break;
    case AnimationTrigger::UpdateType::kPause:
      animation->PauseInternal(ASSERT_NO_EXCEPTION);
      break;
    case AnimationTrigger::UpdateType::kReverse:
      animation->ReverseInternal(ASSERT_NO_EXCEPTION);
      break;
    case AnimationTrigger::UpdateType::kUnpause:
      animation->Unpause();
      break;
    case AnimationTrigger::UpdateType::kReset:
      animation->ResetPlayback();
      break;
    case AnimationTrigger::UpdateType::kNone:
    default:
      NOTREACHED();
  };
}

bool HasPausedCSSPlayState(Animation* animation) {
  if (!animation->IsCSSAnimation()) {
    return false;
  }

  CSSAnimation* css_animation = To<CSSAnimation>(animation);

  if (css_animation->GetIgnoreCSSPlayState()) {
    return false;
  }

  return animation->GetTriggerActionPlayState() == EAnimPlayState::kPaused;
}

}  // namespace

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
      exit_range_end_(exit_range_end) {
  if (timeline_) {
    timeline_->AddAnimationTrigger(this);
  }
  // A default trigger will need to trip immediately.
  Update();
}

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

std::optional<AnimationTrigger::TriggerBoundaries>
AnimationTrigger::CalculateTriggerBoundaries() {
  if (!timeline_ || !timeline_->IsActive()) {
    return std::nullopt;
  }

  if (!timeline_->IsProgressBased()) {
    // Only scroll-triggered animations are supported at the moment.
    // Return values that indicate that the a trigger with the document timeline
    // is always tripped.
    // return std::nullopt;
    return std::make_optional<TriggerBoundaries>(
        {.start = -std::numeric_limits<double>::infinity(),
         .end = std::numeric_limits<double>::infinity(),
         .current_offset = 0});
  }

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
    timeline_source = To<Document>(timeline_source)->ScrollingElementNoLayout();
    if (!timeline_source) {
      return std::nullopt;
    }
  }

  return std::make_optional<>(ComputeTriggerBoundaries(
      *current_offset, *To<Element>(timeline_source), *timeline));
}

AnimationTrigger::TriggerBoundaries AnimationTrigger::ComputeTriggerBoundaries(
    double current_offset,
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

std::optional<AnimationTrigger::State> AnimationTrigger::ComputeState() {
  std::optional<AnimationTrigger::TriggerBoundaries> boundaries =
      CalculateTriggerBoundaries();
  if (!boundaries) {
    return std::nullopt;
  }

  bool within_trigger_range = boundaries->current_offset >= boundaries->start &&
                              boundaries->current_offset <= boundaries->end;
  bool within_exit_range =
      boundaries->current_offset >= boundaries->exit_start &&
      boundaries->current_offset <= boundaries->exit_end;

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

void AnimationTrigger::Update() {
  std::optional<State> new_state = ComputeState();
  if (!new_state) {
    return;
  }

  State old_state = state_;
  if (new_state.value() == old_state) {
    return;
  }

  UpdateInternal(old_state, *new_state);

  state_ = *new_state;
}

void AnimationTrigger::UpdateInternal(State old_state, State new_state) {
  UpdateType update_type = UpdateType::kNone;

  switch (type_.AsEnum()) {
    case Type::Enum::kOnce:
      if (new_state == State::kPrimary) {
        update_type = UpdateType::kUnpause;
      }
      break;
    case Type::Enum::kRepeat:
      if (new_state == State::kPrimary) {
        update_type = UpdateType::kPlay;
      } else {
        update_type = UpdateType::kReset;
      }
      break;
    case Type::Enum::kAlternate:
      if (old_state == State::kIdle) {
        update_type = UpdateType::kPlay;
      } else {
        update_type = UpdateType::kReverse;
      }
      break;
    case Type::Enum::kState:
      if (new_state == State::kPrimary) {
        update_type = UpdateType::kUnpause;
      } else {
        update_type = UpdateType::kPause;
      }
      break;
    default:
      NOTREACHED();
  };

  if (update_type != UpdateType::kNone) {
    UpdateAnimations(update_type);
  }
}

void AnimationTrigger::HandlePostTripAdd(Animation* animation,
                                         ExceptionState& exception_state) {
  DCHECK_NE(state_, State::kIdle);

  if (HasPausedCSSPlayState(animation)) {
    return;
  }

  if (state_ == State::kPrimary) {
    animation->PlayInternal(Animation::AutoRewind::kEnabled, exception_state);
    return;
  }

  switch (type_.AsEnum()) {
    case Type::Enum::kOnce:
      animation->PlayInternal(Animation::AutoRewind::kEnabled, exception_state);
      break;
    case Type::Enum::kRepeat:
      animation->ResetPlayback();
      animation->SetPausedForTrigger(true);
      break;
    case Type::Enum::kAlternate:
      animation->ReverseInternal(exception_state);
      break;
    case Type::Enum::kState:
      animation->PauseInternal(exception_state);
      animation->SetPausedForTrigger(true);
  };
}

void AnimationTrigger::addAnimation(Animation* animation,
                                    ExceptionState& exception_state) {
  if (animations_.Contains(animation)) {
    return;
  }

  animation->PauseInternal(exception_state);
  if (exception_state.HadException()) {
    return;
  }

  if (state_ == State::kIdle) {
    animation->SetPausedForTrigger(true);
  } else {
    HandlePostTripAdd(animation, exception_state);
    if (exception_state.HadException()) {
      return;
    }
  }

  animations_.insert(animation);
  animation->AddTrigger(this);
}

void AnimationTrigger::removeAnimation(Animation* animation) {
  animations_.erase(animation);
  animation->RemoveTrigger(this);
}

void AnimationTrigger::UpdateAnimations(
    AnimationTrigger::UpdateType update_type) {
  DCHECK_NE(update_type, UpdateType::kNone);

  for (Animation* animation : animations_) {
    if (HasPausedCSSPlayState(animation)) {
      continue;
    }
    UpdateAnimation(animation, update_type);
  }
}

}  // namespace blink
