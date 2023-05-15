// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_device_info.h"

#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "services/device/public/cpp/hid/hid_report_type.h"
#include "services/device/public/cpp/hid/hid_report_utils.h"
#include "services/device/public/cpp/test/test_report_descriptors.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

scoped_refptr<HidDeviceInfo> CreateHidDeviceInfo(
    base::span<const uint8_t> report_descriptor) {
#if BUILDFLAG(IS_MAC)
  const uint64_t kTestDeviceId = 0;
#elif BUILDFLAG(IS_WIN)
  const wchar_t* const kTestDeviceId = L"0";
#else
  const char* const kTestDeviceId = "0";
#endif
  return base::MakeRefCounted<HidDeviceInfo>(
      kTestDeviceId, "physical-device-id",
      /*vendor_id=*/0x1234,
      /*product_id=*/0xabcd, "product-name", "serial-number",
      mojom::HidBusType::kHIDBusTypeUSB, report_descriptor);
}

TEST(HidDeviceInfoTest, FindCollectionWithReport_MultipleCollections) {
  // The device has 8 reports (4 input, 4 output) spread over 3 top-level
  // collections.
  auto device =
      CreateHidDeviceInfo(TestReportDescriptors::LogitechUnifyingReceiver());
  EXPECT_TRUE(device->has_report_id());
  EXPECT_EQ(3u, device->collections().size());
  base::flat_map<uint16_t, device::mojom::HidCollectionInfo*> collections;
  for (const auto& collection : device->collections()) {
    ASSERT_TRUE(collection->usage);
    EXPECT_EQ(mojom::kPageVendor, collection->usage->usage_page);
    collections[collection->usage->usage] = collection.get();
  }
  ASSERT_TRUE(collections.contains(1));
  ASSERT_TRUE(collections.contains(2));
  ASSERT_TRUE(collections.contains(4));

  EXPECT_THAT(collections[1]->report_ids, ElementsAre(0x10));
  ASSERT_EQ(1u, collections[1]->input_reports.size());
  EXPECT_EQ(0x10, collections[1]->input_reports[0]->report_id);
  ASSERT_EQ(1u, collections[1]->output_reports.size());
  EXPECT_EQ(0x10, collections[1]->output_reports[0]->report_id);
  EXPECT_EQ(0u, collections[1]->feature_reports.size());

  EXPECT_THAT(collections[2]->report_ids, ElementsAre(0x11));
  ASSERT_EQ(1u, collections[2]->input_reports.size());
  EXPECT_EQ(0x11, collections[2]->input_reports[0]->report_id);
  ASSERT_EQ(1u, collections[2]->output_reports.size());
  EXPECT_EQ(0x11, collections[2]->output_reports[0]->report_id);
  EXPECT_EQ(0u, collections[2]->feature_reports.size());

  EXPECT_THAT(collections[4]->report_ids, UnorderedElementsAre(0x20, 0x21));
  std::vector<uint8_t> input_report_ids;
  for (const auto& report : collections[4]->input_reports)
    input_report_ids.push_back(report->report_id);
  EXPECT_THAT(input_report_ids, UnorderedElementsAre(0x20, 0x21));
  std::vector<uint8_t> output_report_ids;
  for (const auto& report : collections[4]->output_reports)
    output_report_ids.push_back(report->report_id);
  EXPECT_THAT(output_report_ids, UnorderedElementsAre(0x20, 0x21));
  EXPECT_EQ(0u, collections[4]->feature_reports.size());

  // Ensure the correct collection is returned for each report.
  EXPECT_EQ(collections[1], FindCollectionWithReport(*device->device(), 0x10,
                                                     HidReportType::kInput));
  EXPECT_EQ(collections[1], FindCollectionWithReport(*device->device(), 0x10,
                                                     HidReportType::kOutput));
  EXPECT_EQ(collections[2], FindCollectionWithReport(*device->device(), 0x11,
                                                     HidReportType::kInput));
  EXPECT_EQ(collections[2], FindCollectionWithReport(*device->device(), 0x11,
                                                     HidReportType::kOutput));
  EXPECT_EQ(collections[4], FindCollectionWithReport(*device->device(), 0x20,
                                                     HidReportType::kInput));
  EXPECT_EQ(collections[4], FindCollectionWithReport(*device->device(), 0x20,
                                                     HidReportType::kOutput));
  EXPECT_EQ(collections[4], FindCollectionWithReport(*device->device(), 0x21,
                                                     HidReportType::kInput));
  EXPECT_EQ(collections[4], FindCollectionWithReport(*device->device(), 0x21,
                                                     HidReportType::kOutput));

  // Zero is not a valid report ID. Ensure no collection info is returned.
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0,
                                              HidReportType::kInput));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0,
                                              HidReportType::kOutput));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0,
                                              HidReportType::kFeature));

  // Ensure no collection is returned for reports not supported by the device.
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0x10,
                                              HidReportType::kFeature));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0x11,
                                              HidReportType::kFeature));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0x20,
                                              HidReportType::kFeature));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0x21,
                                              HidReportType::kFeature));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0x30,
                                              HidReportType::kInput));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0x30,
                                              HidReportType::kOutput));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0x30,
                                              HidReportType::kFeature));
}

