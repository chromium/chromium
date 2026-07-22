// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Universal MIDI Packet (UMP) is a packet-based MIDI message format defined
// in the MIDI 2.0 specification. It supports both MIDI 1.0 and MIDI 2.0
// protocols, allowing them to be co-located in the same stream. UMPs are
// composed of 32-bit words, ranging from 1 to 4 words depending on the
// message type.
//
// For the official specification, see:
// https://midi.org/specifications
// (Specifically, "Universal MIDI Packet (UMP) and MIDI 2.0 Protocol")

#ifndef MEDIA_MIDI_UMP_MESSAGE_UTIL_H_
#define MEDIA_MIDI_UMP_MESSAGE_UTIL_H_

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_span.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/midi/midi_export.h"

// On Apple platforms, UMP utilities are compiled into the "midi" component
// and need to be exported. On other platforms, they are linked statically
// into tests and fuzzers, so they should not be marked for import/export
// to avoid linker warnings/errors (e.g. "locally defined symbol imported").
#if BUILDFLAG(IS_APPLE)
#define UMP_EXPORT MIDI_EXPORT
#else
#define UMP_EXPORT
#endif

namespace midi {

// Represents a parsed legacy MIDI message.
struct UMP_EXPORT MidiMessage {
  bool is_sysex;

  // Points to the original data buffer to avoid copying.
  base::raw_span<const uint8_t> data;

  // Stores the reconstructed data (e.g. after removing interleaved Real-Time
  // messages). Only populated if the message required reconstruction.
  std::vector<uint8_t> reconstructed_data;

  // Returns the message data. Use this helper instead of accessing `data` or
  // `reconstructed_data` directly.
  base::span<const uint8_t> GetData() const {
    return reconstructed_data.empty()
               ? base::span<const uint8_t>(data)
               : base::span<const uint8_t>(reconstructed_data);
  }
};

// Returns the UMP message type (0-15) from the first word of a UMP.
UMP_EXPORT uint8_t GetUmpMessageType(uint32_t first_word);

// Returns the UMP group (0-15) from the first word of a UMP.
UMP_EXPORT uint8_t GetUmpGroup(uint32_t first_word);

// Returns the length of the UMP in 32-bit words, based on its message type.
// Returns 0 for invalid or unsupported message types.
UMP_EXPORT size_t GetUmpLengthInWords(uint32_t first_word);

// Parses a legacy MIDI 1.0 byte stream into individual MIDI messages.
// Returns a vector of MidiMessage structs representing the parsed messages.
UMP_EXPORT std::vector<MidiMessage> ParseMidiMessages(
    base::span<const uint8_t> data);

// Parses UMPs (Universal MIDI Packets) from a span of 32-bit words and
// dispatches the reconstructed legacy MIDI 1.0 messages to the provided
// callback.
// All UMPs in the span are assumed to have the same timestamp.
// The callback is called synchronously for each message.
UMP_EXPORT void DispatchMidiFromUmpWords(
    base::span<const uint32_t> ump_words,
    base::TimeTicks timestamp,
    base::FunctionRef<void(base::span<const uint8_t> data,
                           base::TimeTicks timestamp)> dispatch_helper);

// Translates legacy MIDI messages into UMP words.
// - `data`: The legacy MIDI bytes to translate.
// - `group`: The UMP group (0-15) to use for the messages.
// - `ump_words`: The destination vector to append the generated UMP words.
UMP_EXPORT void TranslateMidiToUmpWords(base::span<const uint8_t> data,
                                        uint8_t group,
                                        std::vector<uint32_t>& ump_words);

}  // namespace midi

#endif  // MEDIA_MIDI_UMP_MESSAGE_UTIL_H_
