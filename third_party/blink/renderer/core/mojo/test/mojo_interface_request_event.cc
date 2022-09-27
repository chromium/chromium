// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/mojo/test/mojo_interface_request_event.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_interface_request_event_init.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"

namespace blink {

MojoInterfaceRequestEvent::~MojoInterfaceRequestEvent() = default;

void MojoInterfaceRequestEvent::Trace(Visitor* visitor) const {
  Event::Trace(visitor);
  visitor->Trace(handle_);
}

MojoInterfaceRequestEvent::MojoInterfaceRequestEvent(MojoHandle* handle)
    : Event(event_type_names::kInterfacerequest, Bubbles::kNo, Cancelable::kNo),
      handle_(handle) {}

MojoInterfaceRequestEvent::MojoInterfaceRequestEvent(
    const AtomicString& type,
    const MojoInterfaceRequestEventInit* initializer)
    : Event(type, Bubbles::kNo, Cancelable::kNo),
      handle_(initializer->handle()) {}

}  // namespace blink
