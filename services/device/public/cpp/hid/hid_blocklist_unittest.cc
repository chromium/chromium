// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/public/cpp/hid/hid_blocklist.h"

#include <string_view>

#include "base/command_line.h"
#include "base/memory/raw_ref.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "services/device/public/cpp/hid/hid_switches.h"
#include "services/device/public/cpp/test/hid_test_util.h"
#include "services/device/public/cpp/test/test_report_descriptors.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::ElementsAre;

constexpr uint16_t kTestVendorId = 0x1234;
constexpr uint16_t kTestProductId = 0x0001;
constexpr uint16_t kTestUsagePage = 0xff00;
constexpr uint16_t kTestUsage = 0x0001;
constexpr uint8_t kNoReportId = 0x00;
constexpr uint8_t kTestReportId = 0x01;

class HidBlocklistTest : public testing::Test {
 public:
  HidBlocklistTest() : blocklist_(HidBlocklist::Get()) {}
  HidBlocklistTest(HidBlocklistTest&) = delete;
  HidBlocklistTest& operator=(HidBlocklistTest&) = delete;

  const HidBlocklist& list() { return *blocklist_; }

  void SetDynamicBlocklist(std::string_view list) {
    feature_list_.Reset();

    std::map<std::string, std::string> params;
    params[kWebHidBlocklistAdditions.name] = std::string(list);
    feature_list_.InitWithFeaturesAndParameters({{kWebHidBlocklist, params}},
                                                /*disabled_features=*/{});

    blocklist_->ResetToDefaultValuesForTest();
  }

  mojom::HidDeviceInfoPtr CreateTestDeviceWithOneReport(
      uint16_t vendor_id,
      uint16_t product_id,
      uint16_t usage_page,
      uint16_t usage,
      uint8_t report_id,
      HidBlocklist::ReportType report_type) {
    const bool has_report_id = (report_id != 0);
    auto report = mojom::HidReportDescription::New();
    report->report_id = report_id;

    auto collection = mojom::HidCollectionInfo::New();
    collection->usage = mojom::HidUsageAndPage::New(usage, usage_page);
    collection->collection_type = mojom::kHIDCollectionTypeApplication;
    if (has_report_id)
      collection->report_ids.push_back(report_id);
    if (report_type == HidBlocklist::kReportTypeInput)
      collection->input_reports.push_back(std::move(report));
    else if (report_type == HidBlocklist::kReportTypeOutput)
      collection->output_reports.push_back(std::move(report));
    else if (report_type == HidBlocklist::kReportTypeFeature)
      collection->feature_reports.push_back(std::move(report));

    auto device = mojom::HidDeviceInfo::New();
    device->guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    device->vendor_id = vendor_id;
    device->product_id = product_id;
    device->has_report_id = has_report_id;
    device->collections.push_back(std::move(collection));
    device->protected_input_report_ids = blocklist_->GetProtectedReportIds(
        HidBlocklist::kReportTypeInput, vendor_id, product_id,
        device->collections);
    device->protected_output_report_ids = blocklist_->GetProtectedReportIds(
        HidBlocklist::kReportTypeOutput, vendor_id, product_id,
        device->collections);
    device->protected_feature_report_ids = blocklist_->GetProtectedReportIds(
        HidBlocklist::kReportTypeFeature, vendor_id, product_id,
        device->collections);
    device->is_excluded_by_blocklist =
        blocklist_->IsVendorProductBlocked(vendor_id, product_id);
    return device;
  }

