// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/midi/usb_midi_input_stream.h"

#include <string.h>

#include "base/logging.h"
#include "media/midi/usb_midi_device.h"

namespace midi {

UsbMidiInputStream::JackUniqueKey::JackUniqueKey(UsbMidiDevice* device,
                                                 int endpoint_number,
                                                 int cable_number)
    : device(device),
      endpoint_number(endpoint_number),
      cable_number(cable_number) {}

bool UsbMidiInputStream::JackUniqueKey::operator==(
    const JackUniqueKey& that) const {
  return device == that.device &&
      endpoint_number == that.endpoint_number &&
      cable_number == that.cable_number;
}

bool UsbMidiInputStream::JackUniqueKey::operator<(
    const JackUniqueKey& that) const {
  if (device != that.device)
    return device < that.device;
  if (endpoint_number != that.endpoint_number)
    return endpoint_number < that.endpoint_number;
  return cable_number < that.cable_number;
}

UsbMidiInputStream::UsbMidiInputStream(Delegate* delegate)
    : delegate_(delegate) {}

UsbMidiInputStream::~UsbMidiInputStream() = default;

void UsbMidiInputStream::Add(const UsbMidiJack& jack) {
  JackUniqueKey key(jack.device,
                    jack.endpoint_number(),
                    jack.cable_number);

  jacks_.push_back(jack);
  DCHECK(jack_dictionary_.end() == jack_dictionary_.find(key));
  jack_dictionary_.insert(std::make_pair(key, jack_dictionary_.size()));
}

void UsbMidiInputStream::OnReceivedData(UsbMidiDevice* device,
                                        int endpoint_number,
                                        const uint8_t* data,
                                        size_t size,
                                        base::TimeTicks time) {
  DCHECK_EQ(0u, size % kPacketSize);
  size_t current = 0;
  while (current + kPacketSize <= size) {
    ProcessOnePacket(device, endpoint_number, &data[current], time);
    current += kPacketSize;
  }
}

void UsbMidiInputStream::ProcessOnePacket(UsbMidiDevice* device,
                                          int endpoint_number,
                                          const uint8_t* packet,
                                          base::TimeTicks time) {
  // The first 4 bytes of the packet is accessible here.
  uint8_t code_index = packet[0] & 0x0f;
  uint8_t cable_number = packet[0] >> 4;
  const size_t packet_size_table[16] = {
    0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1,
  };
  size_t packet_size = packet_size_table[code_index];
  if (packet_size == 0) {
    // These CINs are reserved. Ignore them.
    DVLOG(1) << "code index number (" << code_index << ") arrives "
             << "but it is reserved.";
    return;
  }
  std::map<JackUniqueKey, size_t>::const_iterator it =
      jack_dictionary_.find(JackUniqueKey(device,
                                          endpoint_number,
                                          cable_number));
  if (it != jack_dictionary_.end())
    delegate_->OnReceivedData(it->second, &packet[1], packet_size, time);
}

}  // namespace midi
