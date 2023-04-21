// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_animation.h"
#include "third_party/blink/renderer/core/animation/animation.h"
#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/animation/keyframe_effect.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

CSSAnimation::CSSAnimation(ExecutionContext* execution_context,
                           AnimationTimeline* timeline,
                           AnimationEffect* content,
                           wtf_size_t animation_index,
                           const String& animation_name)
    : Animation(execution_context, timeline, content),
      animation_index_(animation_index),
      animation_name_(animation_name) {
  // The owning_element does not always equal to the target element of an
  // animation. The following spec gives an example:
  // https://drafts.csswg.org/css-animations-2/#owning-element-section
  owning_element_ = To<KeyframeEffect>(effect())->EffectTarget();
}

String CSSAnimation::playState() const {
  FlushStyles();
  return Animation::playState();
}

bool CSSAnimation::pending() const {
  FlushStyles();
  return Animation::pending();
}

void CSSAnimation::pause(ExceptionState& exception_state) {
  Animation::pause(exception_state);
  if (exception_state.HadException())
    return;
  ignore_css_play_state_ = true;
}

void CSSAnimation::play(ExceptionState& exception_state) {
  Animation::play(exception_state);
  if (exception_state.HadException())
    return;
  ignore_css_play_state_ = true;
}

void CSSAnimation::reverse(ExceptionState& exception_state) {
  PlayStateTransitionScope scope(*this);
  Animation::reverse(exception_state);
}

void CSSAnimation::setTimeline(AnimationTimeline* timeline) {
  Animation::setTimeline(timeline);
  ignore_css_timeline_ = true;
}

void CSSAnimation::setRangeStart(const RangeBoundary* range_start,
                                 ExceptionState& exception_state) {
  Animation::setRangeStart(range_start, exception_state);
  ignore_css_range_start_ = true;
}

void CSSAnimation::setRangeEnd(const RangeBoundary* range_end,
                               ExceptionState& exception_state) {
  Animation::setRangeEnd(range_end, exception_state);
  ignore_css_range_end_ = true;
}

void CSSAnimation::SetRange(const absl::optional<TimelineOffset>& range_start,
                            const absl::optional<TimelineOffset>& range_end) {
  if (GetIgnoreCSSRangeStart() && GetIgnoreCSSRangeEnd()) {
    return;
  }

  const absl::optional<TimelineOffset>& adjusted_range_start =
      GetIgnoreCSSRangeStart() ? GetRangeStartInternal() : range_start;
  const absl::optional<TimelineOffset>& adjusted_range_end =
      GetIgnoreCSSRangeEnd() ? GetRangeEndInternal() : range_end;

  Animation::SetRange(adjusted_range_start, adjusted_range_end);
}

void CSSAnimation::setStartTime(const V8CSSNumberish* start_time,
                                ExceptionState& exception_state) {
  PlayStateTransitionScope scope(*this);
  Animation::setStartTime(start_time, exception_state);
}

AnimationEffect::EventDelegate* CSSAnimation::CreateEventDelegate(
    Element* target,
    const AnimationEffect::EventDelegate* old_event_delegate) {
  return CSSAnimations::CreateEventDelegate(target, animation_name_,
                                            old_event_delegate);
}

void CSSAnimation::FlushStyles() const {
  // TODO(1043778): Flush is likely not required once the CSSAnimation is
  // disassociated from its owning element.
  if (GetDocument()) {
    GetDocument()->UpdateStyleAndLayoutTree();
  }

  // Force resolution of source and offsets.
  if (auto* scroll_timeline = DynamicTo<ScrollTimeline>(timeline())) {
    // A full layout update is required to ensure correctness.
    // UpdateStyleAndLayoutTree does not perform a full update of the layout.
    // The size of the scroll-container and subject size affect timeline-offsets
    // in keyframes. Any change to the scroll-container size or insets also
    // affects the intrinsic iteration duration, and should be reflected in a
    // call to getComputedTiming for the animation's effect. Though currentTime
    // for the timeline is fixed until the next frame, calls to
    // getCurrentTime(range) should likely reflect the affect of any layout
    // changes.
    GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
    if (effect()) {
      // Any change to the scroll bounds or target size can affect the
      // intrinsic iteration duration.
      effect()->InvalidateNormalizedTiming();
    }
    scroll_timeline->FlushStyleUpdate();
  }
}

CSSAnimation::PlayStateTransitionScope::PlayStateTransitionScope(
    CSSAnimation& animation)
    : animation_(animation) {
  was_paused_ = animation_.Paused();
}

CSSAnimation::PlayStateTransitionScope::~PlayStateTransitionScope() {
  bool is_paused = animation_.Paused();
  if (was_paused_ != is_paused)
    animation_.ignore_css_play_state_ = true;
}

}  // namespace blink
