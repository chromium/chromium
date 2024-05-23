// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/event_util.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

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

MutationEventInfo IsDOMMutationEventType(const AtomicString& event_type) {
  if (event_type == event_type_names::kDOMSubtreeModified) {
    return {.is_mutation_event = true,
            .listener_feature = WebFeature::kDOMSubtreeModifiedEvent,
            .event_fired_feature = WebFeature::kDOMSubtreeModifiedEventFired,
            .listener_type = Document::kDOMSubtreeModifiedListener};
  } else if (event_type == event_type_names::kDOMNodeInserted) {
    return {.is_mutation_event = true,
            .listener_feature = WebFeature::kDOMNodeInsertedEvent,
            .event_fired_feature = WebFeature::kDOMNodeInsertedEventFired,
            .listener_type = Document::kDOMNodeInsertedListener};
  } else if (event_type == event_type_names::kDOMNodeRemoved) {
    return {.is_mutation_event = true,
            .listener_feature = WebFeature::kDOMNodeRemovedEvent,
            .event_fired_feature = WebFeature::kDOMNodeRemovedEventFired,
            .listener_type = Document::kDOMNodeRemovedListener};
  } else if (event_type == event_type_names::kDOMNodeRemovedFromDocument) {
    return {.is_mutation_event = true,
            .listener_feature = WebFeature::kDOMNodeRemovedFromDocumentEvent,
            .event_fired_feature =
                WebFeature::kDOMNodeRemovedFromDocumentEventFired,
            .listener_type = Document::kDOMNodeRemovedFromDocumentListener};
  } else if (event_type == event_type_names::kDOMNodeInsertedIntoDocument) {
    return {.is_mutation_event = true,
            .listener_feature = WebFeature::kDOMNodeInsertedIntoDocumentEvent,
            .event_fired_feature =
                WebFeature::kDOMNodeInsertedIntoDocumentEventFired,
            .listener_type = Document::kDOMNodeInsertedIntoDocumentListener};
  } else if (event_type == event_type_names::kDOMCharacterDataModified) {
    return {
        .is_mutation_event = true,
        .listener_feature = WebFeature::kDOMCharacterDataModifiedEvent,
        .event_fired_feature = WebFeature::kDOMCharacterDataModifiedEventFired,
        .listener_type = Document::kDOMCharacterDataModifiedListener};
  }
  return {.is_mutation_event = false};
}

bool IsSnapEventType(const AtomicString& event_type) {
  return event_type == event_type_names::kScrollsnapchanging ||
         event_type == event_type_names::kScrollsnapchange;
}

}  // namespace event_util

}  // namespace blink
