// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_picture_in_picture/document_picture_in_picture_event.h"

namespace blink {

DocumentPictureInPictureEvent* DocumentPictureInPictureEvent::Create(
    const AtomicString& type,
    DOMWindow* document_picture_in_picture_window) {
  return MakeGarbageCollected<DocumentPictureInPictureEvent>(
      type, document_picture_in_picture_window);
}

DocumentPictureInPictureEvent* DocumentPictureInPictureEvent::Create(
    const AtomicString& type,
    const DocumentPictureInPictureEventInit* initializer) {
  return MakeGarbageCollected<DocumentPictureInPictureEvent>(type, initializer);
}

DOMWindow* DocumentPictureInPictureEvent::window() const {
  return document_picture_in_picture_window_.Get();
}

DocumentPictureInPictureEvent::DocumentPictureInPictureEvent(
    AtomicString const& type,
    DOMWindow* document_picture_in_picture_window)
    : Event(type, Bubbles::kYes, Cancelable::kNo),
      document_picture_in_picture_window_(document_picture_in_picture_window) {}

DocumentPictureInPictureEvent::DocumentPictureInPictureEvent(
    AtomicString const& type,
    const DocumentPictureInPictureEventInit* initializer)
    : Event(type, initializer),
      document_picture_in_picture_window_(initializer->window()) {}

void DocumentPictureInPictureEvent::Trace(Visitor* visitor) const {
  visitor->Trace(document_picture_in_picture_window_);
  Event::Trace(visitor);
}

}  // namespace blink