  // Returns a mojom::HidDeviceInfoPtr with |num_collections| collections, each
  // with different usage pages and report IDs. Each collection contains one
  // report of each type (input/output/feature).
  mojom::HidDeviceInfoPtr CreateTestDeviceWithMultipleCollections(
      uint16_t vendor_id,
      uint16_t product_id,
      size_t num_collections) {
    std::vector<mojom::HidCollectionInfoPtr> collections;
    uint16_t usage_page = kTestUsagePage;
    uint8_t report_id = kTestReportId;
    for (size_t i = 0; i < num_collections; ++i) {
      auto collection = mojom::HidCollectionInfo::New();
      collection->usage = mojom::HidUsageAndPage::New(kTestUsage, usage_page++);
      collection->collection_type = mojom::kHIDCollectionTypeApplication;

      auto input_report = mojom::HidReportDescription::New();
      collection->report_ids.push_back(report_id);
      input_report->report_id = report_id++;
      collection->input_reports.push_back(std::move(input_report));

      auto output_report = mojom::HidReportDescription::New();
      collection->report_ids.push_back(report_id);
      output_report->report_id = report_id++;
      collection->output_reports.push_back(std::move(output_report));

      auto feature_report = mojom::HidReportDescription::New();
      collection->report_ids.push_back(report_id);
      feature_report->report_id = report_id++;
      collection->feature_reports.push_back(std::move(feature_report));

      collections.push_back(std::move(collection));
    }

    auto device = mojom::HidDeviceInfo::New();
    device->guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    device->vendor_id = vendor_id;
    device->product_id = product_id;
    device->has_report_id = true;
    device->collections = std::move(collections);
    device->protected_input_report_ids = blocklist_->GetProtectedReportIds(
        HidBlocklist::kReportTypeInput, vendor_id, product_id,
        device->collections);
    device->protected_output_report_ids = blocklist_->GetProtectedReportIds(
        HidBlocklist::kReportTypeOutput, vendor_id, product_id,
        device->collections);
    device->protected_feature_report_ids = blocklist_->GetProtectedReportIds(
        HidBlocklist::kReportTypeFeature, vendor_id, product_id,
        device->collections);
    device->is_excluded_by_blocklist =
        blocklist_->IsVendorProductBlocked(vendor_id, product_id);
    return device;
  }

 private:
  void TearDown() override {
    // Because HidBlocklist is a singleton it must be cleared after tests run
    // to prevent leakage between tests.
    feature_list_.Reset();
    blocklist_->ResetToDefaultValuesForTest();
  }

  base::test::ScopedFeatureList feature_list_;
  const raw_ref<HidBlocklist> blocklist_;
};

}  // namespace

TEST_F(HidBlocklistTest, StringsWithNoValidEntries) {
  SetDynamicBlocklist("");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("~!@#$%^&*()-_=+[]{}/*-");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(":");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(",");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(",,");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("1:2:3:4:5:I");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("18d1:2:ff00:::");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("0x18d1:0x0000::::");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("0000:   0::::");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("000g:0000::::");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("::::255:I");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("::::0xff:I");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(":::::i");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(":::::o");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(":::::f");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist(":::::A");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());

  SetDynamicBlocklist("☯");
  EXPECT_EQ(0u, list().GetDynamicEntryCountForTest());
}

TEST_F(HidBlocklistTest, UnexcludedDevice) {
  auto device = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device->is_excluded_by_blocklist);
  EXPECT_TRUE(device->protected_input_report_ids->empty());
  EXPECT_TRUE(device->protected_output_report_ids->empty());
  EXPECT_TRUE(device->protected_feature_report_ids->empty());
}

TEST_F(HidBlocklistTest, UnexcludedDeviceWithNoCollections) {
  auto device = mojom::HidDeviceInfo::New();
  device->guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  device->vendor_id = kTestVendorId;
  device->product_id = kTestProductId;
  EXPECT_FALSE(device->is_excluded_by_blocklist);
}

TEST_F(HidBlocklistTest, VendorRule) {
  // Exclude all devices with matching vendor ID.
  SetDynamicBlocklist("1234:::::");

  // A device with matching vendor IDs is excluded.
  auto device1 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_TRUE(device1->is_excluded_by_blocklist);
  EXPECT_THAT(*device1->protected_input_report_ids, ElementsAre(kTestReportId));

  // A device with a different vendor ID is not excluded.
  auto device2 = CreateTestDeviceWithOneReport(
      kTestVendorId + 1, kTestProductId, kTestUsagePage, kTestUsage,
      kTestReportId, HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device2->is_excluded_by_blocklist);
  EXPECT_TRUE(device2->protected_input_report_ids->empty());
}

