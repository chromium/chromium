/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/webmidi/midi_input.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/webmidi/midi_access.h"
#include "third_party/blink/renderer/modules/webmidi/midi_message_event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

using midi::mojom::PortState;

MIDIInput::MIDIInput(MIDIAccess* access,
                     const String& id,
                     const String& manufacturer,
                     const String& name,
                     const String& version,
                     PortState state)
    : MIDIPort(access,
               id,
               manufacturer,
               name,
               MIDIPortType::kInput,
               version,
               state) {}

EventListener* MIDIInput::onmidimessage() {
  return GetAttributeEventListener(event_type_names::kMidimessage);
}

void MIDIInput::setOnmidimessage(EventListener* listener) {
  // Implicit open. It does nothing if the port is already opened.
  // See http://www.w3.org/TR/webmidi/#widl-MIDIPort-open-Promise-MIDIPort
  open();

  SetAttributeEventListener(event_type_names::kMidimessage, listener);
}

void MIDIInput::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  MIDIPort::AddedEventListener(event_type, registered_listener);
  if (event_type == event_type_names::kMidimessage) {
    // Implicit open. See setOnmidimessage().
    open();
  }
}

void MIDIInput::DidReceiveMIDIData(unsigned port_index,
                                   const unsigned char* data,
                                   size_t length,
                                   base::TimeTicks time_stamp) {
  DCHECK(IsMainThread());

  if (!length)
    return;

  if (GetConnection() != MIDIPortConnectionState::kOpen)
    return;

  // Drop sysex message here when the client does not request it. Note that this
  // is not a security check but an automatic filtering for clients that do not
  // want sysex message. Also note that sysex message will never be sent unless
  // the current process has an explicit permission to handle sysex message.
  if (data[0] == 0xf0 && !midiAccess()->sysexEnabled())
    return;
  DOMUint8Array* array = DOMUint8Array::Create(
      UNSAFE_TODO(base::span(data, base::checked_cast<unsigned>(length))));

  DispatchEvent(*MakeGarbageCollected<MIDIMessageEvent>(time_stamp, array));

  UseCounter::Count(GetExecutionContext(), WebFeature::kMIDIMessageEvent);
}

void MIDIInput::Trace(Visitor* visitor) const {
  MIDIPort::Trace(visitor);
}

}  // namespace blink
