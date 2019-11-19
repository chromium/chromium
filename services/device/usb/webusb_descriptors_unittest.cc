// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/webusb_descriptors.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/stl_util.h"
#include "services/device/usb/mock_usb_device_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace device {

using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;

namespace {

const uint8_t kExampleBosDescriptor[] = {
    // BOS descriptor.
    0x05, 0x0F, 0x4C, 0x00, 0x03,

    // Container ID descriptor.
    0x14, 0x10, 0x04, 0x00, 0x2A, 0xF9, 0xF6, 0xC2, 0x98, 0x10, 0x2B, 0x49,
    0x8E, 0x64, 0xFF, 0x01, 0x0C, 0x7F, 0x94, 0xE1,

    // WebUSB Platform Capability descriptor.
    0x18, 0x10, 0x05, 0x00, 0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47,
    0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65, 0x00, 0x01, 0x42, 0x01,

    // Microsoft OS 2.0 Platform Capability descriptor.
    0x1C, 0x10, 0x05, 0x00, 0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C,
    0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F, 0x00, 0x00, 0x03, 0x06,
    0x00, 0x00, 0x01, 0x00};

const uint8_t kExampleUrlDescriptor1[] = {
    0x19, 0x03, 0x01, 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'c', 'o',
    'm',  '/',  'i',  'n', 'd', 'e', 'x', '.', 'h', 't', 'm', 'l'};

ACTION_P2(InvokeCallback, data, length) {
  size_t transferred_length = std::min(length, arg6->size());
  memcpy(arg6->front(), data, transferred_length);
  std::move(arg8).Run(UsbTransferStatus::COMPLETED, arg6, transferred_length);
}

void ExpectLandingPage(const GURL& landing_page) {
  EXPECT_EQ(GURL("https://example.com/index.html"), landing_page);
}

class WebUsbDescriptorsTest : public ::testing::Test {};

TEST_F(WebUsbDescriptorsTest, PlatformCapabilityDescriptor) {
  WebUsbPlatformCapabilityDescriptor descriptor;

  ASSERT_TRUE(descriptor.ParseFromBosDescriptor(std::vector<uint8_t>(
      kExampleBosDescriptor,
      kExampleBosDescriptor + sizeof(kExampleBosDescriptor))));
  EXPECT_EQ(0x0100, descriptor.version);
  EXPECT_EQ(0x42, descriptor.vendor_code);
}

TEST_F(WebUsbDescriptorsTest, ShortBosDescriptorHeader) {
  // This BOS descriptor is just too short.
  static const uint8_t kBuffer[] = {0x03, 0x0F, 0x03};

  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongBosDescriptorHeader) {
  // BOS descriptor's bLength is too large.
  static const uint8_t kBuffer[] = {0x06, 0x0F, 0x05, 0x00, 0x01};

  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, InvalidBosDescriptor) {
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(std::vector<uint8_t>(
      kExampleUrlDescriptor1,
      kExampleUrlDescriptor1 + sizeof(kExampleUrlDescriptor1))));
}

