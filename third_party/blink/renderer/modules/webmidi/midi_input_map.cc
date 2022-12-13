// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webmidi/midi_input_map.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_midi_input.h"

namespace blink {

MIDIInputMap::MIDIInputMap(const HeapVector<Member<MIDIInput>>& entries)
    : MIDIPortMap<MIDIInputMap, MIDIInput>(entries) {}

}  // namespace blink
