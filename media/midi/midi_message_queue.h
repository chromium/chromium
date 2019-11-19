// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_MESSAGE_QUEUE_H_
#define MEDIA_MIDI_MIDI_MESSAGE_QUEUE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "media/midi/midi_export.h"

namespace midi {

// A simple message splitter for possibly unsafe/corrupted MIDI data stream.
// This class allows you to:
// - maintain fragmented MIDI message.
// - skip any invalid data sequence.
// - reorder MIDI messages so that "System Real Time Message", which can be
//   inserted at any point of the byte stream, is placed at the boundary of
//   complete MIDI messages.
// - (Optional) reconstruct complete MIDI messages from data stream where
//   MIDI status byte is abbreviated (a.k.a. "running status").
//
// Example (pseudo message loop):
//   MidiMessageQueue queue(true);  // true to support "running status"
//   while (true) {
//     if (is_incoming_midi_data_available()) {
//       std::vector<uint8_t> incoming_data;
//       read_incoming_midi_data(&incoming_data)
//       queue.Add(incoming_data);
//     }
//     while (true) {
//       std::vector<uint8_t> next_message;
//       queue.Get(&next_message);
//       if (!next_message.empty())
//         dispatch(next_message);
//     }
//   }
class MIDI_EXPORT MidiMessageQueue {
 public:
  // Initializes the queue. Set true to |allow_running_status| to enable
  // "MIDI running status" reconstruction.
  explicit MidiMessageQueue(bool allow_running_status);
  ~MidiMessageQueue();

  // Enqueues |data| to the internal buffer.
  void Add(const std::vector<uint8_t>& data);
  void Add(const uint8_t* data, size_t length);

  // Fills the next complete MIDI message into |message|. If |message| is
  // not empty, the data sequence falls into one of the following types of
  // MIDI message.
  // - Single "Channel Voice Message"    (w/o "System Real Time Messages")
  // - Single "Channel Mode Message"     (w/o "System Real Time Messages")
  // - Single "System Exclusive Message" (w/o "System Real Time Messages")
  // - Single "System Common Message"    (w/o "System Real Time Messages")
  // - Single "System Real Time message"
  // |message| is empty if there is no complete MIDI message any more.
  void Get(std::vector<uint8_t>* message);

 private:
  base::circular_deque<uint8_t> queue_;
  std::vector<uint8_t> next_message_;
  const bool allow_running_status_;
  DISALLOW_COPY_AND_ASSIGN(MidiMessageQueue);
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_MESSAGE_QUEUE_H_
