// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/install_event.h"

#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

InstallEvent* InstallEvent::Create(const AtomicString& type,
                                   const ExtendableEventInit* event_init) {
  return MakeGarbageCollected<InstallEvent>(type, event_init);
}

InstallEvent* InstallEvent::Create(const AtomicString& type,
                                   const ExtendableEventInit* event_init,
                                   int event_id,
                                   WaitUntilObserver* observer) {
  return MakeGarbageCollected<InstallEvent>(type, event_init, event_id,
                                            observer);
}

InstallEvent::~InstallEvent() = default;

const AtomicString& InstallEvent::InterfaceName() const {
  return event_interface_names::kInstallEvent;
}

InstallEvent::InstallEvent(const AtomicString& type,
                           const ExtendableEventInit* initializer)
    : ExtendableEvent(type, initializer), event_id_(0) {}

InstallEvent::InstallEvent(const AtomicString& type,
                           const ExtendableEventInit* initializer,
                           int event_id,
                           WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer), event_id_(event_id) {}

}  // namespace blink
