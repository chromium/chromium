// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_input_source_event.h"

namespace blink {

XRInputSourceEvent::XRInputSourceEvent() {}

XRInputSourceEvent::XRInputSourceEvent(const AtomicString& type,
                                       XRFrame* frame,
                                       XRInputSource* input_source)
    : Event(type, Bubbles::kYes, Cancelable::kNo),
      frame_(frame),
      input_source_(input_source) {}

XRInputSourceEvent::XRInputSourceEvent(
    const AtomicString& type,
    const XRInputSourceEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasFrame())
    frame_ = initializer->frame();
  if (initializer->hasInputSource())
    input_source_ = initializer->inputSource();
}

XRInputSourceEvent::~XRInputSourceEvent() {}

const AtomicString& XRInputSourceEvent::InterfaceName() const {
  return event_interface_names::kXRInputSourceEvent;
}

void XRInputSourceEvent::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(input_source_);
  Event::Trace(visitor);
}

}  // namespace blink
