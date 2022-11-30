// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/hid/hid_service_win.h"

#include <vector>

#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::ElementsAre;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

using ReportItem = HidServiceWin::PreparsedData::ReportItem;

// Report IDs.
constexpr uint8_t kNoReportId = 0x00;
constexpr uint8_t kReportId01 = 0x01;
constexpr uint8_t kReportId02 = 0x02;

// HID usage page constants.
constexpr uint16_t kPageButton = mojom::kPageButton;
constexpr uint16_t kPageGenericDesktop = mojom::kPageGenericDesktop;

// HID usage constants.
constexpr uint16_t kUsageMouse = mojom::kGenericDesktopMouse;
constexpr uint16_t kUsageX = mojom::kGenericDesktopX;
constexpr uint16_t kUsageY = mojom::kGenericDesktopY;
constexpr uint16_t kUsage00 = 0x00;
constexpr uint16_t kUsage01 = 0x01;
constexpr uint16_t kUsage02 = 0x02;
constexpr uint16_t kUsage03 = 0x03;
constexpr uint16_t kUsage04 = 0x04;
constexpr uint16_t kUsage05 = 0x05;
constexpr uint16_t kUsage06 = 0x06;
constexpr uint16_t kUsage07 = 0x07;
constexpr uint16_t kUsage08 = 0x08;
constexpr uint16_t kUsageFF = 0xff;

// Data, Array, Abs, No Wrap, Linear, Preferred State, No Null Position.
constexpr uint16_t kBitFieldArray = 0x0000;

// Data, Var, Abs, No Wrap, Linear, Preferred State, No Null Position.
constexpr uint16_t kBitFieldVariable = 0x0002;

class MockPreparsedData : public NiceMock<HidServiceWin::PreparsedData> {
 public:
  MockPreparsedData() {
    ON_CALL(*this, GetReportItems)
        .WillByDefault(Return(std::vector<ReportItem>()));
  }
  ~MockPreparsedData() override = default;

  MOCK_CONST_METHOD0(GetCaps, const HIDP_CAPS&());
  MOCK_CONST_METHOD1(GetReportItems, std::vector<ReportItem>(HIDP_REPORT_TYPE));
};

ReportItem SimpleButtonItem(uint16_t usage_page,
                            uint16_t usage,
                            uint8_t report_id,
                            size_t bit_index) {
  return {report_id,
          kBitFieldVariable,
          /*report_size=*/1,
          /*report_count=*/1,
          usage_page,
          /*usage_min=*/usage,
          /*usage_max=*/usage,
          /*designator_minimum=*/0,
          /*designator_maximum=*/0,
          /*string_minimum=*/0,
          /*string_maximum=*/0,
          /*logical_minimum=*/0,
          /*logical_maximum=*/1,
          /*physical_minimum=*/0,
          /*physical_maximum=*/0,
          /*unit=*/0,
          /*unit_exponent=*/0,
          bit_index};
}

ReportItem RangeButtonItem(uint16_t usage_page,
                           uint16_t usage_min,
                           uint16_t usage_max,
                           uint8_t report_id,
                           size_t bit_index) {
  uint16_t report_count = usage_max - usage_min + 1;
  return {report_id,
          kBitFieldVariable,
          /*report_size=*/1,
          report_count,
          usage_page,
          usage_min,
          usage_max,
          /*designator_minimum=*/0,
          /*designator_maximum=*/0,
          /*string_minimum=*/0,
          /*string_maximum=*/0,
          /*logical_minimum=*/0,
          /*logical_maximum=*/1,
          /*physical_minimum=*/0,
          /*physical_maximum=*/0,
          /*unit=*/0,
          /*unit_exponent=*/0,
          bit_index};
}

ReportItem ArrayItem(uint16_t usage_page,
                     uint16_t usage_min,
                     uint16_t usage_max,
                     uint8_t report_id,
                     uint16_t report_count,
                     size_t bit_index) {
  return {report_id,
          kBitFieldArray,
          /*report_size=*/8,
          report_count,
          usage_page,
          usage_min,
          usage_max,
          /*designator_minimum=*/0,
          /*designator_maximum=*/0,
          /*string_minimum=*/0,
          /*string_maximum=*/0,
          /*logical_minimum=*/usage_min,
          /*logical_maximum=*/usage_max,
          /*physical_minimum=*/0,
          /*physical_maximum=*/0,
          /*unit=*/0,
          /*unit_exponent=*/0,
          bit_index};
}

