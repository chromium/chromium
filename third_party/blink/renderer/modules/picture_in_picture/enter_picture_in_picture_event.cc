// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/enter_picture_in_picture_event.h"

namespace blink {

EnterPictureInPictureEvent* EnterPictureInPictureEvent::Create(
    const AtomicString& type,
    PictureInPictureWindow* picture_in_picture_window) {
  return new EnterPictureInPictureEvent(type, picture_in_picture_window);
}

EnterPictureInPictureEvent* EnterPictureInPictureEvent::Create(
    const AtomicString& type,
    const EnterPictureInPictureEventInit& initializer) {
  return new EnterPictureInPictureEvent(type, initializer);
}

PictureInPictureWindow* EnterPictureInPictureEvent::pictureInPictureWindow()
    const {
  return picture_in_picture_window_.Get();
}

EnterPictureInPictureEvent::EnterPictureInPictureEvent(
    AtomicString const& type,
    PictureInPictureWindow* picture_in_picture_window)
    : Event(type, Bubbles::kYes, Cancelable::kNo),
      picture_in_picture_window_(picture_in_picture_window) {}

EnterPictureInPictureEvent::EnterPictureInPictureEvent(
    AtomicString const& type,
    const EnterPictureInPictureEventInit& initializer)
    : Event(type, initializer),
      picture_in_picture_window_(initializer.pictureInPictureWindow()) {}

void EnterPictureInPictureEvent::Trace(blink::Visitor* visitor) {
  visitor->Trace(picture_in_picture_window_);
  Event::Trace(visitor);
}

}  // namespace blink
