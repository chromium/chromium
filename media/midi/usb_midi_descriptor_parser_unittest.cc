// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/usb_midi_descriptor_parser.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace midi {

namespace {

TEST(UsbMidiDescriptorParserTest, ParseEmpty) {
  UsbMidiDescriptorParser parser;
  std::vector<UsbMidiJack> jacks;
  EXPECT_TRUE(parser.Parse(nullptr, nullptr, 0, &jacks));
  EXPECT_TRUE(jacks.empty());
}

TEST(UsbMidiDescriptorParserTest, InvalidSize) {
  UsbMidiDescriptorParser parser;
  std::vector<UsbMidiJack> jacks;
  uint8_t data[] = {0x04};
  EXPECT_FALSE(parser.Parse(nullptr, data, std::size(data), &jacks));
  EXPECT_TRUE(jacks.empty());
}

TEST(UsbMidiDescriptorParserTest, NonExistingJackIsAssociated) {
  UsbMidiDescriptorParser parser;
  std::vector<UsbMidiJack> jacks;
  // Jack id=1 is found in a CS_ENDPOINT descriptor, but there is no definition
  // for the jack.
  uint8_t data[] = {
      0x09, 0x04, 0x01, 0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x07, 0x24,
      0x01, 0x00, 0x01, 0x07, 0x00, 0x05, 0x25, 0x01, 0x01, 0x01,
  };
  EXPECT_FALSE(parser.Parse(nullptr, data, std::size(data), &jacks));
  EXPECT_TRUE(jacks.empty());
}

TEST(UsbMidiDescriptorParserTest,
     JacksShouldBeIgnoredWhenParserIsNotParsingMidiInterface) {
  UsbMidiDescriptorParser parser;
  std::vector<UsbMidiJack> jacks;
  // a NON-MIDI INTERFACE descriptor followed by ENDPOINT and CS_ENDPOINT
  // descriptors (Compare with the previous test case).
  uint8_t data[] = {
      0x09, 0x04, 0x01, 0x00, 0x02, 0x01, 0x02, 0x00, 0x00, 0x07, 0x24,
      0x01, 0x00, 0x01, 0x07, 0x00, 0x05, 0x25, 0x01, 0x01, 0x01,
  };
  EXPECT_TRUE(parser.Parse(nullptr, data, std::size(data), &jacks));
  EXPECT_TRUE(jacks.empty());
}

TEST(UsbMidiDescriptorParserTest, Parse) {
  UsbMidiDescriptorParser parser;
  std::vector<UsbMidiJack> jacks;
  // A complete device descriptor.
  uint8_t data[] = {
      0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x86, 0x1a, 0x2d, 0x75,
      0x54, 0x02, 0x00, 0x02, 0x00, 0x01, 0x09, 0x02, 0x75, 0x00, 0x02, 0x01,
      0x00, 0x80, 0x30, 0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00,
      0x09, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01, 0x09, 0x04, 0x01,
      0x00, 0x02, 0x01, 0x03, 0x00, 0x00, 0x07, 0x24, 0x01, 0x00, 0x01, 0x51,
      0x00, 0x06, 0x24, 0x02, 0x01, 0x02, 0x00, 0x06, 0x24, 0x02, 0x01, 0x03,
      0x00, 0x06, 0x24, 0x02, 0x02, 0x06, 0x00, 0x09, 0x24, 0x03, 0x01, 0x07,
      0x01, 0x06, 0x01, 0x00, 0x09, 0x24, 0x03, 0x02, 0x04, 0x01, 0x02, 0x01,
      0x00, 0x09, 0x24, 0x03, 0x02, 0x05, 0x01, 0x03, 0x01, 0x00, 0x09, 0x05,
      0x02, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x06, 0x25, 0x01, 0x02, 0x02,
      0x03, 0x09, 0x05, 0x82, 0x02, 0x20, 0x00, 0x00, 0x00, 0x00, 0x05, 0x25,
      0x01, 0x01, 0x07,
  };
  EXPECT_TRUE(parser.Parse(nullptr, data, std::size(data), &jacks));
  ASSERT_EQ(3u, jacks.size());

  EXPECT_EQ(2u, jacks[0].jack_id);
  EXPECT_EQ(0u, jacks[0].cable_number);
  EXPECT_EQ(2u, jacks[0].endpoint_number());
  EXPECT_EQ(UsbMidiJack::DIRECTION_OUT, jacks[0].direction());
  EXPECT_EQ(nullptr, jacks[0].device.get());

  EXPECT_EQ(3u, jacks[1].jack_id);
  EXPECT_EQ(1u, jacks[1].cable_number);
  EXPECT_EQ(2u, jacks[1].endpoint_number());
  EXPECT_EQ(UsbMidiJack::DIRECTION_OUT, jacks[1].direction());
  EXPECT_EQ(nullptr, jacks[1].device.get());

  EXPECT_EQ(7u, jacks[2].jack_id);
  EXPECT_EQ(0u, jacks[2].cable_number);
  EXPECT_EQ(2u, jacks[2].endpoint_number());
  EXPECT_EQ(UsbMidiJack::DIRECTION_IN, jacks[2].direction());
  EXPECT_EQ(nullptr, jacks[2].device.get());
}

TEST(UsbMidiDescriptorParserTest, ParseDeviceInfoEmpty) {
  UsbMidiDescriptorParser parser;
  UsbMidiDescriptorParser::DeviceInfo info;
  EXPECT_FALSE(parser.ParseDeviceInfo(nullptr, 0, &info));
}

TEST(UsbMidiDescriptorParserTest, ParseDeviceInfo) {
  UsbMidiDescriptorParser parser;
  UsbMidiDescriptorParser::DeviceInfo info;
  uint8_t data[] = {
      0x12, 0x01, 0x10, 0x01, 0x00, 0x00, 0x00, 0x08, 0x01,
      0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x00, 0x0a,
  };
  EXPECT_TRUE(parser.ParseDeviceInfo(data, std::size(data), &info));

  EXPECT_EQ(0x2301, info.vendor_id);
  EXPECT_EQ(0x6745, info.product_id);
  EXPECT_EQ(0xab89, info.bcd_device_version);
  EXPECT_EQ(0xcd, info.manufacturer_index);
  EXPECT_EQ(0xef, info.product_index);
}

TEST(UsbMidiDescriptorParserTest, BcdVersionToString) {
  UsbMidiDescriptorParser::DeviceInfo device_info;
  {
    const std::string version = device_info.BcdVersionToString(0x3456);
    EXPECT_EQ(version, "34.56");
  }
  {
    const std::string invalid_version = device_info.BcdVersionToString(0xb456);
    EXPECT_EQ(invalid_version, "Invalid BCD $b4.56");
  }
  {
    const std::string invalid_version = device_info.BcdVersionToString(0x345d);
    EXPECT_EQ(invalid_version, "Invalid BCD $34.5d");
  }
}

}  // namespace

}  // namespace midi