TEST_F(WebUsbDescriptorsTest, ShortBosDescriptor) {
  // wTotalLength is less than bLength. bNumDeviceCaps == 1 to expose buffer
  // length checking bugs.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x04, 0x00, 0x01};

  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongBosDescriptor) {
  // wTotalLength is too large. bNumDeviceCaps == 1 to expose buffer
  // length checking bugs.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x06, 0x00, 0x01};

  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, UnexpectedlyEmptyBosDescriptor) {
  // bNumDeviceCaps == 1 but there are no actual descriptors.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x05, 0x00, 0x01};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortCapabilityDescriptor) {
  // The single capability descriptor in the BOS descriptor is too short.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x06, 0x00, 0x01, 0x02, 0x10};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, LongCapabilityDescriptor) {
  // The bLength on a capability descriptor in the BOS descriptor is longer than
  // the remaining space defined by wTotalLength.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x08, 0x00,
                                    0x01, 0x04, 0x10, 0x05};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, NotACapabilityDescriptor) {
  // There is something other than a device capability descriptor in the BOS
  // descriptor.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x08, 0x00,
                                    0x01, 0x03, 0x0F, 0x05};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, NoPlatformCapabilityDescriptor) {
  // The BOS descriptor only contains a Container ID descriptor.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x19, 0x00, 0x01, 0x14, 0x10,
                                    0x04, 0x00, 0x2A, 0xF9, 0xF6, 0xC2, 0x98,
                                    0x10, 0x2B, 0x49, 0x8E, 0x64, 0xFF, 0x01,
                                    0x0C, 0x7F, 0x94, 0xE1};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortPlatformCapabilityDescriptor) {
  // The platform capability descriptor is too short to contain a UUID.
  static const uint8_t kBuffer[] = {
      0x05, 0x0F, 0x18, 0x00, 0x01, 0x13, 0x10, 0x05, 0x00, 0x2A, 0xF9, 0xF6,
      0xC2, 0x98, 0x10, 0x2B, 0x49, 0x8E, 0x64, 0xFF, 0x01, 0x0C, 0x7F, 0x94};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, NoWebUsbCapabilityDescriptor) {
  // The BOS descriptor only contains another kind of platform capability
  // descriptor.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x19, 0x00, 0x01, 0x14, 0x10,
                                    0x05, 0x00, 0x2A, 0xF9, 0xF6, 0xC2, 0x98,
                                    0x10, 0x2B, 0x49, 0x8E, 0x64, 0xFF, 0x01,
                                    0x0C, 0x7F, 0x94, 0xE1};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, ShortWebUsbPlatformCapabilityDescriptor) {
  // The WebUSB Platform Capability Descriptor is too short.
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x19, 0x00, 0x01, 0x14, 0x10,
                                    0x05, 0x00, 0x38, 0xB6, 0x08, 0x34, 0xA9,
                                    0x09, 0xA0, 0x47, 0x8B, 0xFD, 0xA0, 0x76,
                                    0x88, 0x15, 0xB6, 0x65};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, WebUsbPlatformCapabilityDescriptorOutOfDate) {
  // The WebUSB Platform Capability Descriptor is version 0.9 (too old).
  static const uint8_t kBuffer[] = {0x05, 0x0F, 0x1C, 0x00, 0x01, 0x17, 0x10,
                                    0x05, 0x00, 0x38, 0xB6, 0x08, 0x34, 0xA9,
                                    0x09, 0xA0, 0x47, 0x8B, 0xFD, 0xA0, 0x76,
                                    0x88, 0x15, 0xB6, 0x65, 0x90, 0x00, 0x01};
  WebUsbPlatformCapabilityDescriptor descriptor;
  ASSERT_FALSE(descriptor.ParseFromBosDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer))));
}

TEST_F(WebUsbDescriptorsTest, UrlDescriptor) {
  GURL url;
  ASSERT_TRUE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(
          kExampleUrlDescriptor1,
          kExampleUrlDescriptor1 + sizeof(kExampleUrlDescriptor1)),
      &url));
  EXPECT_EQ(GURL("https://example.com/index.html"), url);
}

TEST_F(WebUsbDescriptorsTest, ShortUrlDescriptorHeader) {
  // The buffer is just too darn short.
  static const uint8_t kBuffer[] = {0x01};
  GURL url;
  ASSERT_FALSE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &url));
}

TEST_F(WebUsbDescriptorsTest, ShortUrlDescriptor) {
  // bLength is too short.
  static const uint8_t kBuffer[] = {0x01, 0x03};
  GURL url;
  ASSERT_FALSE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &url));
}

TEST_F(WebUsbDescriptorsTest, LongUrlDescriptor) {
  // bLength is too long.
  static const uint8_t kBuffer[] = {0x03, 0x03};
  GURL url;
  ASSERT_FALSE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &url));
}

TEST_F(WebUsbDescriptorsTest, EmptyUrl) {
  // The URL in this descriptor set is the empty string.
  static const uint8_t kBuffer[] = {0x03, 0x03, 0x00};
  GURL url;
  ASSERT_FALSE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &url));
}

TEST_F(WebUsbDescriptorsTest, InvalidUrl) {
  // The URL in this descriptor set is not a valid URL: "http://???"
  static const uint8_t kBuffer[] = {0x06, 0x03, 0x00, '?', '?', '?'};
  GURL url;
  ASSERT_FALSE(ParseWebUsbUrlDescriptor(
      std::vector<uint8_t>(kBuffer, kBuffer + sizeof(kBuffer)), &url));
}

TEST_F(WebUsbDescriptorsTest, ReadDescriptors) {
  scoped_refptr<MockUsbDeviceHandle> device_handle(
      new MockUsbDeviceHandle(nullptr));

  EXPECT_CALL(*device_handle,
              ControlTransferInternal(UsbTransferDirection::INBOUND,
                                      UsbControlTransferType::STANDARD,
                                      UsbControlTransferRecipient::DEVICE, 0x06,
                                      0x0F00, 0x0000, _, _, _))
      .Times(2)
      .WillRepeatedly(
          InvokeCallback(kExampleBosDescriptor, sizeof(kExampleBosDescriptor)));
  EXPECT_CALL(*device_handle,
              ControlTransferInternal(UsbTransferDirection::INBOUND,
                                      UsbControlTransferType::VENDOR,
                                      UsbControlTransferRecipient::DEVICE, 0x42,
                                      0x0001, 0x0002, _, _, _))
      .WillOnce(InvokeCallback(kExampleUrlDescriptor1,
                               sizeof(kExampleUrlDescriptor1)));

  ReadWebUsbDescriptors(device_handle, base::BindOnce(&ExpectLandingPage));
}

}  // namespace

}  // namespace device
