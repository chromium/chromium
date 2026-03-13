// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_message_queue.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  auto queue_running = std::make_unique<midi::MidiMessageQueue>(true);
  auto queue_normal = std::make_unique<midi::MidiMessageQueue>(false);

  queue_running->Add(data);
  queue_normal->Add(data);

  std::vector<uint8_t> message;
  while (true) {
    queue_running->Get(&message);
    if (message.empty()) {
      break;
    }
  }

  while (true) {
    queue_normal->Get(&message);
    if (message.empty()) {
      break;
    }
  }

  return 0;
}
