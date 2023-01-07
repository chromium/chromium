// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_index_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_extendable_event_init.h"

namespace blink {

ContentIndexEvent::ContentIndexEvent(const AtomicString& type,
                                     ContentIndexEventInit* init,
                                     WaitUntilObserver* observer)
    : ExtendableEvent(type, init, observer), id_(init->id()) {}

ContentIndexEvent::~ContentIndexEvent() = default;

const String& ContentIndexEvent::id() const {
  return id_;
}

const AtomicString& ContentIndexEvent::InterfaceName() const {
  return event_interface_names::kContentIndexEvent;
}

}  // namespace blink
