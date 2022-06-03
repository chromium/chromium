// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/usb_midi_descriptor_parser.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace midi {

namespace {

// The constants below are specified in USB spec, USB audio spec
// and USB midi spec.

enum DescriptorType {
  TYPE_DEVICE = 1,
  TYPE_CONFIGURATION = 2,
  TYPE_STRING = 3,
  TYPE_INTERFACE = 4,
  TYPE_ENDPOINT = 5,
  TYPE_DEVICE_QUALIFIER = 6,
  TYPE_OTHER_SPEED_CONFIGURATION = 7,
  TYPE_INTERFACE_POWER = 8,

  TYPE_CS_INTERFACE = 36,
  TYPE_CS_ENDPOINT = 37,
};

enum DescriptorSubType {
  SUBTYPE_MS_DESCRIPTOR_UNDEFINED = 0,
  SUBTYPE_MS_HEADER = 1,
  SUBTYPE_MIDI_IN_JACK = 2,
  SUBTYPE_MIDI_OUT_JACK = 3,
  SUBTYPE_ELEMENT = 4,
};

enum JackType {
  JACK_TYPE_UNDEFINED = 0,
  JACK_TYPE_EMBEDDED = 1,
  JACK_TYPE_EXTERNAL = 2,
};

const uint8_t kAudioInterfaceClass = 1;
const uint8_t kAudioMidiInterfaceSubclass = 3;

class JackMatcher {
 public:
  explicit JackMatcher(uint8_t id) : id_(id) {}

  bool operator() (const UsbMidiJack& jack) const {
    return jack.jack_id == id_;
  }

 private:
  uint8_t id_;
};

bool DecodeBcd(uint8_t byte, int* decoded) {
  // Write decoded decimal value from |byte| into |decoded|. If either nibble in
  // |byte| exceeds decimal 10, returns false.
  const uint8_t k_nibble_ten = 0xa;
  const uint8_t nibble_major = (byte & 0xf0) >> 4;
  const uint8_t nibble_minor = byte & 0x0f;
  if (nibble_major >= k_nibble_ten || nibble_minor >= k_nibble_ten) {
    return false;
  }
  *decoded = nibble_major * 10 + nibble_minor;
  return true;
}

}  // namespace

std::string UsbMidiDescriptorParser::DeviceInfo::BcdVersionToString(
    uint16_t version) {
  const int byte_1 = version >> 8;
  const int byte_2 = version & 0xff;
  int version_major, version_minor;
  if (!DecodeBcd(byte_1, &version_major) ||
      !DecodeBcd(byte_2, &version_minor)) {
    return base::StringPrintf("Invalid BCD $%02x.%02x", byte_1, byte_2);
  }
  return base::StringPrintf("%d.%02d", version_major, version_minor);
}

UsbMidiDescriptorParser::UsbMidiDescriptorParser()
    : is_parsing_usb_midi_interface_(false),
      current_endpoint_address_(0),
      current_cable_number_(0) {}

UsbMidiDescriptorParser::~UsbMidiDescriptorParser() = default;

bool UsbMidiDescriptorParser::Parse(UsbMidiDevice* device,
                                    const uint8_t* data,
                                    size_t size,
                                    std::vector<UsbMidiJack>* jacks) {
  jacks->clear();
  bool result = ParseInternal(device, data, size, jacks);
  if (!result)
    jacks->clear();
  Clear();
  return result;
}

bool UsbMidiDescriptorParser::ParseDeviceInfo(const uint8_t* data,
                                              size_t size,
                                              DeviceInfo* info) {
  *info = DeviceInfo();
  for (const uint8_t* current = data; current < data + size;
       current += current[0]) {
    uint8_t length = current[0];
    if (length < 2) {
      DVLOG(1) << "Descriptor Type is not accessible.";
      return false;
    }
    if (current + length > data + size) {
      DVLOG(1) << "The header size is incorrect.";
      return false;
    }
    DescriptorType descriptor_type = static_cast<DescriptorType>(current[1]);
    if (descriptor_type != TYPE_DEVICE)
      continue;
    // We assume that ParseDevice doesn't modify |*info| if it returns false.
    return ParseDevice(current, length, info);
  }
  // No DEVICE descriptor is found.
  return false;
}

bool UsbMidiDescriptorParser::ParseInternal(UsbMidiDevice* device,
                                            const uint8_t* data,
                                            size_t size,
                                            std::vector<UsbMidiJack>* jacks) {
  for (const uint8_t* current = data; current < data + size;
       current += current[0]) {
    uint8_t length = current[0];
    if (length < 2) {
      DVLOG(1) << "Descriptor Type is not accessible.";
      return false;
    }
    if (current + length > data + size) {
      DVLOG(1) << "The header size is incorrect.";
      return false;
    }
    DescriptorType descriptor_type = static_cast<DescriptorType>(current[1]);
    if (descriptor_type != TYPE_INTERFACE && !is_parsing_usb_midi_interface_)
      continue;

    switch (descriptor_type) {
      case TYPE_INTERFACE:
        if (!ParseInterface(current, length))
          return false;
        break;
      case TYPE_CS_INTERFACE:
        // We are assuming that the corresponding INTERFACE precedes
        // the CS_INTERFACE descriptor, as specified.
        if (!ParseCSInterface(device, current, length))
          return false;
        break;
      case TYPE_ENDPOINT:
        // We are assuming that endpoints are contained in an interface.
        if (!ParseEndpoint(current, length))
          return false;
        break;
      case TYPE_CS_ENDPOINT:
        // We are assuming that the corresponding ENDPOINT precedes
        // the CS_ENDPOINT descriptor, as specified.
        if (!ParseCSEndpoint(current, length, jacks))
          return false;
        break;
      default:
        // Ignore uninteresting types.
        break;
    }
  }
  return true;
}

