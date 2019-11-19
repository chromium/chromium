/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/events/focus_event.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatcher.h"
#include "third_party/blink/renderer/core/dom/events/event_path.h"
#include "third_party/blink/renderer/core/event_interface_names.h"

namespace blink {

const AtomicString& FocusEvent::InterfaceName() const {
  return event_interface_names::kFocusEvent;
}

bool FocusEvent::IsFocusEvent() const {
  return true;
}

FocusEvent::FocusEvent() = default;

FocusEvent::FocusEvent(const AtomicString& type,
                       Bubbles bubbles,
                       AbstractView* view,
                       int detail,
                       EventTarget* related_target,
                       InputDeviceCapabilities* source_capabilities)
    : UIEvent(type,
              bubbles,
              Cancelable::kNo,
              ComposedMode::kComposed,
              base::TimeTicks::Now(),
              view,
              detail,
              source_capabilities),
      related_target_(related_target) {}

FocusEvent::FocusEvent(const AtomicString& type,
                       const FocusEventInit* initializer)
    : UIEvent(type, initializer) {
  if (initializer->hasRelatedTarget())
    related_target_ = initializer->relatedTarget();
}

void FocusEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(related_target_);
  UIEvent::Trace(visitor);
}

DispatchEventResult FocusEvent::DispatchEvent(EventDispatcher& dispatcher) {
  GetEventPath().AdjustForRelatedTarget(dispatcher.GetNode(), relatedTarget());
  return dispatcher.Dispatch();
}

}  // namespace blink
