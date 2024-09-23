// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/midi/midi_message_queue.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "media/midi/message_util.h"

namespace midi {

MidiMessageQueue::MidiMessageQueue(bool allow_running_status)
    : allow_running_status_(allow_running_status) {}

MidiMessageQueue::~MidiMessageQueue() = default;

void MidiMessageQueue::Add(const std::vector<uint8_t>& data) {
  queue_.insert(queue_.end(), data.begin(), data.end());
}

void MidiMessageQueue::Add(const uint8_t* data, size_t length) {
  queue_.insert(queue_.end(), data, data + length);
}

void MidiMessageQueue::Get(std::vector<uint8_t>* message) {
  message->clear();

  while (true) {
    // Check if |next_message_| is already a complete MIDI message or not.
    if (!next_message_.empty()) {
      const uint8_t status_byte = next_message_.front();
      const size_t target_len = GetMessageLength(status_byte);
      if (target_len == 0) {
        DCHECK_EQ(kSysExByte, status_byte);
        if (next_message_.back() == kEndOfSysExByte) {
          // OK, this is a complete SysEx message.
          std::swap(*message, next_message_);
          DCHECK(next_message_.empty());
          return;
        }
      } else if (next_message_.size() == target_len) {
        // OK, this is a complete non-SysEx message.
        std::swap(*message, next_message_);
        DCHECK(next_message_.empty());
        if (allow_running_status_ && !IsSystemMessage(status_byte)) {
          // Speculatively keep the status byte in case of running status.
          // If this assumption is not true, |next_message_| will be cleared
          // anyway.  Note that system common messages should reset the
          // running status.
          next_message_.push_back(status_byte);
        }
        return;
      } else if (next_message_.size() > target_len) {
        NOTREACHED_IN_MIGRATION();
      }
    }

    if (queue_.empty())
      return;

    // "System Real Time Messages" is a special MIDI message, which can appear
    // at an arbitrary byte position of MIDI stream. Here we reorder
    // "System Real Time Messages" prior to |next_message_| so that each message
    // can be clearly separated as a complete MIDI message.
    const uint8_t next = queue_.front();
    if (IsSystemRealTimeMessage(next)) {
      message->push_back(next);
      queue_.pop_front();
      return;
    }

    if (next_message_.empty()) {
      const size_t target_len = GetMessageLength(next);
      // If |target_len| is zero, it means either |next| is not a valid status
      // byte or |next| is a valid status byte but the message length is
      // unpredictable.  For the latter case, only SysEx can be accepted.
      if (target_len > 0 || next == kSysExByte) {
        next_message_.push_back(next);
      }
      // Consume |next| always, since if |next| isn't status byte, which means
      // that |next| is just corrupted data, or a data byte followed by
      // reserved message, which we are unable to understand and deal with
      // anyway.
      queue_.pop_front();
      continue;
    }

    const uint8_t status_byte = next_message_.front();

    // If we receive a new non-data byte before completing the pending message,
    // drop the pending message and respin the loop to re-evaluate |next|.
    // This also clears the running status byte speculatively added above, as
    // well as any broken incomplete messages.
    if (!IsDataByte(next) &&
        !(status_byte == kSysExByte && next == kEndOfSysExByte)) {
      next_message_.clear();
      continue;
    }

    // OK to consume this byte.
    next_message_.push_back(next);
    queue_.pop_front();
  }
}

}  // namespace midi