ReportItem SimpleValueItem(uint16_t usage_page,
                           uint16_t usage,
                           uint8_t report_id,
                           size_t bit_index) {
  return {report_id,
          kBitFieldVariable,
          /*report_size=*/8,
          /*report_count=*/1,
          usage_page,
          /*usage_min=*/usage,
          /*usage_max=*/usage,
          /*designator_minimum=*/0,
          /*designator_maximum=*/0,
          /*string_minimum=*/0,
          /*string_maximum=*/0,
          /*logical_minimum=*/0x00,
          /*logical_maximum=*/0xff,
          /*physical_minimum=*/0,
          /*physical_maximum=*/0,
          /*unit=*/0,
          /*unit_exponent=*/0,
          bit_index};
}

ReportItem RangeValueItem(uint16_t usage_page,
                          uint16_t usage_min,
                          uint16_t usage_max,
                          uint16_t report_count,
                          uint8_t report_id,
                          size_t bit_index) {
  return {report_id,
          kBitFieldVariable,
          /*report_size=*/8,
          report_count,
          usage_page,
          usage_min,
          usage_max,
          /*designator_minimum=*/0,
          /*designator_maximum=*/0,
          /*string_minimum=*/0,
          /*string_maximum=*/0,
          /*logical_minimum=*/0x00,
          /*logical_maximum=*/0xff,
          /*physical_minimum=*/0,
          /*physical_maximum=*/0,
          /*unit=*/0,
          /*unit_exponent=*/0,
          bit_index};
}

}  // namespace

