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

bool IsDOMMutationEventType(const AtomicString& event_type) {
  WebFeature web_feature;
  Document::ListenerType listener_type;
  return IsDOMMutationEventType(event_type, web_feature, listener_type);
}
bool IsDOMMutationEventType(const AtomicString& event_type,
                            WebFeature& web_feature,
                            Document::ListenerType& listener_type) {
  if (event_type == event_type_names::kDOMSubtreeModified) {
    web_feature = WebFeature::kDOMSubtreeModifiedEvent;
    listener_type = Document::kDOMSubtreeModifiedListener;
    return true;
  } else if (event_type == event_type_names::kDOMNodeInserted) {
    web_feature = WebFeature::kDOMNodeInsertedEvent;
    listener_type = Document::kDOMNodeInsertedListener;
    return true;
  } else if (event_type == event_type_names::kDOMNodeRemoved) {
    web_feature = WebFeature::kDOMNodeRemovedEvent;
    listener_type = Document::kDOMNodeRemovedListener;
    return true;
  } else if (event_type == event_type_names::kDOMNodeRemovedFromDocument) {
    web_feature = WebFeature::kDOMNodeRemovedFromDocumentEvent;
    listener_type = Document::kDOMNodeRemovedFromDocumentListener;
    return true;
  } else if (event_type == event_type_names::kDOMNodeInsertedIntoDocument) {
    web_feature = WebFeature::kDOMNodeInsertedIntoDocumentEvent;
    listener_type = Document::kDOMNodeInsertedIntoDocumentListener;
    return true;
  } else if (event_type == event_type_names::kDOMCharacterDataModified) {
    web_feature = WebFeature::kDOMCharacterDataModifiedEvent;
    listener_type = Document::kDOMCharacterDataModifiedListener;
    return true;
  }
  return false;
}

}  // namespace event_util

}  // namespace blink
