// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "media/midi/usb_midi_descriptor_parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  midi::UsbMidiDescriptorParser parser;
  std::vector<midi::UsbMidiJack> jacks;
  parser.Parse(nullptr, data, size, &jacks);

  return 0;
}
