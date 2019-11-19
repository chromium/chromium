// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_input_sources_change_event.h"

namespace blink {

XRInputSourcesChangeEvent::XRInputSourcesChangeEvent(
    const AtomicString& type,
    XRSession* session,
    const HeapVector<Member<XRInputSource>>& added,
    const HeapVector<Member<XRInputSource>>& removed)
    : Event(type, Bubbles::kYes, Cancelable::kNo),
      session_(session),
      added_(added),
      removed_(removed) {}

XRInputSourcesChangeEvent::XRInputSourcesChangeEvent(
    const AtomicString& type,
    const XRInputSourcesChangeEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasSession())
    session_ = initializer->session();
  if (initializer->hasAdded())
    added_ = initializer->added();
  if (initializer->hasRemoved())
    removed_ = initializer->removed();
}

XRInputSourcesChangeEvent::~XRInputSourcesChangeEvent() = default;

const AtomicString& XRInputSourcesChangeEvent::InterfaceName() const {
  return event_interface_names::kXRInputSourcesChangeEvent;
}

void XRInputSourcesChangeEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(session_);
  visitor->Trace(added_);
  visitor->Trace(removed_);
  Event::Trace(visitor);
}

}  // namespace blink
