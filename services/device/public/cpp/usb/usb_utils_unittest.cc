// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using testing::Return;

class UsbUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    std::vector<mojom::UsbConfigurationInfoPtr> configs;
    configs.push_back(mojom::UsbConfigurationInfo::New());
    configs[0]->interfaces.push_back(mojom::UsbInterfaceInfo::New());

    auto alternate = mojom::UsbAlternateInterfaceInfo::New();
    alternate->alternate_setting = 0;
    alternate->class_code = 0xff;
    alternate->subclass_code = 0x42;
    alternate->protocol_code = 0x01;

    // Endpoint 1 IN
    auto endpoint_1 = mojom::UsbEndpointInfo::New();
    endpoint_1->endpoint_number = 0x01;
    endpoint_1->direction = mojom::UsbTransferDirection::INBOUND;
    alternate->endpoints.push_back(std::move(endpoint_1));

    // Endpoint 2 OUT
    auto endpoint_2 = mojom::UsbEndpointInfo::New();
    endpoint_2->endpoint_number = 0x02;
    endpoint_2->direction = mojom::UsbTransferDirection::OUTBOUND;
    alternate->endpoints.push_back(std::move(endpoint_2));

    configs[0]->interfaces[0]->alternates.push_back(
        std::move(std::move(alternate)));

    android_phone_ =
        new FakeUsbDeviceInfo(/*vendor_id*/ 0x18d1,
                              /*vendor_id*/ 0x4ee2,
                              /*manufacturer_string*/ "Google Inc.",
                              /*product_string*/ "Nexus 5",
                              /*serial_number*/ "ABC123", std::move(configs));
  }

 protected:
  const mojom::UsbDeviceInfo& GetPhoneInfo() {
    EXPECT_TRUE(android_phone_);
    return android_phone_->GetDeviceInfo();
  }

  scoped_refptr<FakeUsbDeviceInfo> android_phone_;
};

