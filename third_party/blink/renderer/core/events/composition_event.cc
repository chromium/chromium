/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/events/composition_event.h"

#include "third_party/blink/renderer/core/event_interface_names.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"

namespace blink {

CompositionEvent::CompositionEvent() = default;

CompositionEvent::CompositionEvent(const AtomicString& type,
                                   AbstractView* view,
                                   const String& data)
    : UIEvent(type,
              Bubbles::kYes,
              Cancelable::kYes,
              ComposedMode::kComposed,
              base::TimeTicks::Now(),
              view,
              0,
              view ? view->GetInputDeviceCapabilities()->FiresTouchEvents(false)
                   : nullptr),
      data_(data) {}

CompositionEvent::CompositionEvent(const AtomicString& type,
                                   const CompositionEventInit* initializer)
    : UIEvent(type, initializer) {
  if (initializer->hasData())
    data_ = initializer->data();
}

CompositionEvent::~CompositionEvent() = default;

void CompositionEvent::initCompositionEvent(const AtomicString& type,
                                            bool bubbles,
                                            bool cancelable,
                                            AbstractView* view,
                                            const String& data) {
  if (IsBeingDispatched())
    return;

  initUIEvent(type, bubbles, cancelable, view, 0);

  data_ = data;
}

const AtomicString& CompositionEvent::InterfaceName() const {
  return event_interface_names::kCompositionEvent;
}

bool CompositionEvent::IsCompositionEvent() const {
  return true;
}

void CompositionEvent::Trace(blink::Visitor* visitor) {
  UIEvent::Trace(visitor);
}

}  // namespace blink
