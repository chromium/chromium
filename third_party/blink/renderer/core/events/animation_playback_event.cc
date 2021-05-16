// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/animation_playback_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_animation_playback_event_init.h"
#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

AnimationPlaybackEvent::AnimationPlaybackEvent(
    const AtomicString& type,
    absl::optional<double> current_time,
    absl::optional<double> timeline_time)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      current_time_(current_time),
      timeline_time_(timeline_time) {
  DCHECK(!current_time_ || !std::isnan(current_time_.value()));
  DCHECK(!timeline_time_ || !std::isnan(timeline_time_.value()));
}

AnimationPlaybackEvent::AnimationPlaybackEvent(
    const AtomicString& type,
    const AnimationPlaybackEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasCurrentTimeNonNull()) {
    current_time_ = initializer->currentTimeNonNull();
  }
  if (initializer->hasTimelineTimeNonNull()) {
    timeline_time_ = initializer->timelineTime();
  }
  DCHECK(!current_time_ || !std::isnan(current_time_.value()));
  DCHECK(!timeline_time_ || !std::isnan(timeline_time_.value()));
}

AnimationPlaybackEvent::~AnimationPlaybackEvent() = default;

const AtomicString& AnimationPlaybackEvent::InterfaceName() const {
  return event_interface_names::kAnimationPlaybackEvent;
}

void AnimationPlaybackEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