TEST_F(UsbUtilsTest, MatchAny) {
  auto filter = mojom::UsbDeviceFilter::New();
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchVendorId) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x18d1;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchVendorIdNegative) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x1d6b;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchProductId) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x18d1;
  filter->has_product_id = true;
  filter->product_id = 0x4ee2;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchProductIdNegative) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x18d1;
  filter->has_product_id = true;
  filter->product_id = 0x4ee1;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchInterfaceClass) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xff;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchInterfaceClassNegative) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xe0;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchInterfaceSubclass) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xff;
  filter->has_subclass_code = true;
  filter->subclass_code = 0x42;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchInterfaceSubclassNegative) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xff;
  filter->has_subclass_code = true;
  filter->subclass_code = 0x01;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchInterfaceProtocol) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xff;
  filter->has_subclass_code = true;
  filter->subclass_code = 0x42;
  filter->has_protocol_code = true;
  filter->protocol_code = 0x01;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchInterfaceProtocolNegative) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xff;
  filter->has_subclass_code = true;
  filter->subclass_code = 0x42;
  filter->has_protocol_code = true;
  filter->protocol_code = 0x02;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchSerialNumber) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->serial_number = u"ABC123";
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
  filter->has_vendor_id = true;
  filter->vendor_id = 0x18d1;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
  filter->vendor_id = 0x18d2;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
  filter->vendor_id = 0x18d1;
  filter->serial_number = u"DIFFERENT";
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchDeviceClass) {
  auto device_info = mojom::UsbDeviceInfo::New();
  device_info->class_code = 1;
  device_info->subclass_code = 2;
  device_info->protocol_code = 3;

  auto matching_class_filter = mojom::UsbDeviceFilter::New();
  matching_class_filter->has_class_code = true;
  matching_class_filter->class_code = 1;
  EXPECT_TRUE(UsbDeviceFilterMatches(*matching_class_filter, *device_info));

  auto nonmatching_class_filter = mojom::UsbDeviceFilter::New();
  nonmatching_class_filter->has_class_code = true;
  nonmatching_class_filter->class_code = 2;
  EXPECT_FALSE(UsbDeviceFilterMatches(*nonmatching_class_filter, *device_info));

  auto matching_subclass_filter = mojom::UsbDeviceFilter::New();
  matching_subclass_filter->has_class_code = true;
  matching_subclass_filter->class_code = 1;
  matching_subclass_filter->has_subclass_code = true;
  matching_subclass_filter->subclass_code = 2;
  EXPECT_TRUE(UsbDeviceFilterMatches(*matching_subclass_filter, *device_info));

  auto nonmatching_subclass_filter = mojom::UsbDeviceFilter::New();
  nonmatching_subclass_filter->has_class_code = true;
  nonmatching_subclass_filter->class_code = 1;
  nonmatching_subclass_filter->has_subclass_code = true;
  nonmatching_subclass_filter->subclass_code = 3;
  EXPECT_FALSE(
      UsbDeviceFilterMatches(*nonmatching_subclass_filter, *device_info));

  auto matching_protocol_filter = mojom::UsbDeviceFilter::New();
  matching_protocol_filter->has_class_code = true;
  matching_protocol_filter->class_code = 1;
  matching_protocol_filter->has_subclass_code = true;
  matching_protocol_filter->subclass_code = 2;
  matching_protocol_filter->has_protocol_code = true;
  matching_protocol_filter->protocol_code = 3;
  EXPECT_TRUE(UsbDeviceFilterMatches(*matching_protocol_filter, *device_info));

  auto nonmatching_protocol_filter = mojom::UsbDeviceFilter::New();
  nonmatching_protocol_filter->has_class_code = true;
  nonmatching_protocol_filter->class_code = 1;
  nonmatching_protocol_filter->has_subclass_code = true;
  nonmatching_protocol_filter->subclass_code = 2;
  nonmatching_protocol_filter->has_protocol_code = true;
  nonmatching_protocol_filter->protocol_code = 1;
  EXPECT_FALSE(
      UsbDeviceFilterMatches(*nonmatching_protocol_filter, *device_info));

  // Without |has_subclass_code| set the |protocol_code| filter should be
  // ignored.
  auto invalid_matching_protocol_filter = mojom::UsbDeviceFilter::New();
  invalid_matching_protocol_filter->has_class_code = true;
  invalid_matching_protocol_filter->class_code = 1;
  invalid_matching_protocol_filter->has_protocol_code = true;
  invalid_matching_protocol_filter->protocol_code = 2;
  EXPECT_TRUE(
      UsbDeviceFilterMatches(*invalid_matching_protocol_filter, *device_info));
}

TEST_F(UsbUtilsTest, MatchAnyEmptyList) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  ASSERT_TRUE(UsbDeviceFilterMatchesAny(filters, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchesAnyVendorId) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  filters.push_back(mojom::UsbDeviceFilter::New());
  filters.back()->has_vendor_id = true;
  filters.back()->vendor_id = 0x18d1;
  ASSERT_TRUE(UsbDeviceFilterMatchesAny(filters, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, MatchesAnyVendorIdNegative) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  filters.push_back(mojom::UsbDeviceFilter::New());
  filters.back()->has_vendor_id = true;
  filters.back()->vendor_id = 0x1d6b;
  ASSERT_FALSE(UsbDeviceFilterMatchesAny(filters, GetPhoneInfo()));
}

TEST_F(UsbUtilsTest, EndpointDirectionNumberConversion) {
  auto& alternate =
      GetPhoneInfo().configurations[0]->interfaces[0]->alternates[0];
  EXPECT_EQ(2u, alternate->endpoints.size());

  // Check Endpoint 1 INs
  auto& mojo_endpoint_1 = alternate->endpoints[0];
  ASSERT_EQ(ConvertEndpointNumberToAddress(*mojo_endpoint_1), 0x81);
  ASSERT_EQ(0x01, ConvertEndpointAddressToNumber(0x81));

  // Check Endpoint 2 OUT
  auto& mojo_endpoint_2 = alternate->endpoints[1];
  ASSERT_EQ(ConvertEndpointNumberToAddress(*mojo_endpoint_2), 0x02);
  ASSERT_EQ(0x02, ConvertEndpointAddressToNumber(0x02));
}

}  // namespace

}  // namespace device
