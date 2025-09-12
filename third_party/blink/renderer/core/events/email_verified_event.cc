// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/email_verified_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_email_verified_event_init.h"

namespace blink {

EmailVerifiedEvent::EmailVerifiedEvent(const AtomicString& type,
                                       const String& presentation_token)
    : Event(type, Bubbles::kYes, Cancelable::kYes),
      presentation_token_(presentation_token) {}

EmailVerifiedEvent::EmailVerifiedEvent(
    const AtomicString& type,
    const EmailVerifiedEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasPresentationToken()) {
    presentation_token_ = initializer->presentationToken();
  }
}

const AtomicString& EmailVerifiedEvent::InterfaceName() const {
  return event_interface_names::kEmailVerifiedEvent;
}

}  // namespace blink
