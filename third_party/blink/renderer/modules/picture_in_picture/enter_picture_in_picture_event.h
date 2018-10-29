// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_ENTER_PICTURE_IN_PICTURE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_ENTER_PICTURE_IN_PICTURE_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/picture_in_picture/enter_picture_in_picture_event_init.h"
#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_window.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// An EnterPictureInPictureEvent is a subclass of Event with an additional
// attribute that points to the Picture-in-Picture window.
class MODULES_EXPORT EnterPictureInPictureEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static EnterPictureInPictureEvent* Create(const AtomicString&,
                                            PictureInPictureWindow*);
  static EnterPictureInPictureEvent* Create(
      const AtomicString&,
      const EnterPictureInPictureEventInit&);

  PictureInPictureWindow* pictureInPictureWindow() const;

  void Trace(blink::Visitor*) override;

 private:
  EnterPictureInPictureEvent(AtomicString const&, PictureInPictureWindow*);
  EnterPictureInPictureEvent(AtomicString const&,
                             const EnterPictureInPictureEventInit&);

  Member<PictureInPictureWindow> picture_in_picture_window_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_ENTER_PICTURE_IN_PICTURE_EVENT_H_