TEST(HidPreparsedDataTest, NoReportItems) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_TRUE(collection->report_ids.empty());
  EXPECT_TRUE(collection->input_reports.empty());
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, OneButtonItemWithNoReportId) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 2;
  capabilities.NumberInputButtonCaps = 1;
  std::vector<ReportItem> input_items = {
      SimpleButtonItem(kPageButton, kUsage01, kNoReportId, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_TRUE(collection->report_ids.empty());
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kNoReportId);
  ASSERT_EQ(report->items.size(), 2U);
  EXPECT_FALSE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_TRUE(report->items[0]->is_variable);
  ASSERT_EQ(report->items[0]->usages.size(), 1U);
  EXPECT_EQ(report->items[0]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usages[0]->usage, kUsage01);
  EXPECT_EQ(report->items[0]->report_size, 1U);
  EXPECT_EQ(report->items[0]->report_count, 1U);
  EXPECT_TRUE(report->items[1]->is_constant);
  EXPECT_EQ(report->items[1]->report_size, 7U);
  EXPECT_EQ(report->items[1]->report_count, 1U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, OneButtonItemWithReportId) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 2;
  capabilities.NumberInputButtonCaps = 1;
  std::vector<ReportItem> input_items = {
      SimpleButtonItem(kPageButton, kUsage01, kReportId01, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 2U);
  EXPECT_FALSE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_TRUE(report->items[0]->is_variable);
  ASSERT_EQ(report->items[0]->usages.size(), 1U);
  EXPECT_EQ(report->items[0]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usages[0]->usage, kUsage01);
  EXPECT_EQ(report->items[0]->report_size, 1U);
  EXPECT_EQ(report->items[0]->report_count, 1U);
  EXPECT_TRUE(report->items[1]->is_constant);
  EXPECT_EQ(report->items[1]->report_size, 7U);
  EXPECT_EQ(report->items[1]->report_count, 1U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ButtonItemWithUsageRange) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 2;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {RangeButtonItem(
      kPageButton, kUsage01, kUsage08, kReportId01, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 1U);
  EXPECT_TRUE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_TRUE(report->items[0]->is_variable);
  EXPECT_EQ(report->items[0]->usage_minimum->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usage_minimum->usage, kUsage01);
  EXPECT_EQ(report->items[0]->usage_maximum->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usage_maximum->usage, kUsage08);
  EXPECT_EQ(report->items[0]->report_size, 1U);
  EXPECT_EQ(report->items[0]->report_count, 8U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ArrayItemWithReportCount1) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 2;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {
      ArrayItem(kPageButton, kUsage00, kUsageFF, kReportId01,
                /*report_count=*/1, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 1U);
  EXPECT_TRUE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_FALSE(report->items[0]->is_variable);
  EXPECT_EQ(report->items[0]->usage_minimum->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usage_minimum->usage, kUsage00);
  EXPECT_EQ(report->items[0]->usage_maximum->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usage_maximum->usage, kUsageFF);
  EXPECT_EQ(report->items[0]->report_size, 8U);
  EXPECT_EQ(report->items[0]->report_count, 1U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ArrayItemWithReportCount2) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 3;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {
      ArrayItem(kPageButton, kUsage00, kUsageFF, kReportId01,
                /*report_count=*/2, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 1U);
  EXPECT_TRUE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_FALSE(report->items[0]->is_variable);
  EXPECT_EQ(report->items[0]->usage_minimum->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usage_minimum->usage, kUsage00);
  EXPECT_EQ(report->items[0]->usage_maximum->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usage_maximum->usage, kUsageFF);
  EXPECT_EQ(report->items[0]->report_size, 8U);
  EXPECT_EQ(report->items[0]->report_count, 2U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ArrayItemWithNoValidUsages) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 2;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {
      ArrayItem(kPageButton, kUsage00, kUsage00, kReportId01,
                /*report_count=*/1, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  EXPECT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 1U);
  EXPECT_FALSE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_FALSE(report->items[0]->is_variable);
  ASSERT_EQ(report->items[0]->usages.size(), 1U);
  EXPECT_EQ(report->items[0]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usages[0]->usage, kUsage00);
  EXPECT_EQ(report->items[0]->report_size, 8U);
  EXPECT_EQ(report->items[0]->report_count, 1U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ValueItemWithNoReportId) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 2;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {SimpleValueItem(
      kPageGenericDesktop, kUsageX, kNoReportId, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_TRUE(collection->report_ids.empty());
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kNoReportId);
  ASSERT_EQ(report->items.size(), 1U);
  EXPECT_FALSE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_TRUE(report->items[0]->is_variable);
  ASSERT_EQ(report->items[0]->usages.size(), 1U);
  EXPECT_EQ(report->items[0]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report->items[0]->usages[0]->usage, kUsageX);
  EXPECT_EQ(report->items[0]->report_size, 8U);
  EXPECT_EQ(report->items[0]->report_count, 1U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ValueItemWithReportId) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 2;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {SimpleValueItem(
      kPageGenericDesktop, kUsageX, kReportId01, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 1U);
  EXPECT_FALSE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_TRUE(report->items[0]->is_variable);
  ASSERT_EQ(report->items[0]->usages.size(), 1U);
  EXPECT_EQ(report->items[0]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report->items[0]->usages[0]->usage, kUsageX);
  EXPECT_EQ(report->items[0]->report_size, 8U);
  EXPECT_EQ(report->items[0]->report_count, 1U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, TwoValueItemsWithMatchingReportIds) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 3;
  capabilities.NumberInputValueCaps = 2;
  std::vector<ReportItem> input_items = {
      SimpleValueItem(kPageGenericDesktop, kUsageX, kReportId01,
                      /*bit_index=*/0),
      SimpleValueItem(kPageGenericDesktop, kUsageY, kReportId01,
                      /*bit_index=*/8),
  };

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 2U);
  EXPECT_FALSE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_TRUE(report->items[0]->is_variable);
  ASSERT_EQ(report->items[0]->usages.size(), 1U);
  EXPECT_EQ(report->items[0]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report->items[0]->usages[0]->usage, kUsageX);
  EXPECT_EQ(report->items[0]->report_size, 8U);
  EXPECT_EQ(report->items[0]->report_count, 1U);
  EXPECT_FALSE(report->items[1]->is_range);
  EXPECT_FALSE(report->items[1]->is_constant);
  EXPECT_TRUE(report->items[1]->is_variable);
  ASSERT_EQ(report->items[1]->usages.size(), 1U);
  EXPECT_EQ(report->items[1]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report->items[1]->usages[0]->usage, kUsageY);
  EXPECT_EQ(report->items[1]->report_size, 8U);
  EXPECT_EQ(report->items[1]->report_count, 1U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, TwoValueItemsWithDifferentReportIds) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 2;
  capabilities.NumberInputValueCaps = 2;
  std::vector<ReportItem> input_items = {
      SimpleValueItem(kPageGenericDesktop, kUsageX, kReportId01,
                      /*bit_index=*/0),
      SimpleValueItem(kPageGenericDesktop, kUsageY, kReportId02,
                      /*bit_index=*/0),
  };

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01, kReportId02));
  ASSERT_EQ(collection->input_reports.size(), 2U);
  const auto& report01 = collection->input_reports[0];
  EXPECT_EQ(report01->report_id, kReportId01);
  ASSERT_EQ(report01->items.size(), 1U);
  EXPECT_FALSE(report01->items[0]->is_range);
  EXPECT_FALSE(report01->items[0]->is_constant);
  EXPECT_TRUE(report01->items[0]->is_variable);
  ASSERT_EQ(report01->items[0]->usages.size(), 1U);
  EXPECT_EQ(report01->items[0]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report01->items[0]->usages[0]->usage, kUsageX);
  EXPECT_EQ(report01->items[0]->report_size, 8U);
  EXPECT_EQ(report01->items[0]->report_count, 1U);
  const auto& report02 = collection->input_reports[1];
  EXPECT_EQ(report02->report_id, kReportId02);
  ASSERT_EQ(report02->items.size(), 1U);
  EXPECT_FALSE(report02->items[0]->is_range);
  EXPECT_FALSE(report02->items[0]->is_constant);
  EXPECT_TRUE(report02->items[0]->is_variable);
  ASSERT_EQ(report02->items[0]->usages.size(), 1U);
  EXPECT_EQ(report02->items[0]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report02->items[0]->usages[0]->usage, kUsageY);
  EXPECT_EQ(report02->items[0]->report_size, 8U);
  EXPECT_EQ(report02->items[0]->report_count, 1U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, TwoValueItemsWithDifferentReportTypes) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 2;
  capabilities.OutputReportByteLength = 2;
  capabilities.NumberInputValueCaps = 1;
  capabilities.NumberOutputValueCaps = 1;
  std::vector<ReportItem> input_items = {SimpleValueItem(
      kPageGenericDesktop, kUsageX, kReportId01, /*bit_index=*/0)};
  std::vector<ReportItem> output_items = {SimpleValueItem(
      kPageGenericDesktop, kUsageY, kReportId01, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(output_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& in_report = collection->input_reports[0];
  EXPECT_EQ(in_report->report_id, kReportId01);
  ASSERT_EQ(in_report->items.size(), 1U);
  EXPECT_FALSE(in_report->items[0]->is_range);
  EXPECT_FALSE(in_report->items[0]->is_constant);
  EXPECT_TRUE(in_report->items[0]->is_variable);
  ASSERT_EQ(in_report->items[0]->usages.size(), 1U);
  EXPECT_EQ(in_report->items[0]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(in_report->items[0]->usages[0]->usage, kUsageX);
  EXPECT_EQ(in_report->items[0]->report_size, 8U);
  EXPECT_EQ(in_report->items[0]->report_count, 1U);
  ASSERT_EQ(collection->output_reports.size(), 1U);
  const auto& out_report = collection->output_reports[0];
  EXPECT_EQ(out_report->report_id, kReportId01);
  ASSERT_EQ(out_report->items.size(), 1U);
  EXPECT_FALSE(out_report->items[0]->is_range);
  EXPECT_FALSE(out_report->items[0]->is_constant);
  EXPECT_TRUE(out_report->items[0]->is_variable);
  ASSERT_EQ(out_report->items[0]->usages.size(), 1U);
  EXPECT_EQ(out_report->items[0]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(out_report->items[0]->usages[0]->usage, kUsageY);
  EXPECT_EQ(out_report->items[0]->report_size, 8U);
  EXPECT_EQ(out_report->items[0]->report_count, 1U);
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ValueItemWithUsageRange) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 3;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {
      RangeValueItem(kPageGenericDesktop, kUsageX, kUsageY, /*report_count=*/2,
                     kReportId01, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 1U);
  EXPECT_TRUE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_TRUE(report->items[0]->is_variable);
  EXPECT_EQ(report->items[0]->usage_minimum->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report->items[0]->usage_minimum->usage, kUsageX);
  EXPECT_EQ(report->items[0]->usage_maximum->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report->items[0]->usage_maximum->usage, kUsageY);
  EXPECT_EQ(report->items[0]->report_size, 8U);
  EXPECT_EQ(report->items[0]->report_count, 2U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ValueItemWithUsageRangeAndRepeatedUsageValue) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 3;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {
      RangeValueItem(kPageGenericDesktop, kUsageX, kUsageX, /*report_count=*/2,
                     kReportId01, /*bit_index=*/0)};

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 1U);
  EXPECT_FALSE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_TRUE(report->items[0]->is_variable);
  ASSERT_EQ(report->items[0]->usages.size(), 1U);
  EXPECT_EQ(report->items[0]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report->items[0]->usages[0]->usage, kUsageX);
  EXPECT_EQ(report->items[0]->report_size, 8U);
  EXPECT_EQ(report->items[0]->report_count, 2U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ButtonAndValueItemsInSameReport) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 3;
  capabilities.NumberInputButtonCaps = 1;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {
      RangeButtonItem(kPageButton, kUsage01, kUsage08, kReportId01,
                      /*bit_index=*/0),
      SimpleValueItem(kPageGenericDesktop, kUsageX, kReportId01,
                      /*bit_index=*/8),
  };

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 2U);
  EXPECT_TRUE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_TRUE(report->items[0]->is_variable);
  EXPECT_EQ(report->items[0]->usage_minimum->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usage_minimum->usage, kUsage01);
  EXPECT_EQ(report->items[0]->usage_maximum->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usage_maximum->usage, kUsage08);
  EXPECT_EQ(report->items[0]->report_size, 1U);
  EXPECT_EQ(report->items[0]->report_count, 8U);
  EXPECT_FALSE(report->items[1]->is_range);
  EXPECT_FALSE(report->items[1]->is_constant);
  EXPECT_TRUE(report->items[1]->is_variable);
  ASSERT_EQ(report->items[1]->usages.size(), 1U);
  EXPECT_EQ(report->items[1]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report->items[1]->usages[0]->usage, kUsageX);
  EXPECT_EQ(report->items[1]->report_size, 8U);
  EXPECT_EQ(report->items[1]->report_count, 1U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ButtonAndValueItemsInSameReportWithGap) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 3;
  capabilities.NumberInputButtonCaps = 1;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {
      SimpleButtonItem(kPageButton, kUsage01, kReportId01, /*bit_index=*/0),
      SimpleValueItem(kPageGenericDesktop, kUsageX, kReportId01,
                      /*bit_index=*/8),
  };

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_TRUE(collection->usage);
  EXPECT_EQ(collection->usage->usage_page, kPageGenericDesktop);
  EXPECT_EQ(collection->usage->usage, kUsageMouse);
  EXPECT_THAT(collection->report_ids, ElementsAre(kReportId01));
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 3U);
  EXPECT_FALSE(report->items[0]->is_range);
  EXPECT_FALSE(report->items[0]->is_constant);
  EXPECT_TRUE(report->items[0]->is_variable);
  ASSERT_EQ(report->items[0]->usages.size(), 1U);
  EXPECT_EQ(report->items[0]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usages[0]->usage, kUsage01);
  EXPECT_EQ(report->items[0]->report_size, 1U);
  EXPECT_EQ(report->items[0]->report_count, 1U);
  EXPECT_TRUE(report->items[1]->is_constant);
  EXPECT_EQ(report->items[1]->report_size, 7U);
  EXPECT_EQ(report->items[1]->report_count, 1U);
  EXPECT_FALSE(report->items[2]->is_range);
  EXPECT_FALSE(report->items[2]->is_constant);
  EXPECT_TRUE(report->items[2]->is_variable);
  ASSERT_EQ(report->items[2]->usages.size(), 1U);
  EXPECT_EQ(report->items[2]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report->items[2]->usages[0]->usage, kUsageX);
  EXPECT_EQ(report->items[2]->report_size, 8U);
  EXPECT_EQ(report->items[2]->report_count, 1U);
  EXPECT_TRUE(collection->output_reports.empty());
  EXPECT_TRUE(collection->feature_reports.empty());
  EXPECT_TRUE(collection->children.empty());
}

TEST(HidPreparsedDataTest, ButtonAndValueItemsInWrongOrderAndOffByteAlignment) {
  HIDP_CAPS capabilities = {0};
  capabilities.UsagePage = kPageGenericDesktop;
  capabilities.Usage = kUsageMouse;
  capabilities.InputReportByteLength = 3;
  capabilities.NumberInputButtonCaps = 1;
  capabilities.NumberInputValueCaps = 1;
  std::vector<ReportItem> input_items = {
      SimpleButtonItem(kPageButton, kUsage01, kReportId01, /*bit_index=*/15),
      SimpleButtonItem(kPageButton, kUsage02, kReportId01, /*bit_index=*/14),
      SimpleButtonItem(kPageButton, kUsage03, kReportId01, /*bit_index=*/13),
      SimpleButtonItem(kPageButton, kUsage04, kReportId01, /*bit_index=*/12),
      SimpleButtonItem(kPageButton, kUsage05, kReportId01, /*bit_index=*/3),
      SimpleButtonItem(kPageButton, kUsage06, kReportId01, /*bit_index=*/2),
      SimpleButtonItem(kPageButton, kUsage07, kReportId01, /*bit_index=*/1),
      SimpleButtonItem(kPageButton, kUsage08, kReportId01, /*bit_index=*/0),
      SimpleValueItem(kPageGenericDesktop, kUsageX, kReportId01,
                      /*bit_index=*/4),
  };

  MockPreparsedData preparsed_data;
  ON_CALL(preparsed_data, GetCaps).WillByDefault(ReturnRef(capabilities));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Input))
      .WillOnce(Return(input_items));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Output))
      .WillOnce(Return(std::vector<ReportItem>()));
  EXPECT_CALL(preparsed_data, GetReportItems(HidP_Feature))
      .WillOnce(Return(std::vector<ReportItem>()));

  const auto collection = preparsed_data.CreateHidCollectionInfo();
  ASSERT_EQ(collection->input_reports.size(), 1U);
  const auto& report = collection->input_reports[0];
  EXPECT_EQ(report->report_id, kReportId01);
  ASSERT_EQ(report->items.size(), 9U);
  ASSERT_EQ(report->items[0]->usages.size(), 1U);
  EXPECT_EQ(report->items[0]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[0]->usages[0]->usage, kUsage08);
  EXPECT_EQ(report->items[0]->report_size, 1U);
  EXPECT_EQ(report->items[0]->report_count, 1U);
  ASSERT_EQ(report->items[1]->usages.size(), 1U);
  EXPECT_EQ(report->items[1]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[1]->usages[0]->usage, kUsage07);
  EXPECT_EQ(report->items[1]->report_size, 1U);
  EXPECT_EQ(report->items[1]->report_count, 1U);
  ASSERT_EQ(report->items[2]->usages.size(), 1U);
  EXPECT_EQ(report->items[2]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[2]->usages[0]->usage, kUsage06);
  EXPECT_EQ(report->items[2]->report_size, 1U);
  EXPECT_EQ(report->items[2]->report_count, 1U);
  ASSERT_EQ(report->items[3]->usages.size(), 1U);
  EXPECT_EQ(report->items[3]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[3]->usages[0]->usage, kUsage05);
  EXPECT_EQ(report->items[3]->report_size, 1U);
  EXPECT_EQ(report->items[3]->report_count, 1U);
  ASSERT_EQ(report->items[4]->usages.size(), 1U);
  EXPECT_EQ(report->items[4]->usages[0]->usage_page, kPageGenericDesktop);
  EXPECT_EQ(report->items[4]->usages[0]->usage, kUsageX);
  EXPECT_EQ(report->items[4]->report_size, 8U);
  EXPECT_EQ(report->items[4]->report_count, 1U);
  ASSERT_EQ(report->items[5]->usages.size(), 1U);
  EXPECT_EQ(report->items[5]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[5]->usages[0]->usage, kUsage04);
  EXPECT_EQ(report->items[5]->report_size, 1U);
  EXPECT_EQ(report->items[5]->report_count, 1U);
  ASSERT_EQ(report->items[6]->usages.size(), 1U);
  EXPECT_EQ(report->items[6]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[6]->usages[0]->usage, kUsage03);
  EXPECT_EQ(report->items[6]->report_size, 1U);
  EXPECT_EQ(report->items[6]->report_count, 1U);
  ASSERT_EQ(report->items[7]->usages.size(), 1U);
  EXPECT_EQ(report->items[7]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[7]->usages[0]->usage, kUsage02);
  EXPECT_EQ(report->items[7]->report_size, 1U);
  EXPECT_EQ(report->items[7]->report_count, 1U);
  ASSERT_EQ(report->items[8]->usages.size(), 1U);
  EXPECT_EQ(report->items[8]->usages[0]->usage_page, kPageButton);
  EXPECT_EQ(report->items[8]->usages[0]->usage, kUsage01);
  EXPECT_EQ(report->items[8]->report_size, 1U);
  EXPECT_EQ(report->items[8]->report_count, 1U);
}

}  // namespace device