TEST_F(HidBlocklistTest, VendorProductRule) {
  // Exclude devices with matching vendor and product ID.
  SetDynamicBlocklist("1234:0001::::");

  // A device with matching vendor/product IDs is excluded.
  auto device1 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_TRUE(device1->is_excluded_by_blocklist);
  EXPECT_THAT(*device1->protected_input_report_ids, ElementsAre(kTestReportId));

  // A device with matching vendor ID but different product ID is not excluded.
  auto device2 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId + 1, kTestUsagePage, kTestUsage,
      kTestReportId, HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device2->is_excluded_by_blocklist);
  EXPECT_TRUE(device2->protected_input_report_ids->empty());
}

TEST_F(HidBlocklistTest, ExcludedDeviceAllowedWithFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableHidBlocklist);

  // Exclude devices with matching vendor and product ID.
  SetDynamicBlocklist("1234:0001::::");

  // A device with matching vendor/product IDs is not excluded because the
  // blocklist is disabled.
  auto device = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device->is_excluded_by_blocklist);
  EXPECT_TRUE(device->protected_input_report_ids->empty());
  EXPECT_TRUE(device->protected_output_report_ids->empty());
  EXPECT_TRUE(device->protected_feature_report_ids->empty());
}

TEST_F(HidBlocklistTest, ProductRuleWithoutVendorDoesNothing) {
  // Add an invalid rule with a product ID but no vendor ID.
  SetDynamicBlocklist(":0001::::");

  // A device with matching product ID is not excluded.
  auto device = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device->is_excluded_by_blocklist);
  EXPECT_TRUE(device->protected_input_report_ids->empty());
}

TEST_F(HidBlocklistTest, UsagePageRule) {
  // Protect reports by the usage page of the top-level collection.
  SetDynamicBlocklist("::ff00:::");

  // A device with matching usage page is not excluded.
  auto device1 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device1->is_excluded_by_blocklist);
  EXPECT_THAT(*device1->protected_input_report_ids, ElementsAre(kTestReportId));

  // A device with a different usage page is not excluded.
  auto device2 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage + 1, kTestUsage,
      kTestReportId, HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device2->is_excluded_by_blocklist);
  EXPECT_TRUE(device2->protected_input_report_ids->empty());
}

TEST_F(HidBlocklistTest, UsagePageAndUsageRule) {
  // Protect reports by the usage page and usage ID of the top-level collection.
  SetDynamicBlocklist("::ff00:0001::");

  // A device with matching usage page is not excluded.
  auto device1 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device1->is_excluded_by_blocklist);
  EXPECT_THAT(*device1->protected_input_report_ids, ElementsAre(kTestReportId));

  // A device with a different usage page is not excluded.
  auto device2 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage + 1,
      kTestReportId, HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device2->is_excluded_by_blocklist);
  EXPECT_TRUE(device2->protected_input_report_ids->empty());
}

TEST_F(HidBlocklistTest, UsageRuleWithoutUsagePageDoesNothing) {
  // Add an invalid rule with a usage ID but no usage page.
  SetDynamicBlocklist(":::0001::");

  // A device with matching usage ID is not excluded.
  auto device = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device->is_excluded_by_blocklist);
  EXPECT_TRUE(device->protected_input_report_ids->empty());
}

TEST_F(HidBlocklistTest, NonZeroReportIdRule) {
  // Protect reports by report ID.
  SetDynamicBlocklist("::::01:");

  // A device with matching report ID is not excluded.
  auto device1 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device1->is_excluded_by_blocklist);
  EXPECT_THAT(*device1->protected_input_report_ids, ElementsAre(kTestReportId));

  // A device with a different report ID is not excluded.
  auto device2 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage,
      kTestReportId + 1, HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device2->is_excluded_by_blocklist);
  EXPECT_TRUE(device2->protected_input_report_ids->empty());
}

