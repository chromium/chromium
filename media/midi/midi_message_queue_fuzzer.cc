// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "media/midi/midi_message_queue.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto queue_running = std::make_unique<midi::MidiMessageQueue>(true);
  auto queue_normal = std::make_unique<midi::MidiMessageQueue>(false);

  queue_running->Add(data, size);
  queue_normal->Add(data, size);

  std::vector<uint8_t> message;
  while (true) {
    queue_running->Get(&message);
    if (message.empty())
      break;
  }

  while (true) {
    queue_normal->Get(&message);
    if (message.empty())
      break;
  }

  return 0;
}
