/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/events/animation_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_animation_event_init.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

AnimationEvent::AnimationEvent() = default;

AnimationEvent::AnimationEvent(const AtomicString& type,
                               const AnimationEventInit* initializer)
    : Event(type, initializer),
      animation_name_(initializer->animationName()),
      elapsed_time_(
          ANIMATION_TIME_DELTA_FROM_SECONDS(initializer->elapsedTime())),
      pseudo_element_(initializer->pseudoElement()) {}

AnimationEvent::AnimationEvent(const AtomicString& type,
                               const String& animation_name,
                               const AnimationTimeDelta& elapsed_time,
                               const String& pseudo_element)
    : Event(type, Bubbles::kYes, Cancelable::kYes),
      animation_name_(animation_name),
      elapsed_time_(elapsed_time),
      pseudo_element_(pseudo_element) {}

AnimationEvent::~AnimationEvent() = default;

const String& AnimationEvent::animationName() const {
  return animation_name_;
}

double AnimationEvent::elapsedTime() const {
  return elapsed_time_.InSecondsF();
}

const String& AnimationEvent::pseudoElement() const {
  return pseudo_element_;
}

const AtomicString& AnimationEvent::InterfaceName() const {
  return event_interface_names::kAnimationEvent;
}

void AnimationEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
}

}  // namespace blink
