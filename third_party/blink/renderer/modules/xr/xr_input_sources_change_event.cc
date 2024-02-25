// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_input_sources_change_event.h"

#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"

namespace blink {

XRInputSourcesChangeEvent::XRInputSourcesChangeEvent(
    const AtomicString& type,
    XRSession* session,
    HeapVector<Member<XRInputSource>> added,
    HeapVector<Member<XRInputSource>> removed)
    : Event(type, Bubbles::kYes, Cancelable::kNo),
      session_(session),
      added_(
          MakeGarbageCollected<FrozenArray<XRInputSource>>(std::move(added))),
      removed_(MakeGarbageCollected<FrozenArray<XRInputSource>>(
          std::move(removed))) {}

XRInputSourcesChangeEvent::XRInputSourcesChangeEvent(
    const AtomicString& type,
    const XRInputSourcesChangeEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasSession()) {
    session_ = initializer->session();
  }
  if (initializer->hasAdded()) {
    added_ =
        MakeGarbageCollected<FrozenArray<XRInputSource>>(initializer->added());
  } else {
    added_ = MakeGarbageCollected<FrozenArray<XRInputSource>>();
  }
  if (initializer->hasRemoved()) {
    removed_ = MakeGarbageCollected<FrozenArray<XRInputSource>>(
        initializer->removed());
  } else {
    removed_ = MakeGarbageCollected<FrozenArray<XRInputSource>>();
  }
}

XRInputSourcesChangeEvent::~XRInputSourcesChangeEvent() = default;

const AtomicString& XRInputSourcesChangeEvent::InterfaceName() const {
  return event_interface_names::kXRInputSourcesChangeEvent;
}

void XRInputSourcesChangeEvent::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(added_);
  visitor->Trace(removed_);
  Event::Trace(visitor);
}

}  // namespace blink
