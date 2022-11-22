// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_JACK_H_
#define MEDIA_MIDI_USB_MIDI_JACK_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "media/midi/usb_midi_export.h"

namespace midi {

class UsbMidiDevice;

// UsbMidiJack represents an EMBEDDED MIDI jack.
struct USB_MIDI_EXPORT UsbMidiJack {
  // The direction of the endpoint associated with an EMBEDDED MIDI jack.
  // Note that an IN MIDI jack associated with an OUT endpoint has
  // ***DIRECTION_OUT*** direction.
  enum Direction {
    DIRECTION_IN,
    DIRECTION_OUT,
  };
  UsbMidiJack(UsbMidiDevice* device,
              uint8_t jack_id,
              uint8_t cable_number,
              uint8_t endpoint_address)
      : device(device),
        jack_id(jack_id),
        cable_number(cable_number),
        endpoint_address(endpoint_address) {}
  // Not owned
  raw_ptr<UsbMidiDevice> device;
  // The id of this jack unique in the interface.
  uint8_t jack_id;
  // The cable number of this jack in the associated endpoint.
  uint8_t cable_number;
  // The address of the endpoint that this jack is associated with.
  uint8_t endpoint_address;

  Direction direction() const {
    return (endpoint_address & 0x80) ? DIRECTION_IN : DIRECTION_OUT;
  }
  uint8_t endpoint_number() const { return (endpoint_address & 0xf); }
};

}  // namespace midi

#endif  // MEDIA_MIDI_USB_MIDI_JACK_H_
