// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/event_util.h"

#include "third_party/blink/renderer/core/event_type_names.h"

namespace blink {

namespace event_util {

bool IsPointerEventType(const AtomicString& event_type) {
  return event_type == EventTypeNames::gotpointercapture ||
         event_type == EventTypeNames::lostpointercapture ||
         event_type == EventTypeNames::pointercancel ||
         event_type == EventTypeNames::pointerdown ||
         event_type == EventTypeNames::pointerenter ||
         event_type == EventTypeNames::pointerleave ||
         event_type == EventTypeNames::pointermove ||
         event_type == EventTypeNames::pointerout ||
         event_type == EventTypeNames::pointerover ||
         event_type == EventTypeNames::pointerup;
}

bool IsDOMMutationEventType(const AtomicString& event_type) {
  return event_type == EventTypeNames::DOMCharacterDataModified ||
         event_type == EventTypeNames::DOMNodeInserted ||
         event_type == EventTypeNames::DOMNodeInsertedIntoDocument ||
         event_type == EventTypeNames::DOMNodeRemoved ||
         event_type == EventTypeNames::DOMNodeRemovedFromDocument ||
         event_type == EventTypeNames::DOMSubtreeModified;
}

}  // namespace event_util

}  // namespace blink