TEST(HidDeviceInfoTest, FindCollectionWithReport_SameReportId) {
  // The device has 6 reports (1 input, 1 output, 4 feature). The input report,
  // output report, and first feature report share the same report ID.
  auto device = CreateHidDeviceInfo(TestReportDescriptors::SonyDualshock3Usb());
  EXPECT_TRUE(device->has_report_id());
  ASSERT_EQ(1u, device->collections().size());
  const auto* collection = device->collections()[0].get();
  EXPECT_FALSE(collection->report_ids.empty());
  ASSERT_EQ(1u, collection->input_reports.size());
  EXPECT_EQ(0x01, collection->input_reports[0]->report_id);
  ASSERT_EQ(1u, collection->output_reports.size());
  EXPECT_EQ(0x01, collection->output_reports[0]->report_id);
  ASSERT_EQ(4u, collection->feature_reports.size());
  std::vector<uint8_t> feature_report_ids;
  for (const auto& report : collection->feature_reports)
    feature_report_ids.push_back(report->report_id);
  EXPECT_THAT(feature_report_ids, UnorderedElementsAre(0x01, 0x02, 0xee, 0xef));

  // Ensure the correct collection is returned for each report.
  EXPECT_EQ(collection, FindCollectionWithReport(*device->device(), 0x01,
                                                 HidReportType::kInput));
  EXPECT_EQ(collection, FindCollectionWithReport(*device->device(), 0x01,
                                                 HidReportType::kOutput));
  EXPECT_EQ(collection, FindCollectionWithReport(*device->device(), 0x01,
                                                 HidReportType::kFeature));
  EXPECT_EQ(collection, FindCollectionWithReport(*device->device(), 0x02,
                                                 HidReportType::kFeature));
  EXPECT_EQ(collection, FindCollectionWithReport(*device->device(), 0xee,
                                                 HidReportType::kFeature));
  EXPECT_EQ(collection, FindCollectionWithReport(*device->device(), 0xef,
                                                 HidReportType::kFeature));

  // Zero is not a valid report ID. Ensure no collection info is returned.
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0,
                                              HidReportType::kInput));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0,
                                              HidReportType::kOutput));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0,
                                              HidReportType::kFeature));

  // Ensure no collection is returned for reports not supported by the device.
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0x02,
                                              HidReportType::kInput));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0x02,
                                              HidReportType::kOutput));
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0x03,
                                              HidReportType::kFeature));
}

TEST(HidDeviceInfoTest, FindCollectionWithReport_NoReportIds) {
  // The device has 2 reports (1 input, 1 output) and does not use report IDs.
  auto device = CreateHidDeviceInfo(TestReportDescriptors::FidoU2fHid());
  EXPECT_FALSE(device->has_report_id());
  ASSERT_EQ(1u, device->collections().size());
  const auto* collection = device->collections()[0].get();
  EXPECT_TRUE(collection->report_ids.empty());
  ASSERT_EQ(1u, collection->input_reports.size());
  EXPECT_EQ(0u, collection->input_reports[0]->report_id);
  ASSERT_EQ(1u, collection->output_reports.size());
  EXPECT_EQ(0u, collection->output_reports[0]->report_id);
  EXPECT_TRUE(collection->feature_reports.empty());

  // Ensure the correct collection is returned for each report.
  EXPECT_EQ(collection, FindCollectionWithReport(*device->device(), 0,
                                                 HidReportType::kInput));
  EXPECT_EQ(collection, FindCollectionWithReport(*device->device(), 0,
                                                 HidReportType::kOutput));

  // Ensure no collection is found containing a feature report.
  EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), 0,
                                              HidReportType::kFeature));

  // No collections should be found for any non-zero report ID.
  for (uint32_t report_id = 0x01; report_id <= 0xff; ++report_id) {
    EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), report_id,
                                                HidReportType::kInput));
    EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), report_id,
                                                HidReportType::kOutput));
    EXPECT_EQ(nullptr, FindCollectionWithReport(*device->device(), report_id,
                                                HidReportType::kFeature));
  }
}

}  // namespace

}  // namespace device
