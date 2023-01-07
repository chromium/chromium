// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webmidi/midi_message_event.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_midi_message_event_init.h"

namespace blink {

MIDIMessageEvent::MIDIMessageEvent(const AtomicString& type,
                                   const MIDIMessageEventInit* initializer)
    : Event(type, initializer) {
  if (initializer->hasData())
    data_ = initializer->data().Get();
}

}  // namespace blink