bool UsbMidiDescriptorParser::ParseDevice(const uint8_t* data,
                                          size_t size,
                                          DeviceInfo* info) {
  if (size < 0x12) {
    DVLOG(1) << "DEVICE header size is incorrect.";
    return false;
  }

  info->vendor_id = data[8] | (data[9] << 8);
  info->product_id = data[0xa] | (data[0xb] << 8);
  info->bcd_device_version = data[0xc] | (data[0xd] << 8);
  info->manufacturer_index = data[0xe];
  info->product_index = data[0xf];
  return true;
}

bool UsbMidiDescriptorParser::ParseInterface(const uint8_t* data, size_t size) {
  if (size != 9) {
    DVLOG(1) << "INTERFACE header size is incorrect.";
    return false;
  }
  incomplete_jacks_.clear();

  uint8_t interface_class = data[5];
  uint8_t interface_subclass = data[6];

  // All descriptors of endpoints contained in this interface
  // precede the next INTERFACE descriptor.
  is_parsing_usb_midi_interface_ =
      interface_class == kAudioInterfaceClass &&
      interface_subclass == kAudioMidiInterfaceSubclass;
  return true;
}

bool UsbMidiDescriptorParser::ParseCSInterface(UsbMidiDevice* device,
                                               const uint8_t* data,
                                               size_t size) {
  // Descriptor Type and Descriptor Subtype should be accessible.
  if (size < 3) {
    DVLOG(1) << "CS_INTERFACE header size is incorrect.";
    return false;
  }

  DescriptorSubType subtype = static_cast<DescriptorSubType>(data[2]);

  if (subtype != SUBTYPE_MIDI_OUT_JACK &&
      subtype != SUBTYPE_MIDI_IN_JACK)
    return true;

  if (size < 6) {
    DVLOG(1) << "CS_INTERFACE (MIDI JACK) header size is incorrect.";
    return false;
  }
  uint8_t jack_type = data[3];
  uint8_t id = data[4];
  if (jack_type == JACK_TYPE_EMBEDDED) {
    // We can't determine the associated endpoint now.
    incomplete_jacks_.push_back(UsbMidiJack(device, id, 0, 0));
  }
  return true;
}

bool UsbMidiDescriptorParser::ParseEndpoint(const uint8_t* data, size_t size) {
  if (size < 4) {
    DVLOG(1) << "ENDPOINT header size is incorrect.";
    return false;
  }
  current_endpoint_address_ = data[2];
  current_cable_number_ = 0;
  return true;
}

bool UsbMidiDescriptorParser::ParseCSEndpoint(const uint8_t* data,
                                              size_t size,
                                              std::vector<UsbMidiJack>* jacks) {
  const size_t kSizeForEmptyJacks = 4;
  // CS_ENDPOINT must be of size 4 + n where n is the number of associated
  // jacks.
  if (size < kSizeForEmptyJacks) {
    DVLOG(1) << "CS_ENDPOINT header size is incorrect.";
    return false;
  }
  uint8_t num_jacks = data[3];
  if (size != kSizeForEmptyJacks + num_jacks) {
    DVLOG(1) << "CS_ENDPOINT header size is incorrect.";
    return false;
  }

  for (size_t i = 0; i < num_jacks; ++i) {
    uint8_t jack = data[kSizeForEmptyJacks + i];
    auto it = std::find_if(incomplete_jacks_.begin(), incomplete_jacks_.end(),
                           JackMatcher(jack));
    if (it == incomplete_jacks_.end()) {
      DVLOG(1) << "A non-existing MIDI jack is associated.";
      return false;
    }
    if (current_cable_number_ > 0xf) {
      DVLOG(1) << "Cable number should range from 0x0 to 0xf.";
      return false;
    }
    // CS_ENDPOINT follows ENDPOINT and hence we can use the following
    // member variables.
    it->cable_number = current_cable_number_++;
    it->endpoint_address = current_endpoint_address_;
    jacks->push_back(*it);
    incomplete_jacks_.erase(it);
  }
  return true;
}

void UsbMidiDescriptorParser::Clear() {
  is_parsing_usb_midi_interface_ = false;
  current_endpoint_address_ = 0;
  current_cable_number_ = 0;
  incomplete_jacks_.clear();
}

}  // namespace midi
