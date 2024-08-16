// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/usb/usb_descriptors.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "services/device/usb/mock_usb_device_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace device {

using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbSynchronizationType;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;
using mojom::UsbTransferType;
using mojom::UsbUsageType;

namespace {

ACTION_P2(InvokeCallback, data, length) {
  size_t transferred_length = std::min(length, arg6->size());
  base::span(arg6->as_vector())
      .copy_prefix_from(
          UNSAFE_TODO(base::span(data, length)).first(transferred_length));
  std::move(arg8).Run(UsbTransferStatus::COMPLETED, arg6, transferred_length);
}

void ExpectStringDescriptors(
    std::unique_ptr<std::map<uint8_t, std::u16string>> string_map) {
  EXPECT_EQ(3u, string_map->size());
  EXPECT_EQ(u"String 1", (*string_map)[1]);
  EXPECT_EQ(u"String 2", (*string_map)[2]);
  EXPECT_EQ(u"String 3", (*string_map)[3]);
}

// clang-format off
const uint8_t kDeviceDescriptor[] = {0x12, 0x01, 0x10, 0x03, 0xFF, 0xFF,
                                     0xFF, 0x09, 0x34, 0x12, 0x78, 0x56,
                                     0x00, 0x01, 0x01, 0x02, 0x03, 0x02};

const uint8_t kConfig1Descriptor[] = {
    // Config 1
    0x09, 0x02, 0x38, 0x00, 0x02, 0x01, 0x01, 0x01, 0x10,
    // Interface Association (0 + 1)
    0x08, 0x0B, 0x00, 0x02, 0xFF, 0xFF, 0xFF, 0x00,
    // Interface 0
    0x09, 0x04, 0x00, 0x00, 0x03, 0x12, 0x34, 0x56, 0x02,
    // Endpoint 1 IN
    0x07, 0x05, 0x81, 0x02, 0x00, 0x02, 0x00,
    // Endpoint 2 IN
    0x07, 0x05, 0x82, 0x03, 0x00, 0x02, 0x04,
    // Endpoint 3 OUT
    0x07, 0x05, 0x03, 0x13, 0x00, 0x02, 0x04,
    // Interface 1
    0x09, 0x04, 0x01, 0x00, 0x00, 0x78, 0x9A, 0xAB, 0x03,
};

const uint8_t kConfig2Descriptor[] = {
    // Config 2
    0x09, 0x02, 0x29, 0x00, 0x01, 0x02, 0x04, 0x03, 0x20,
    // Interface 0 (alternate 0)
    0x09, 0x04, 0x00, 0x00, 0x00, 0xCD, 0xEF, 0x01, 0x04,
    // Interface 0 (alternate 1)
    0x09, 0x04, 0x00, 0x01, 0x02, 0xCD, 0xEF, 0x01, 0x05,
    // Endpoint 1 IN
    0x07, 0x05, 0x81, 0x01, 0x00, 0x04, 0x08,
    // Endpoint 2 OUT
    0x07, 0x05, 0x02, 0x11, 0x00, 0x04, 0x08,
};
// clang-format on

void ExpectConfig1Info(const mojom::UsbConfigurationInfo& config) {
  // Config 1
  EXPECT_EQ(1, config.configuration_value);
  EXPECT_FALSE(config.self_powered);
  EXPECT_FALSE(config.remote_wakeup);
  EXPECT_EQ(16, config.maximum_power);
  ASSERT_EQ(2u, config.interfaces.size());
  EXPECT_EQ(8u, config.extra_data.size());
  // Interface 0
  EXPECT_EQ(0, config.interfaces[0]->interface_number);
  EXPECT_EQ(0, config.interfaces[0]->first_interface);
  const auto& alternate = config.interfaces[0]->alternates[0];
  EXPECT_EQ(0, alternate->alternate_setting);
  EXPECT_EQ(0x12, alternate->class_code);
  EXPECT_EQ(0x34, alternate->subclass_code);
  EXPECT_EQ(0x56, alternate->protocol_code);
  ASSERT_EQ(3u, alternate->endpoints.size());
  EXPECT_EQ(0u, alternate->extra_data.size());

  // Endpoint 1 IN
  EXPECT_EQ(0x01, alternate->endpoints[0]->endpoint_number);
  EXPECT_EQ(UsbTransferDirection::INBOUND, alternate->endpoints[0]->direction);
  EXPECT_EQ(512u, alternate->endpoints[0]->packet_size);
  EXPECT_EQ(UsbSynchronizationType::NONE,
            alternate->endpoints[0]->synchronization_type);
  EXPECT_EQ(UsbTransferType::BULK, alternate->endpoints[0]->type);
  EXPECT_EQ(UsbUsageType::RESERVED, alternate->endpoints[0]->usage_type);
  EXPECT_EQ(0, alternate->endpoints[0]->polling_interval);
  EXPECT_EQ(0u, alternate->endpoints[0]->extra_data.size());
  // Endpoint 2 IN
  EXPECT_EQ(0x02, alternate->endpoints[1]->endpoint_number);
  EXPECT_EQ(UsbTransferDirection::INBOUND, alternate->endpoints[1]->direction);
  EXPECT_EQ(512u, alternate->endpoints[1]->packet_size);
  EXPECT_EQ(UsbSynchronizationType::NONE,
            alternate->endpoints[1]->synchronization_type);
  EXPECT_EQ(UsbTransferType::INTERRUPT, alternate->endpoints[1]->type);
  EXPECT_EQ(UsbUsageType::PERIODIC, alternate->endpoints[1]->usage_type);
  EXPECT_EQ(4, alternate->endpoints[1]->polling_interval);
  EXPECT_EQ(0u, alternate->endpoints[1]->extra_data.size());
  // Endpoint 3 OUT
  EXPECT_EQ(0x03, alternate->endpoints[2]->endpoint_number);
  EXPECT_EQ(UsbTransferDirection::OUTBOUND, alternate->endpoints[2]->direction);
  EXPECT_EQ(512u,
            config.interfaces[0]->alternates[0]->endpoints[2]->packet_size);
  EXPECT_EQ(UsbSynchronizationType::NONE,
            alternate->endpoints[2]->synchronization_type);
  EXPECT_EQ(UsbTransferType::INTERRUPT, alternate->endpoints[2]->type);
  EXPECT_EQ(UsbUsageType::NOTIFICATION, alternate->endpoints[2]->usage_type);
  EXPECT_EQ(4, alternate->endpoints[2]->polling_interval);
  EXPECT_EQ(0u, alternate->endpoints[2]->extra_data.size());
  // Interface 1
  EXPECT_EQ(1, config.interfaces[1]->interface_number);
  EXPECT_EQ(0, config.interfaces[1]->first_interface);
  EXPECT_EQ(0, config.interfaces[1]->alternates[0]->alternate_setting);
  EXPECT_EQ(0x78, config.interfaces[1]->alternates[0]->class_code);
  EXPECT_EQ(0x9A, config.interfaces[1]->alternates[0]->subclass_code);
  EXPECT_EQ(0xAB, config.interfaces[1]->alternates[0]->protocol_code);
  ASSERT_EQ(0u, config.interfaces[1]->alternates[0]->endpoints.size());
  EXPECT_EQ(0u, config.interfaces[1]->alternates[0]->extra_data.size());
}

void ExpectConfig2Info(const mojom::UsbConfigurationInfo& config) {
  // Config 2
  EXPECT_EQ(2, config.configuration_value);
  EXPECT_TRUE(config.self_powered);
  EXPECT_FALSE(config.remote_wakeup);
  EXPECT_EQ(32, config.maximum_power);
  ASSERT_EQ(1u, config.interfaces.size());
  ASSERT_EQ(2u, config.interfaces[0]->alternates.size());
  EXPECT_EQ(0u, config.extra_data.size());
  // Interface 0
  EXPECT_EQ(0, config.interfaces[0]->interface_number);
  EXPECT_EQ(0, config.interfaces[0]->alternates[0]->alternate_setting);
  EXPECT_EQ(0xCD, config.interfaces[0]->alternates[0]->class_code);
  EXPECT_EQ(0xEF, config.interfaces[0]->alternates[0]->subclass_code);
  EXPECT_EQ(0x01, config.interfaces[0]->alternates[0]->protocol_code);
  ASSERT_EQ(0u, config.interfaces[0]->alternates[0]->endpoints.size());
  EXPECT_EQ(0u, config.interfaces[0]->alternates[0]->extra_data.size());
  EXPECT_EQ(0, config.interfaces[0]->first_interface);
  // Interface 0 (alternate 1)
  const auto& alternate = config.interfaces[0]->alternates[1];
  EXPECT_EQ(1, alternate->alternate_setting);
  EXPECT_EQ(0xCD, alternate->class_code);
  EXPECT_EQ(0xEF, alternate->subclass_code);
  EXPECT_EQ(0x01, alternate->protocol_code);
  ASSERT_EQ(2u, alternate->endpoints.size());
  EXPECT_EQ(0u, alternate->extra_data.size());
  // Endpoint 1 IN
  EXPECT_EQ(0x01, alternate->endpoints[0]->endpoint_number);
  EXPECT_EQ(UsbTransferDirection::INBOUND, alternate->endpoints[0]->direction);
  EXPECT_EQ(1024u, alternate->endpoints[0]->packet_size);
  EXPECT_EQ(UsbSynchronizationType::NONE,
            alternate->endpoints[0]->synchronization_type);
  EXPECT_EQ(UsbTransferType::ISOCHRONOUS, alternate->endpoints[0]->type);
  EXPECT_EQ(UsbUsageType::DATA, alternate->endpoints[0]->usage_type);
  EXPECT_EQ(8, alternate->endpoints[0]->polling_interval);
  EXPECT_EQ(0u, alternate->endpoints[0]->extra_data.size());
  // Endpoint 2 OUT
  EXPECT_EQ(0x02, alternate->endpoints[1]->endpoint_number);
  EXPECT_EQ(UsbTransferDirection::OUTBOUND, alternate->endpoints[1]->direction);
  EXPECT_EQ(1024u, alternate->endpoints[1]->packet_size);
  EXPECT_EQ(UsbSynchronizationType::NONE,
            alternate->endpoints[1]->synchronization_type);
  EXPECT_EQ(UsbTransferType::ISOCHRONOUS, alternate->endpoints[1]->type);
  EXPECT_EQ(UsbUsageType::FEEDBACK, alternate->endpoints[1]->usage_type);
  EXPECT_EQ(8, alternate->endpoints[1]->polling_interval);
  EXPECT_EQ(0u, alternate->endpoints[1]->extra_data.size());
}

void ExpectDeviceDescriptor(const UsbDeviceDescriptor& descriptor) {
  // Device
  EXPECT_EQ(0x03, descriptor.device_info->usb_version_major);
  EXPECT_EQ(0x01, descriptor.device_info->usb_version_minor);
  EXPECT_EQ(0x00, descriptor.device_info->usb_version_subminor);
  EXPECT_EQ(0xFF, descriptor.device_info->class_code);
  EXPECT_EQ(0xFF, descriptor.device_info->subclass_code);
  EXPECT_EQ(0xFF, descriptor.device_info->protocol_code);
  EXPECT_EQ(0x1234, descriptor.device_info->vendor_id);
  EXPECT_EQ(0x5678, descriptor.device_info->product_id);
  EXPECT_EQ(0x01, descriptor.device_info->device_version_major);
  EXPECT_EQ(0x00, descriptor.device_info->device_version_minor);
  EXPECT_EQ(0x00, descriptor.device_info->device_version_subminor);
  ASSERT_EQ(2u, descriptor.device_info->configurations.size());
  ExpectConfig1Info(*descriptor.device_info->configurations[0]);
  ExpectConfig2Info(*descriptor.device_info->configurations[1]);
}

void OnReadDescriptors(std::unique_ptr<UsbDeviceDescriptor> descriptor) {
  ASSERT_TRUE(descriptor);
  ExpectDeviceDescriptor(*descriptor);
}

class UsbDescriptorsTest : public ::testing::Test {};

TEST_F(UsbDescriptorsTest, ParseDescriptor) {
  std::vector<uint8_t> buffer;
  buffer.insert(buffer.end(), kDeviceDescriptor,
                kDeviceDescriptor + sizeof(kDeviceDescriptor));
  buffer.insert(buffer.end(), kConfig1Descriptor,
                kConfig1Descriptor + sizeof(kConfig1Descriptor));
  buffer.insert(buffer.end(), kConfig2Descriptor,
                kConfig2Descriptor + sizeof(kConfig2Descriptor));

  UsbDeviceDescriptor descriptor;
  ASSERT_TRUE(descriptor.Parse(buffer));
  ExpectDeviceDescriptor(descriptor);
}

TEST_F(UsbDescriptorsTest, ReadDescriptors) {
  scoped_refptr<MockUsbDeviceHandle> device_handle(
      new MockUsbDeviceHandle(nullptr));
  EXPECT_CALL(*device_handle,
              ControlTransferInternal(UsbTransferDirection::INBOUND,
                                      UsbControlTransferType::STANDARD,
                                      UsbControlTransferRecipient::DEVICE, 0x06,
                                      0x0100, 0x0000, _, _, _))
      .WillOnce(InvokeCallback(kDeviceDescriptor, sizeof(kDeviceDescriptor)));
  EXPECT_CALL(*device_handle,
              ControlTransferInternal(UsbTransferDirection::INBOUND,
                                      UsbControlTransferType::STANDARD,
                                      UsbControlTransferRecipient::DEVICE, 0x06,
                                      0x0200, 0x0000, _, _, _))
      .Times(2)
      .WillRepeatedly(
          InvokeCallback(kConfig1Descriptor, sizeof(kConfig1Descriptor)));
  EXPECT_CALL(*device_handle,
              ControlTransferInternal(UsbTransferDirection::INBOUND,
                                      UsbControlTransferType::STANDARD,
                                      UsbControlTransferRecipient::DEVICE, 0x06,
                                      0x0201, 0x0000, _, _, _))
      .Times(2)
      .WillRepeatedly(
          InvokeCallback(kConfig2Descriptor, sizeof(kConfig2Descriptor)));

  ReadUsbDescriptors(device_handle, base::BindOnce(&OnReadDescriptors));
}

TEST_F(UsbDescriptorsTest, NoInterfaceAssociations) {
  mojom::UsbConfigurationInfoPtr config =
      BuildUsbConfigurationInfoPtr(1, false, false, 0);
  config->interfaces.push_back(BuildUsbInterfaceInfoPtr(0, 0, 255, 255, 255));
  config->interfaces.push_back(BuildUsbInterfaceInfoPtr(0, 1, 255, 255, 255));
  config->interfaces.push_back(BuildUsbInterfaceInfoPtr(1, 0, 255, 255, 255));
  AssignFirstInterfaceNumbers(config.get());

  EXPECT_EQ(0, config->interfaces[0]->first_interface);
  EXPECT_EQ(0, config->interfaces[1]->first_interface);
  EXPECT_EQ(1, config->interfaces[2]->first_interface);
}

TEST_F(UsbDescriptorsTest, InterfaceAssociations) {
  // Links interfaces 0 and 1 into a single function.
  static const uint8_t kIAD1[] = {0x08, 0x0b, 0x00, 0x02,
                                  0xff, 0xff, 0xff, 0x00};
  // Only references a single interface, 2.
  static const uint8_t kIAD2[] = {0x08, 0x0b, 0x02, 0x01,
                                  0xff, 0xff, 0xff, 0x00};
  // Malformed. References interface 3 but bInterfaceCount is 0.
  static const uint8_t kIAD3[] = {0x08, 0x0b, 0x03, 0x00,
                                  0xff, 0xff, 0xff, 0x00};
  // Links interfaces 4 and 5 into a single function.
  static const uint8_t kIAD4[] = {0x08, 0x0b, 0x04, 0x02,
                                  0xff, 0xff, 0xff, 0x00};

  mojom::UsbConfigurationInfoPtr config =
      BuildUsbConfigurationInfoPtr(1, false, false, 0);
  config->extra_data.assign(kIAD1, kIAD1 + sizeof(kIAD1));
  config->extra_data.insert(config->extra_data.end(), kIAD2,
                            kIAD2 + sizeof(kIAD2));
  config->interfaces.push_back(BuildUsbInterfaceInfoPtr(0, 0, 255, 255, 255));
  config->interfaces.push_back(BuildUsbInterfaceInfoPtr(1, 0, 255, 255, 255));
  mojom::UsbInterfaceInfoPtr iface1a =
      BuildUsbInterfaceInfoPtr(1, 1, 255, 255, 255);
  iface1a->alternates[0]->extra_data.assign(kIAD3, kIAD3 + sizeof(kIAD3));
  config->interfaces.push_back(std::move(iface1a));
  config->interfaces.push_back(BuildUsbInterfaceInfoPtr(2, 0, 255, 255, 255));
  config->interfaces.push_back(BuildUsbInterfaceInfoPtr(3, 0, 255, 255, 255));
  mojom::UsbInterfaceInfoPtr iface4 =
      BuildUsbInterfaceInfoPtr(4, 0, 255, 255, 255);
  iface4->alternates[0]->extra_data.assign(kIAD4, kIAD4 + sizeof(kIAD4));
  config->interfaces.push_back(std::move(iface4));
  config->interfaces.push_back(BuildUsbInterfaceInfoPtr(5, 0, 255, 255, 255));
  AssignFirstInterfaceNumbers(config.get());

  // Interfaces 0 and 1 (plus 1's alternate) are a single function.
  EXPECT_EQ(0, config->interfaces[0]->interface_number);
  EXPECT_EQ(0, config->interfaces[0]->first_interface);
  EXPECT_EQ(1, config->interfaces[1]->interface_number);
  EXPECT_EQ(0, config->interfaces[1]->first_interface);
  EXPECT_EQ(1, config->interfaces[2]->interface_number);
  EXPECT_EQ(0, config->interfaces[2]->first_interface);

  // Interfaces 2 and 3 are their own functions.
  EXPECT_EQ(2, config->interfaces[3]->interface_number);
  EXPECT_EQ(2, config->interfaces[3]->first_interface);
  EXPECT_EQ(3, config->interfaces[4]->interface_number);
  EXPECT_EQ(3, config->interfaces[4]->first_interface);

  // Interfaces 4 and 5 are a single function.
  EXPECT_EQ(4, config->interfaces[5]->interface_number);
  EXPECT_EQ(4, config->interfaces[5]->first_interface);
  EXPECT_EQ(5, config->interfaces[6]->interface_number);
  EXPECT_EQ(4, config->interfaces[6]->first_interface);
}

TEST_F(UsbDescriptorsTest, CorruptInterfaceAssociations) {
  {
    // Descriptor is too short.
    static const uint8_t kIAD[] = {0x01};
    mojom::UsbConfigurationInfoPtr config =
        BuildUsbConfigurationInfoPtr(1, false, false, 0);
    config->extra_data.assign(kIAD, kIAD + sizeof(kIAD));
    AssignFirstInterfaceNumbers(config.get());
  }
  {
    // Descriptor is too long.
    static const uint8_t kIAD[] = {0x09, 0x0b, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00};
    mojom::UsbConfigurationInfoPtr config =
        BuildUsbConfigurationInfoPtr(1, false, false, 0);
    config->extra_data.assign(kIAD, kIAD + sizeof(kIAD));
    AssignFirstInterfaceNumbers(config.get());
  }
  {
    // References an undefined interface.
    static const uint8_t kIAD[] = {0x08, 0x0b, 0x07, 0x00,
                                   0xff, 0xff, 0xff, 0x00};
    mojom::UsbConfigurationInfoPtr config =
        BuildUsbConfigurationInfoPtr(1, false, false, 0);
    config->interfaces.push_back(BuildUsbInterfaceInfoPtr(0, 0, 255, 255, 255));
    config->extra_data.assign(kIAD, kIAD + sizeof(kIAD));
    AssignFirstInterfaceNumbers(config.get());

    EXPECT_EQ(0, config->interfaces[0]->interface_number);
    EXPECT_EQ(0, config->interfaces[0]->first_interface);
  }
}

TEST_F(UsbDescriptorsTest, StringDescriptor) {
  static const uint8_t kBuffer[] = {0x1a, 0x03, 'H', 0, 'e', 0, 'l', 0, 'l', 0,
                                    'o',  0,    ' ', 0, 'w', 0, 'o', 0, 'r', 0,
                                    'l',  0,    'd', 0, '!', 0};
  std::u16string string;
  ASSERT_TRUE(ParseUsbStringDescriptor(kBuffer, &string));
  EXPECT_EQ(u"Hello world!", string);
}

TEST_F(UsbDescriptorsTest, ShortStringDescriptorHeader) {
  // The buffer is just too darn short.
  static const uint8_t kBuffer[] = {0x01};
  std::u16string string;
  ASSERT_FALSE(ParseUsbStringDescriptor(kBuffer, &string));
}

TEST_F(UsbDescriptorsTest, ShortStringDescriptor) {
  // The buffer is just too darn short.
  static const uint8_t kBuffer[] = {0x01, 0x03};
  std::u16string string;
  ASSERT_FALSE(ParseUsbStringDescriptor(kBuffer, &string));
}

TEST_F(UsbDescriptorsTest, OddLengthStringDescriptor) {
  // There's an extra byte at the end of the string.
  static const uint8_t kBuffer[] = {0x0d, 0x03, 'H', 0,   'e', 0,  'l',
                                    0,    'l',  0,   'o', 0,   '!'};
  std::u16string string;
  ASSERT_TRUE(ParseUsbStringDescriptor(kBuffer, &string));
  EXPECT_EQ(u"Hello", string);
}

TEST_F(UsbDescriptorsTest, EmptyStringDescriptor) {
  // The string is empty.
  static const uint8_t kBuffer[] = {0x02, 0x03};
  std::u16string string;
  ASSERT_TRUE(ParseUsbStringDescriptor(kBuffer, &string));
  EXPECT_EQ(std::u16string(), string);
}

TEST_F(UsbDescriptorsTest, OneByteStringDescriptor) {
  // The string is only one byte.
  static const uint8_t kBuffer[] = {0x03, 0x03, '?'};
  std::u16string string;
  ASSERT_TRUE(ParseUsbStringDescriptor(kBuffer, &string));
  EXPECT_EQ(std::u16string(), string);
}

TEST_F(UsbDescriptorsTest, ReadStringDescriptors) {
  std::unique_ptr<std::map<uint8_t, std::u16string>> string_map(
      new std::map<uint8_t, std::u16string>());
  (*string_map)[1] = std::u16string();
  (*string_map)[2] = std::u16string();
  (*string_map)[3] = std::u16string();

  scoped_refptr<MockUsbDeviceHandle> device_handle(
      new MockUsbDeviceHandle(nullptr));
  static const uint8_t kStringDescriptor0[] = {0x04, 0x03, 0x21, 0x43};
  EXPECT_CALL(*device_handle,
              ControlTransferInternal(UsbTransferDirection::INBOUND,
                                      UsbControlTransferType::STANDARD,
                                      UsbControlTransferRecipient::DEVICE, 0x06,
                                      0x0300, 0x0000, _, _, _))
      .WillOnce(InvokeCallback(kStringDescriptor0, sizeof(kStringDescriptor0)));
  static const uint8_t kStringDescriptor1[] = {0x12, 0x03, 'S', 0, 't', 0,
                                               'r',  0,    'i', 0, 'n', 0,
                                               'g',  0,    ' ', 0, '1', 0};
  EXPECT_CALL(*device_handle,
              ControlTransferInternal(UsbTransferDirection::INBOUND,
                                      UsbControlTransferType::STANDARD,
                                      UsbControlTransferRecipient::DEVICE, 0x06,
                                      0x0301, 0x4321, _, _, _))
      .WillOnce(InvokeCallback(kStringDescriptor1, sizeof(kStringDescriptor1)));
  static const uint8_t kStringDescriptor2[] = {0x12, 0x03, 'S', 0, 't', 0,
                                               'r',  0,    'i', 0, 'n', 0,
                                               'g',  0,    ' ', 0, '2', 0};
  EXPECT_CALL(*device_handle,
              ControlTransferInternal(UsbTransferDirection::INBOUND,
                                      UsbControlTransferType::STANDARD,
                                      UsbControlTransferRecipient::DEVICE, 0x06,
                                      0x0302, 0x4321, _, _, _))
      .WillOnce(InvokeCallback(kStringDescriptor2, sizeof(kStringDescriptor2)));
  static const uint8_t kStringDescriptor3[] = {0x12, 0x03, 'S', 0, 't', 0,
                                               'r',  0,    'i', 0, 'n', 0,
                                               'g',  0,    ' ', 0, '3', 0};
  EXPECT_CALL(*device_handle,
              ControlTransferInternal(UsbTransferDirection::INBOUND,
                                      UsbControlTransferType::STANDARD,
                                      UsbControlTransferRecipient::DEVICE, 0x06,
                                      0x0303, 0x4321, _, _, _))
      .WillOnce(InvokeCallback(kStringDescriptor3, sizeof(kStringDescriptor3)));

  ReadUsbStringDescriptors(device_handle, std::move(string_map),
                           base::BindOnce(&ExpectStringDescriptors));
}

}  // namespace

}  // namespace device
