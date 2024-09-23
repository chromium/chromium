// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/elements/media_control_animation_event_listener.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

MediaControlAnimationEventListener::MediaControlAnimationEventListener(
    Observer* observer)
    : observer_(observer) {
  observer_->WatchedAnimationElement().addEventListener(
      event_type_names::kAnimationend, this, false);
  observer_->WatchedAnimationElement().addEventListener(
      event_type_names::kAnimationiteration, this, false);
}

void MediaControlAnimationEventListener::Detach() {
  observer_->WatchedAnimationElement().removeEventListener(
      event_type_names::kAnimationend, this, false);
  observer_->WatchedAnimationElement().removeEventListener(
      event_type_names::kAnimationiteration, this, false);
}

void MediaControlAnimationEventListener::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  EventListener::Trace(visitor);
}

void MediaControlAnimationEventListener::Invoke(ExecutionContext* context,
                                                Event* event) {
  if (event->type() == event_type_names::kAnimationend) {
    observer_->OnAnimationEnd();
    return;
  }
  if (event->type() == event_type_names::kAnimationiteration) {
    observer_->OnAnimationIteration();
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

void MediaControlAnimationEventListener::Observer::Trace(Visitor*) const {}

}  // namespace blink
