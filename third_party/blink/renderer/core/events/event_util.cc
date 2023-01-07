// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/event_util.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

namespace event_util {

const Vector<AtomicString>& MouseButtonEventTypes() {
  DEFINE_STATIC_LOCAL(
      const Vector<AtomicString>, mouse_button_event_types,
      ({event_type_names::kClick, event_type_names::kMousedown,
        event_type_names::kMouseup, event_type_names::kDOMActivate}));
  return mouse_button_event_types;
}

bool IsMouseButtonEventType(const AtomicString& event_type) {
  return base::Contains(MouseButtonEventTypes(), event_type);
}

bool IsPointerEventType(const AtomicString& event_type) {
  return event_type == event_type_names::kGotpointercapture ||
         event_type == event_type_names::kLostpointercapture ||
         event_type == event_type_names::kPointercancel ||
         event_type == event_type_names::kPointerdown ||
         event_type == event_type_names::kPointerenter ||
         event_type == event_type_names::kPointerleave ||
         event_type == event_type_names::kPointermove ||
         event_type == event_type_names::kPointerout ||
         event_type == event_type_names::kPointerover ||
         event_type == event_type_names::kPointerup;
}

bool IsDOMMutationEventType(const AtomicString& event_type) {
  return event_type == event_type_names::kDOMCharacterDataModified ||
         event_type == event_type_names::kDOMNodeInserted ||
         event_type == event_type_names::kDOMNodeInsertedIntoDocument ||
         event_type == event_type_names::kDOMNodeRemoved ||
         event_type == event_type_names::kDOMNodeRemovedFromDocument ||
         event_type == event_type_names::kDOMSubtreeModified;
}

}  // namespace event_util

}  // namespace blink
