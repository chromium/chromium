// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_index/content_index_event.h"

#include "third_party/blink/renderer/modules/service_worker/extendable_event_init.h"

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
