// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_device.h"

#include "services/device/public/mojom/hid.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// Construct and return a sample HID report item.
device::mojom::blink::HidReportItemPtr MakeReportItem() {
  auto item = device::mojom::blink::HidReportItem::New();
  item->is_range = false;  // Usages for this item are defined by |usages|.

  // Configure the report item with reasonable values for a button-like input.
  item->is_constant = false;         // Data.
  item->is_variable = true;          // Variable.
  item->is_relative = false;         // Absolute.
  item->wrap = false;                // No wrap.
  item->is_non_linear = false;       // Linear.
  item->no_preferred_state = false;  // Preferred State.
  item->has_null_position = false;   // No Null position.
  item->is_volatile = false;         // Non Volatile.
  item->is_buffered_bytes = false;   // Bit Field.

  // Assign the primary button usage to this item.
  item->usages.push_back(device::mojom::blink::HidUsageAndPage::New(
      0x01, device::mojom::blink::kPageButton));
  // |usage_minimum| and |usage_maximum| are unused.
  item->usage_minimum = device::mojom::blink::HidUsageAndPage::New(0, 0);
  item->usage_maximum = device::mojom::blink::HidUsageAndPage::New(0, 0);

  // Set the designator index and string index extents to zero. This indicates
  // that no physical designators or strings are associated with this item.
  item->designator_minimum = 0;
  item->designator_minimum = 0;
  item->string_minimum = 0;
  item->string_maximum = 0;

  // The report field described by this item can only hold the logical values 0
  // and 1.
  item->logical_minimum = 0;
  item->logical_maximum = 1;
  item->physical_minimum = 0;
  item->physical_maximum = 1;

  // Values reported in this field are unitless.
  item->unit_exponent = 0;
  item->unit = 0;

  // This item defines a single report field, 8 bits wide.
  item->report_size = 8;  // 1 byte.
  item->report_count = 1;

  return item;
}

}  // namespace

TEST(HIDDeviceTest, singleUsageItem) {
  device::mojom::blink::HidReportItemPtr mojo_item = MakeReportItem();
  HIDReportItem* item = HIDDevice::ToHIDReportItem(*mojo_item);

  // Check that all item properties are correctly converted for the sample
  // report item.
  EXPECT_TRUE(item->isAbsolute());
  EXPECT_FALSE(item->isArray());
  EXPECT_FALSE(item->isRange());
  EXPECT_FALSE(item->hasNull());
  EXPECT_EQ(1U, item->usages().size());
  EXPECT_EQ(0x00090001U, item->usages()[0]);
  EXPECT_FALSE(item->hasUsageMinimum());
  EXPECT_FALSE(item->hasUsageMaximum());
  EXPECT_FALSE(item->hasStrings());
  EXPECT_EQ(8U, item->reportSize());
  EXPECT_EQ(1U, item->reportCount());
  EXPECT_EQ(0, item->unitExponent());
  EXPECT_EQ("none", item->unitSystem());
  EXPECT_EQ(0, item->unitFactorLengthExponent());
  EXPECT_EQ(0, item->unitFactorMassExponent());
  EXPECT_EQ(0, item->unitFactorTimeExponent());
  EXPECT_EQ(0, item->unitFactorTemperatureExponent());
  EXPECT_EQ(0, item->unitFactorCurrentExponent());
  EXPECT_EQ(0, item->unitFactorLuminousIntensityExponent());
  EXPECT_EQ(0, item->logicalMinimum());
  EXPECT_EQ(1, item->logicalMaximum());
  EXPECT_EQ(0, item->physicalMinimum());
  EXPECT_EQ(1, item->physicalMaximum());
}

TEST(HIDDeviceTest, multiUsageItem) {
  device::mojom::blink::HidReportItemPtr mojo_item = MakeReportItem();

  // Configure the item to use 8 non-consecutive usages.
  mojo_item->usages.clear();
  for (int i = 1; i < 9; ++i) {
    mojo_item->usages.push_back(device::mojom::blink::HidUsageAndPage::New(
        2 * i, device::mojom::blink::kPageButton));
  }
  mojo_item->report_size = 1;  // 1 bit.
  mojo_item->report_count = 8;
  HIDReportItem* item = HIDDevice::ToHIDReportItem(*mojo_item);

  EXPECT_EQ(8U, item->usages().size());
  EXPECT_EQ(0x00090002U, item->usages()[0]);
  EXPECT_EQ(0x00090004U, item->usages()[1]);
  EXPECT_EQ(0x00090006U, item->usages()[2]);
  EXPECT_EQ(0x00090008U, item->usages()[3]);
  EXPECT_EQ(0x0009000aU, item->usages()[4]);
  EXPECT_EQ(0x0009000cU, item->usages()[5]);
  EXPECT_EQ(0x0009000eU, item->usages()[6]);
  EXPECT_EQ(0x00090010U, item->usages()[7]);
  EXPECT_EQ(1U, item->reportSize());
  EXPECT_EQ(8U, item->reportCount());
}

TEST(HIDDeviceTest, usageRangeItem) {
  device::mojom::blink::HidReportItemPtr mojo_item = MakeReportItem();

  // Configure the item to use a usage range. The item defines eight fields,
  // each 1-bit wide, with consecutive usages from the Button usage page.
  mojo_item->is_range = true;
  mojo_item->usages.clear();
  mojo_item->usage_minimum->usage_page = device::mojom::blink::kPageButton;
  mojo_item->usage_minimum->usage = 0x01;  // 1st button usage (primary).
  mojo_item->usage_maximum->usage_page = device::mojom::blink::kPageButton;
  mojo_item->usage_maximum->usage = 0x08;  // 8th button usage.
  mojo_item->report_size = 1;              // 1 bit.
  mojo_item->report_count = 8;
  HIDReportItem* item = HIDDevice::ToHIDReportItem(*mojo_item);

  EXPECT_FALSE(item->hasStrings());
  EXPECT_FALSE(item->hasUsages());
  EXPECT_EQ(0x00090001U, item->usageMinimum());
  EXPECT_EQ(0x00090008U, item->usageMaximum());
  EXPECT_EQ(1U, item->reportSize());
  EXPECT_EQ(8U, item->reportCount());
}

TEST(HIDDeviceTest, unitDefinition) {
  device::mojom::blink::HidReportItemPtr mojo_item = MakeReportItem();

  // Add a unit definition and check that the unit properties are correctly
  // converted.
  mojo_item->unit_exponent = 0x0C;  // 10^-4
  mojo_item->unit = 0x0000E111;     // g*cm/s^2
  HIDReportItem* item = HIDDevice::ToHIDReportItem(*mojo_item);

  EXPECT_EQ("si-linear", item->unitSystem());
  EXPECT_EQ(-4, item->unitExponent());
  EXPECT_EQ(1, item->unitFactorLengthExponent());
  EXPECT_EQ(1, item->unitFactorMassExponent());
  EXPECT_EQ(-2, item->unitFactorTimeExponent());
  EXPECT_EQ(0, item->unitFactorTemperatureExponent());
  EXPECT_EQ(0, item->unitFactorCurrentExponent());
  EXPECT_EQ(0, item->unitFactorLuminousIntensityExponent());
}

}  // namespace blink
