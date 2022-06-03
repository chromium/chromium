// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_INPUT_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_INPUT_MAP_H_

#include "third_party/blink/renderer/modules/webmidi/midi_input.h"
#include "third_party/blink/renderer/modules/webmidi/midi_port_map.h"

namespace blink {

class MIDIInputMap : public MIDIPortMap<MIDIInput> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit MIDIInputMap(const HeapVector<Member<MIDIInput>>&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBMIDI_MIDI_INPUT_MAP_H_
