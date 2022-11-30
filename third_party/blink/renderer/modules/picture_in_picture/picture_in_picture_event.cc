// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_event.h"

namespace blink {

PictureInPictureEvent* PictureInPictureEvent::Create(
    const AtomicString& type,
    PictureInPictureWindow* picture_in_picture_window) {
  return MakeGarbageCollected<PictureInPictureEvent>(type,
                                                     picture_in_picture_window);
}

PictureInPictureEvent* PictureInPictureEvent::Create(
    const AtomicString& type,
    const PictureInPictureEventInit* initializer) {
  return MakeGarbageCollected<PictureInPictureEvent>(type, initializer);
}

PictureInPictureWindow* PictureInPictureEvent::pictureInPictureWindow() const {
  return picture_in_picture_window_.Get();
}

PictureInPictureEvent::PictureInPictureEvent(
    AtomicString const& type,
    PictureInPictureWindow* picture_in_picture_window)
    : Event(type, Bubbles::kYes, Cancelable::kNo),
      picture_in_picture_window_(picture_in_picture_window) {}

PictureInPictureEvent::PictureInPictureEvent(
    AtomicString const& type,
    const PictureInPictureEventInit* initializer)
    : Event(type, initializer),
      picture_in_picture_window_(initializer->pictureInPictureWindow()) {}

void PictureInPictureEvent::Trace(Visitor* visitor) const {
  visitor->Trace(picture_in_picture_window_);
  Event::Trace(visitor);
}

}  // namespace blink
