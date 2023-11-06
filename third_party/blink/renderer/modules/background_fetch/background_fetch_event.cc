// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_background_fetch_event_init.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/event_interface_modules_names.h"

namespace blink {

BackgroundFetchEvent::BackgroundFetchEvent(
    const AtomicString& type,
    const BackgroundFetchEventInit* initializer,
    WaitUntilObserver* observer)
    : ExtendableEvent(type, initializer, observer),
      registration_(initializer->registration()) {}

BackgroundFetchEvent::~BackgroundFetchEvent() = default;

BackgroundFetchRegistration* BackgroundFetchEvent::registration() const {
  return registration_.Get();
}

const AtomicString& BackgroundFetchEvent::InterfaceName() const {
  return event_interface_names::kBackgroundFetchEvent;
}

void BackgroundFetchEvent::Trace(Visitor* visitor) const {
  visitor->Trace(registration_);
  ExtendableEvent::Trace(visitor);
}

}  // namespace blink
