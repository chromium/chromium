// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_PRESENCE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_PRESENCE_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"

namespace blink {

class SmartCardReader;

class SmartCardReaderPresenceEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SmartCardReaderPresenceEvent(const AtomicString& type, SmartCardReader*);

  // SmartCardReaderPresenceEvent idl
  SmartCardReader* reader() const { return reader_; }

  // ScriptWrappable overrides
  void Trace(Visitor*) const override;

 private:
  Member<SmartCardReader> reader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_READER_PRESENCE_EVENT_H_
