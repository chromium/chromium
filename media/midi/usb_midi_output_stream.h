// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_USB_MIDI_OUTPUT_STREAM_H_
#define MEDIA_MIDI_USB_MIDI_OUTPUT_STREAM_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "media/midi/usb_midi_export.h"
#include "media/midi/usb_midi_jack.h"

namespace midi {

// UsbMidiOutputStream converts MIDI data to USB-MIDI data.
// See "USB Device Class Definition for MIDI Devices" Release 1.0,
// Section 4 "USB-MIDI Event Packets" for details.
class USB_MIDI_EXPORT UsbMidiOutputStream {
 public:
  explicit UsbMidiOutputStream(const UsbMidiJack& jack);

  UsbMidiOutputStream(const UsbMidiOutputStream&) = delete;
  UsbMidiOutputStream& operator=(const UsbMidiOutputStream&) = delete;

  // Converts |data| to USB-MIDI data and send it to the jack.
  void Send(const std::vector<uint8_t>& data);

  const UsbMidiJack& jack() const { return jack_; }

 private:
  size_t GetSize(const std::vector<uint8_t>& data) const;
  uint8_t Get(const std::vector<uint8_t>& data, size_t index) const;

  bool PushSysExMessage(const std::vector<uint8_t>& data,
                        size_t* current,
                        std::vector<uint8_t>* data_to_send);
  bool PushSysCommonMessage(const std::vector<uint8_t>& data,
                            size_t* current,
                            std::vector<uint8_t>* data_to_send);
  void PushSysRTMessage(const std::vector<uint8_t>& data,
                        size_t* current,
                        std::vector<uint8_t>* data_to_send);
  bool PushChannelMessage(const std::vector<uint8_t>& data,
                          size_t* current,
                          std::vector<uint8_t>* data_to_send);

  static const size_t kPacketContentSize = 3;

  UsbMidiJack jack_;
  size_t pending_size_;
  uint8_t pending_data_[kPacketContentSize];
  bool is_sending_sysex_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_USB_MIDI_OUTPUT_STREAM_H_
