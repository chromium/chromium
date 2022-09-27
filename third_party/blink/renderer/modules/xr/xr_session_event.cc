// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_session_event.h"

namespace blink {

XRSessionEvent::XRSessionEvent() = default;

XRSessionEvent::XRSessionEvent(const AtomicString& type, XRSession* session)
    : Event(type, Bubbles::kNo, Cancelable::kYes), session_(session) {}

XRSessionEvent::XRSessionEvent(const AtomicString& type,
                               XRSession* session,
                               Event::Bubbles bubbles,
                               Event::Cancelable cancelable,
                               Event::ComposedMode composed)
    : Event(type, bubbles, cancelable, composed), session_(session) {}

XRSessionEvent::XRSessionEvent(const AtomicString& type,
                               const XRSessionEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasSession())
    session_ = initializer->session();
}

XRSessionEvent::~XRSessionEvent() = default;

const AtomicString& XRSessionEvent::InterfaceName() const {
  return event_interface_names::kXRSessionEvent;
}

void XRSessionEvent::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  Event::Trace(visitor);
}

}  // namespace blink
