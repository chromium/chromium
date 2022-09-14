// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_DESCRIPTOR_PARSER_H_
#define MEDIA_MIDI_USB_MIDI_DESCRIPTOR_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "media/midi/usb_midi_export.h"
#include "media/midi/usb_midi_jack.h"

namespace midi {

class UsbMidiDevice;

// UsbMidiDescriptorParser parses USB descriptors and
// generates input / output lists of MIDIPortInfo.
// This is not a generic USB descriptor parser: this parser is designed
// for collecting USB-MIDI jacks information from the descriptor.
class USB_MIDI_EXPORT UsbMidiDescriptorParser {
 public:
  struct DeviceInfo {
    DeviceInfo()
        : vendor_id(0),
          product_id(0),
          bcd_device_version(0),
          manufacturer_index(0),
          product_index(0) {}
    uint16_t vendor_id;
    uint16_t product_id;
    // The higher one byte represents the "major" number and the lower one byte
    // represents the "minor" number.
    uint16_t bcd_device_version;
    uint8_t manufacturer_index;
    uint8_t product_index;

    static std::string BcdVersionToString(uint16_t);
  };

  UsbMidiDescriptorParser();

  UsbMidiDescriptorParser(const UsbMidiDescriptorParser&) = delete;
  UsbMidiDescriptorParser& operator=(const UsbMidiDescriptorParser&) = delete;

  ~UsbMidiDescriptorParser();

  // Returns true if the operation succeeds.
  // When an incorrect input is given, this method may return true but
  // never crashes.
  bool Parse(UsbMidiDevice* device,
             const uint8_t* data,
             size_t size,
             std::vector<UsbMidiJack>* jacks);

  bool ParseDeviceInfo(const uint8_t* data, size_t size, DeviceInfo* info);

 private:
  bool ParseInternal(UsbMidiDevice* device,
                     const uint8_t* data,
                     size_t size,
                     std::vector<UsbMidiJack>* jacks);
  bool ParseDevice(const uint8_t* data, size_t size, DeviceInfo* info);
  bool ParseInterface(const uint8_t* data, size_t size);
  bool ParseCSInterface(UsbMidiDevice* device,
                        const uint8_t* data,
                        size_t size);
  bool ParseEndpoint(const uint8_t* data, size_t size);
  bool ParseCSEndpoint(const uint8_t* data,
                       size_t size,
                       std::vector<UsbMidiJack>* jacks);
  void Clear();

  bool is_parsing_usb_midi_interface_;
  uint8_t current_endpoint_address_;
  uint8_t current_cable_number_;

  std::vector<UsbMidiJack> incomplete_jacks_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_USB_MIDI_DESCRIPTOR_PARSER_H_
