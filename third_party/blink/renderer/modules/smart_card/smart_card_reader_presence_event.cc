// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_reader_presence_event.h"

#include "third_party/blink/renderer/modules/smart_card/smart_card_reader.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SmartCardReaderPresenceEvent::SmartCardReaderPresenceEvent(
    const AtomicString& type,
    SmartCardReader* reader)
    : Event(type, Bubbles::kNo, Cancelable::kNo), reader_(reader) {}

void SmartCardReaderPresenceEvent::Trace(Visitor* visitor) const {
  visitor->Trace(reader_);
  Event::Trace(visitor);
}

}  // namespace blink
