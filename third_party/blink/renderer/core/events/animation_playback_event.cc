// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/animation_playback_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_animation_playback_event_init.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_values.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

AnimationPlaybackEvent::AnimationPlaybackEvent(const AtomicString& type,
                                               V8CSSNumberish* current_time,
                                               V8CSSNumberish* timeline_time)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      current_time_(current_time),
      timeline_time_(timeline_time) {}

AnimationPlaybackEvent::AnimationPlaybackEvent(
    const AtomicString& type,
    const AnimationPlaybackEventInit* initializer)
    : Event(type, initializer),
      current_time_(initializer->currentTime()),
      timeline_time_(initializer->timelineTime()) {}

AnimationPlaybackEvent::~AnimationPlaybackEvent() = default;

const AtomicString& AnimationPlaybackEvent::InterfaceName() const {
  return event_interface_names::kAnimationPlaybackEvent;
}

void AnimationPlaybackEvent::Trace(Visitor* visitor) const {
  TraceIfNeeded<Member<V8CSSNumberish>>::Trace(visitor, current_time_);
  TraceIfNeeded<Member<V8CSSNumberish>>::Trace(visitor, timeline_time_);
  Event::Trace(visitor);
}

}  // namespace blink
