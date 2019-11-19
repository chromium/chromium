// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_ERROR_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_ERROR_EVENT_H_

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/nfc/ndef_error_event_init.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class NDEFErrorEvent : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NDEFErrorEvent* Create(const AtomicString& event_type,
                                const NDEFErrorEventInit* initializer) {
    return MakeGarbageCollected<NDEFErrorEvent>(event_type, initializer);
  }

  NDEFErrorEvent(const AtomicString& event_type, DOMException* error);
  NDEFErrorEvent(const AtomicString& event_type,
                 const NDEFErrorEventInit* initializer);
  ~NDEFErrorEvent() override;

  void Trace(blink::Visitor*) override;

  const AtomicString& InterfaceName() const override;

  DOMException* error() { return error_; }

 private:
  Member<DOMException> error_;
};

DEFINE_TYPE_CASTS(NDEFErrorEvent,
                  Event,
                  event,
                  event->InterfaceName() ==
                      event_interface_names::kNDEFErrorEvent,
                  event.InterfaceName() ==
                      event_interface_names::kNDEFErrorEvent);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_ERROR_EVENT_H_