TEST_F(HidBlocklistTest, ZeroReportIdRule) {
  // Protect reports from devices that do not use report IDs.
  SetDynamicBlocklist("::::00:");

  // A device that does not use report IDs is not excluded.
  auto device1 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kNoReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device1->is_excluded_by_blocklist);
  EXPECT_THAT(*device1->protected_input_report_ids, ElementsAre(kNoReportId));

  // A device that uses report IDs is not excluded.
  auto device2 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device2->is_excluded_by_blocklist);
  EXPECT_TRUE(device2->protected_input_report_ids->empty());
}

TEST_F(HidBlocklistTest, ReportTypeRule) {
  // Protect reports by report type.
  SetDynamicBlocklist(":::::I");

  // A device with only an input report is not excluded.
  auto device1 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeInput);
  EXPECT_FALSE(device1->is_excluded_by_blocklist);
  EXPECT_THAT(*device1->protected_input_report_ids, ElementsAre(kTestReportId));

  // A device with an output report is not excluded.
  auto device2 = CreateTestDeviceWithOneReport(
      kTestVendorId, kTestProductId, kTestUsagePage, kTestUsage, kTestReportId,
      HidBlocklist::kReportTypeOutput);
  EXPECT_FALSE(device2->is_excluded_by_blocklist);
  EXPECT_TRUE(device2->protected_output_report_ids->empty());
}

TEST_F(HidBlocklistTest, DeviceWithAnyUnprotectedReportsNotExcluded) {
  // Protect input report 0x01.
  SetDynamicBlocklist("::::01:I");

  // Create a device with six reports divided into two collections. One of the
  // reports matches the blocklist entry and should be protected, but the other
  // reports should not be protected.
  auto device =
      CreateTestDeviceWithMultipleCollections(kTestVendorId, kTestProductId, 2);
  EXPECT_FALSE(device->is_excluded_by_blocklist);
  EXPECT_THAT(*device->protected_input_report_ids, ElementsAre(0x01));
  EXPECT_TRUE(device->protected_output_report_ids->empty());
  EXPECT_TRUE(device->protected_feature_report_ids->empty());
}

TEST_F(HidBlocklistTest, DeviceWithAllProtectedReportsIsNotExcluded) {
  // Protect six reports by report ID and report type.
  SetDynamicBlocklist(
      "::::01:I, ::::02:O, ::::03:F, ::::04:I, ::::05:O, ::::06:F");

  // Create a device with six reports divided into two collections. All of the
  // reports match the above blocklist rules and should be protected.
  auto device =
      CreateTestDeviceWithMultipleCollections(kTestVendorId, kTestProductId, 2);
  EXPECT_FALSE(device->is_excluded_by_blocklist);
  EXPECT_THAT(*device->protected_input_report_ids, ElementsAre(0x01, 0x04));
  EXPECT_THAT(*device->protected_output_report_ids, ElementsAre(0x02, 0x05));
  EXPECT_THAT(*device->protected_feature_report_ids, ElementsAre(0x03, 0x06));
}

TEST_F(HidBlocklistTest, SpecificOutputReportIsProtected) {
  // Block report 0x05 from usage page 0xff00 on devices from vendor 0x0b0e.
  SetDynamicBlocklist("0b0e::ff00::05:O");

  // Create a device with several reports, one of which matches the blocklist
  // rule.
  auto device = CreateDeviceFromReportDescriptor(
      /*vendor_id=*/0x0b0e, /*product_id=*/0x24c9,
      TestReportDescriptors::JabraLink380c());

  // Check that only the blocked report is excluded.
  EXPECT_FALSE(device->is_excluded_by_blocklist);
  EXPECT_TRUE(device->protected_input_report_ids->empty());
  EXPECT_THAT(*device->protected_output_report_ids, ElementsAre(0x05));
  EXPECT_TRUE(device->protected_feature_report_ids->empty());
}

}  // namespace device
