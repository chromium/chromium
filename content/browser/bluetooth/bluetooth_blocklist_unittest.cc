// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_blocklist.h"

#include "base/memory/raw_ref.h"
#include "base/test/gtest_util.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

using device::BluetoothUUID;
using DataPrefix = content::BluetoothBlocklist::DataPrefix;
using ManufacturerId = device::BluetoothDevice::ManufacturerId;

namespace content {

namespace {

// Unused if CHECK strings are discarded to reduce code bloat;
[[maybe_unused]] const char kInvalidUUIDErrorRegex[] = "uuid.IsValid\\(\\)";

}  // namespace

class BluetoothBlocklistTest : public ::testing::Test {
 public:
  BluetoothBlocklistTest() : list_(BluetoothBlocklist::Get()) {
    // Because BluetoothBlocklist is used via a singleton instance, the data
    // must be reset for each test.
    list_->ResetToDefaultValuesForTest();
  }
  const raw_ref<BluetoothBlocklist> list_;
};

TEST_F(BluetoothBlocklistTest, NonExcludedUUID) {
  BluetoothUUID non_excluded_uuid("00000000-0000-0000-0000-000000000000");
  EXPECT_FALSE(list_->IsExcluded(non_excluded_uuid));
  EXPECT_FALSE(list_->IsExcludedFromReads(non_excluded_uuid));
  EXPECT_FALSE(list_->IsExcludedFromWrites(non_excluded_uuid));
}

TEST_F(BluetoothBlocklistTest, ExcludeUUID) {
  BluetoothUUID excluded_uuid("eeee");
  list_->Add(excluded_uuid, BluetoothBlocklist::Value::EXCLUDE);
  EXPECT_TRUE(list_->IsExcluded(excluded_uuid));
  EXPECT_TRUE(list_->IsExcludedFromReads(excluded_uuid));
  EXPECT_TRUE(list_->IsExcludedFromWrites(excluded_uuid));
}

TEST_F(BluetoothBlocklistTest, ExcludeReadsUUID) {
  BluetoothUUID exclude_reads_uuid("eeee");
  list_->Add(exclude_reads_uuid, BluetoothBlocklist::Value::EXCLUDE_READS);
  EXPECT_FALSE(list_->IsExcluded(exclude_reads_uuid));
  EXPECT_TRUE(list_->IsExcludedFromReads(exclude_reads_uuid));
  EXPECT_FALSE(list_->IsExcludedFromWrites(exclude_reads_uuid));
}

TEST_F(BluetoothBlocklistTest, ExcludeWritesUUID) {
  BluetoothUUID exclude_writes_uuid("eeee");
  list_->Add(exclude_writes_uuid, BluetoothBlocklist::Value::EXCLUDE_WRITES);
  EXPECT_FALSE(list_->IsExcluded(exclude_writes_uuid));
  EXPECT_FALSE(list_->IsExcludedFromReads(exclude_writes_uuid));
  EXPECT_TRUE(list_->IsExcludedFromWrites(exclude_writes_uuid));
}

// Abreviated UUIDs used to create, or test against, the blocklist work
// correctly compared to full UUIDs.
TEST_F(BluetoothBlocklistTest, AbreviatedUUIDs) {
  list_->Add(BluetoothUUID("aaaa"), BluetoothBlocklist::Value::EXCLUDE);
  EXPECT_TRUE(
      list_->IsExcluded(BluetoothUUID("0000aaaa-0000-1000-8000-00805f9b34fb")));

  list_->Add(BluetoothUUID("0000bbbb-0000-1000-8000-00805f9b34fb"),
             BluetoothBlocklist::Value::EXCLUDE);
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("bbbb")));
}

// Tests permutations of previous values and then Add() with a new value,
// requiring result to be strictest result of the combination.
TEST_F(BluetoothBlocklistTest, Add_MergingExcludeValues) {
  list_->Add(BluetoothUUID("ee01"), BluetoothBlocklist::Value::EXCLUDE);
  list_->Add(BluetoothUUID("ee01"), BluetoothBlocklist::Value::EXCLUDE);
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("ee01")));

  list_->Add(BluetoothUUID("ee02"), BluetoothBlocklist::Value::EXCLUDE);
  list_->Add(BluetoothUUID("ee02"), BluetoothBlocklist::Value::EXCLUDE_READS);
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("ee02")));

  list_->Add(BluetoothUUID("ee03"), BluetoothBlocklist::Value::EXCLUDE);
  list_->Add(BluetoothUUID("ee03"), BluetoothBlocklist::Value::EXCLUDE_WRITES);
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("ee03")));

  list_->Add(BluetoothUUID("ee04"), BluetoothBlocklist::Value::EXCLUDE_READS);
  list_->Add(BluetoothUUID("ee04"), BluetoothBlocklist::Value::EXCLUDE);
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("ee04")));

  list_->Add(BluetoothUUID("ee05"), BluetoothBlocklist::Value::EXCLUDE_READS);
  list_->Add(BluetoothUUID("ee05"), BluetoothBlocklist::Value::EXCLUDE_READS);
  EXPECT_FALSE(list_->IsExcluded(BluetoothUUID("ee05")));
  EXPECT_TRUE(list_->IsExcludedFromReads(BluetoothUUID("ee05")));

  list_->Add(BluetoothUUID("ee06"), BluetoothBlocklist::Value::EXCLUDE_READS);
  list_->Add(BluetoothUUID("ee06"), BluetoothBlocklist::Value::EXCLUDE_WRITES);
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("ee06")));

  list_->Add(BluetoothUUID("ee07"), BluetoothBlocklist::Value::EXCLUDE_WRITES);
  list_->Add(BluetoothUUID("ee07"), BluetoothBlocklist::Value::EXCLUDE);
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("ee07")));

  list_->Add(BluetoothUUID("ee08"), BluetoothBlocklist::Value::EXCLUDE_WRITES);
  list_->Add(BluetoothUUID("ee08"), BluetoothBlocklist::Value::EXCLUDE_READS);
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("ee08")));

  list_->Add(BluetoothUUID("ee09"), BluetoothBlocklist::Value::EXCLUDE_WRITES);
  list_->Add(BluetoothUUID("ee09"), BluetoothBlocklist::Value::EXCLUDE_WRITES);
  EXPECT_FALSE(list_->IsExcluded(BluetoothUUID("ee09")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("ee09")));
}

// Tests Add() with string that contains many UUID:exclusion value pairs,
// checking that the correct blocklist entries are created for them.
TEST_F(BluetoothBlocklistTest, Add_StringWithValidEntries) {
  list_->Add(
      "0001:e,0002:r,0003:w, "  // Single items.
      "0004:r,0004:r, "         // Duplicate items.
      "0005:r,0005:w, "         // Items that merge.
      "00000006:e, "            // 8 char UUID.
      "00000007-0000-1000-8000-00805f9b34fb:e");

  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0001")));

  EXPECT_FALSE(list_->IsExcluded(BluetoothUUID("0002")));
  EXPECT_TRUE(list_->IsExcludedFromReads(BluetoothUUID("0002")));

  EXPECT_FALSE(list_->IsExcluded(BluetoothUUID("0003")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("0003")));

  EXPECT_FALSE(list_->IsExcluded(BluetoothUUID("0004")));
  EXPECT_TRUE(list_->IsExcludedFromReads(BluetoothUUID("0004")));

  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0005")));
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0006")));
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0007")));
}

// Tests Add() with strings that contain no valid UUID:exclusion value.
TEST_F(BluetoothBlocklistTest, Add_StringsWithNoValidEntries) {
  size_t previous_list_size = list_->size();
  list_->Add("");
  list_->Add("~!@#$%^&*()-_=+[]{}/*-");
  list_->Add(":");
  list_->Add(",");
  list_->Add(",,");
  list_->Add(",:,");
  list_->Add("1234:");
  list_->Add("1234:q");
  list_->Add("1234:E");
  list_->Add("1234:R");
  list_->Add("1234:W");
  list_->Add("1234:ee");
  list_->Add("1234 :e");
  list_->Add("1234: e");
  list_->Add("1:e");
  list_->Add("1:r");
  list_->Add("1:w");
  list_->Add("00001800-0000-1000-8000-00805f9b34fb:ee");
  list_->Add("z0001800-0000-1000-8000-00805f9b34fb:e");
  list_->Add("☯");
  EXPECT_EQ(previous_list_size, list_->size());
}

// Tests Add() with strings that contain exactly one valid UUID:exclusion value
// pair, and optionally other issues in the string that are ignored.
TEST_F(BluetoothBlocklistTest, Add_StringsWithOneValidEntry) {
  size_t previous_list_size = list_->size();
  list_->Add("0001:e");
  EXPECT_EQ(++previous_list_size, list_->size());
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0001")));

  list_->Add("00000002:e");
  EXPECT_EQ(++previous_list_size, list_->size());
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0002")));

  list_->Add("00000003-0000-1000-8000-00805f9b34fb:e");
  EXPECT_EQ(++previous_list_size, list_->size());
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0003")));

  list_->Add(" 0004:e ");
  EXPECT_EQ(++previous_list_size, list_->size());
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0004")));

  list_->Add(", 0005:e ,");
  EXPECT_EQ(++previous_list_size, list_->size());
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0005")));

  list_->Add(":, 0006:e ,,no");
  EXPECT_EQ(++previous_list_size, list_->size());
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0006")));

  list_->Add("0007:, 0008:e");
  EXPECT_EQ(++previous_list_size, list_->size());
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0008")));

  list_->Add("\r\n0009:e\n\r");
  EXPECT_EQ(++previous_list_size, list_->size());
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("0009")));
}

TEST_F(BluetoothBlocklistTest, IsExcluded_BluetoothScanFilter_ReturnsFalse) {
  list_->Add(BluetoothUUID("eeee"), BluetoothBlocklist::Value::EXCLUDE);
  list_->Add(BluetoothUUID("ee01"), BluetoothBlocklist::Value::EXCLUDE_READS);
  list_->Add(BluetoothUUID("ee02"), BluetoothBlocklist::Value::EXCLUDE_WRITES);
  {
    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr> empty_filters;
    EXPECT_FALSE(list_->IsExcluded(empty_filters));
  }
  {
    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>
        single_filter_with_no_services(1);

    single_filter_with_no_services[0] =
        blink::mojom::WebBluetoothLeScanFilter::New();

    EXPECT_FALSE(single_filter_with_no_services[0]->services);
    EXPECT_FALSE(list_->IsExcluded(single_filter_with_no_services));
  }
  {
    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr> single_empty_filter(
        1);

    single_empty_filter[0] = blink::mojom::WebBluetoothLeScanFilter::New();
    single_empty_filter[0]->services.emplace();

    EXPECT_EQ(0u, single_empty_filter[0]->services->size());
    EXPECT_FALSE(list_->IsExcluded(single_empty_filter));
  }
  {
    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>
        single_non_matching_filter(1);

    single_non_matching_filter[0] =
        blink::mojom::WebBluetoothLeScanFilter::New();
    single_non_matching_filter[0]->services.emplace();
    single_non_matching_filter[0]->services->push_back(BluetoothUUID("0000"));

    EXPECT_FALSE(list_->IsExcluded(single_non_matching_filter));
  }
  {
    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>
        multiple_non_matching_filters(2);

    multiple_non_matching_filters[0] =
        blink::mojom::WebBluetoothLeScanFilter::New();
    multiple_non_matching_filters[0]->services.emplace();
    multiple_non_matching_filters[0]->services->push_back(
        BluetoothUUID("0000"));
    multiple_non_matching_filters[0]->services->push_back(
        BluetoothUUID("ee01"));

    multiple_non_matching_filters[1] =
        blink::mojom::WebBluetoothLeScanFilter::New();
    multiple_non_matching_filters[1]->services.emplace();
    multiple_non_matching_filters[1]->services->push_back(
        BluetoothUUID("ee02"));
    multiple_non_matching_filters[1]->services->push_back(
        BluetoothUUID("0003"));

    EXPECT_FALSE(list_->IsExcluded(multiple_non_matching_filters));
  }
}

TEST_F(BluetoothBlocklistTest, IsExcluded_BluetoothScanFilter_ReturnsTrue) {
  list_->Add(BluetoothUUID("eeee"), BluetoothBlocklist::Value::EXCLUDE);
  {
    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>
        single_matching_filter(1);

    single_matching_filter[0] = blink::mojom::WebBluetoothLeScanFilter::New();
    single_matching_filter[0]->services.emplace();
    single_matching_filter[0]->services->push_back(BluetoothUUID("eeee"));

    EXPECT_TRUE(list_->IsExcluded(single_matching_filter));
  }
  {
    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>
        first_matching_filter(2);

    first_matching_filter[0] = blink::mojom::WebBluetoothLeScanFilter::New();
    first_matching_filter[0]->services.emplace();
    first_matching_filter[0]->services->push_back(BluetoothUUID("eeee"));
    first_matching_filter[0]->services->push_back(BluetoothUUID("0001"));

    first_matching_filter[1] = blink::mojom::WebBluetoothLeScanFilter::New();
    first_matching_filter[1]->services.emplace();
    first_matching_filter[1]->services->push_back(BluetoothUUID("0002"));
    first_matching_filter[1]->services->push_back(BluetoothUUID("0003"));

    EXPECT_TRUE(list_->IsExcluded(first_matching_filter));
  }
  {
    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr> last_matching_filter(
        2);

    last_matching_filter[0] = blink::mojom::WebBluetoothLeScanFilter::New();
    last_matching_filter[0]->services.emplace();
    last_matching_filter[0]->services->push_back(BluetoothUUID("0001"));
    last_matching_filter[0]->services->push_back(BluetoothUUID("0001"));

    last_matching_filter[1] = blink::mojom::WebBluetoothLeScanFilter::New();
    last_matching_filter[1]->services.emplace();
    last_matching_filter[1]->services->push_back(BluetoothUUID("0002"));
    last_matching_filter[1]->services->push_back(BluetoothUUID("eeee"));

    EXPECT_TRUE(list_->IsExcluded(last_matching_filter));
  }
  {
    std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>
        multiple_matching_filters(2);

    multiple_matching_filters[0] =
        blink::mojom::WebBluetoothLeScanFilter::New();
    multiple_matching_filters[0]->services.emplace();
    multiple_matching_filters[0]->services->push_back(BluetoothUUID("eeee"));
    multiple_matching_filters[0]->services->push_back(BluetoothUUID("eeee"));

    multiple_matching_filters[1] =
        blink::mojom::WebBluetoothLeScanFilter::New();
    multiple_matching_filters[1]->services.emplace();
    multiple_matching_filters[1]->services->push_back(BluetoothUUID("eeee"));
    multiple_matching_filters[1]->services->push_back(BluetoothUUID("eeee"));

    EXPECT_TRUE(list_->IsExcluded(multiple_matching_filters));
  }
}

TEST_F(BluetoothBlocklistTest, RemoveExcludedUUIDs_NonMatching) {
  list_->Add(BluetoothUUID("eeee"), BluetoothBlocklist::Value::EXCLUDE);
  list_->Add(BluetoothUUID("ee01"), BluetoothBlocklist::Value::EXCLUDE_READS);
  list_->Add(BluetoothUUID("ee02"), BluetoothBlocklist::Value::EXCLUDE_WRITES);

  // options.optional_services should be the same before and after
  // RemoveExcludedUUIDs().
  {
    // Empty optional_services.
    blink::mojom::WebBluetoothRequestDeviceOptions options;

    std::vector<BluetoothUUID> expected = options.optional_services;

    list_->RemoveExcludedUUIDs(&options);
    EXPECT_EQ(expected, options.optional_services);
  }
  {
    // One non-matching service in optional_services.
    blink::mojom::WebBluetoothRequestDeviceOptions options;
    options.optional_services.push_back(BluetoothUUID("0000"));

    std::vector<BluetoothUUID> expected = options.optional_services;

    list_->RemoveExcludedUUIDs(&options);
    EXPECT_EQ(expected, options.optional_services);
  }
  {
    // Multiple non-matching services in optional_services.
    blink::mojom::WebBluetoothRequestDeviceOptions options;
    options.optional_services.push_back(BluetoothUUID("0000"));
    options.optional_services.push_back(BluetoothUUID("ee01"));
    options.optional_services.push_back(BluetoothUUID("ee02"));
    options.optional_services.push_back(BluetoothUUID("0003"));

    std::vector<BluetoothUUID> expected = options.optional_services;

    list_->RemoveExcludedUUIDs(&options);
    EXPECT_EQ(expected, options.optional_services);
  }
}

TEST_F(BluetoothBlocklistTest, RemoveExcludedUuids_Matching) {
  list_->Add(BluetoothUUID("eeee"), BluetoothBlocklist::Value::EXCLUDE);
  list_->Add(BluetoothUUID("eee2"), BluetoothBlocklist::Value::EXCLUDE);
  list_->Add(BluetoothUUID("eee3"), BluetoothBlocklist::Value::EXCLUDE);
  list_->Add(BluetoothUUID("eee4"), BluetoothBlocklist::Value::EXCLUDE);
  {
    // Single matching service in optional_services.
    blink::mojom::WebBluetoothRequestDeviceOptions options;
    options.optional_services.push_back(BluetoothUUID("eeee"));

    std::vector<BluetoothUUID> expected;

    list_->RemoveExcludedUUIDs(&options);

    EXPECT_EQ(expected, options.optional_services);
  }
  {
    // Single matching of many services in optional_services.
    blink::mojom::WebBluetoothRequestDeviceOptions options;
    options.optional_services.push_back(BluetoothUUID("0000"));
    options.optional_services.push_back(BluetoothUUID("eeee"));
    options.optional_services.push_back(BluetoothUUID("0001"));

    std::vector<BluetoothUUID> expected;
    expected.push_back(BluetoothUUID("0000"));
    expected.push_back(BluetoothUUID("0001"));

    list_->RemoveExcludedUUIDs(&options);
    EXPECT_EQ(expected, options.optional_services);
  }
  {
    // All matching of many services in optional_services.
    blink::mojom::WebBluetoothRequestDeviceOptions options;
    options.optional_services.push_back(BluetoothUUID("eee2"));
    options.optional_services.push_back(BluetoothUUID("eee4"));
    options.optional_services.push_back(BluetoothUUID("eee3"));
    options.optional_services.push_back(BluetoothUUID("eeee"));

    std::vector<BluetoothUUID> expected;

    list_->RemoveExcludedUUIDs(&options);
    EXPECT_EQ(expected, options.optional_services);
  }
}

TEST_F(BluetoothBlocklistTest, VerifyDefaultBlocklistSize) {
  // REMINDER: ADD new blocklist items to tests below for each exclusion type.
  EXPECT_EQ(15u, list_->size());
}

TEST_F(BluetoothBlocklistTest, VerifyDefaultExcludeList) {
  EXPECT_FALSE(list_->IsExcluded(BluetoothUUID("1800")));
  EXPECT_FALSE(list_->IsExcluded(BluetoothUUID("1801")));
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("1812")));
  EXPECT_TRUE(
      list_->IsExcluded(BluetoothUUID("00001530-1212-efde-1523-785feabcd123")));
  EXPECT_TRUE(
      list_->IsExcluded(BluetoothUUID("f000ffc0-0451-4000-b000-000000000000")));
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("00060000")));
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("fff9")));
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("fffd")));
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("fde2")));
  EXPECT_FALSE(list_->IsExcluded(BluetoothUUID("2a02")));
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("2a03")));
  EXPECT_TRUE(list_->IsExcluded(BluetoothUUID("2a25")));
  EXPECT_FALSE(
      list_->IsExcluded(BluetoothUUID("bad1c9a2-9a5b-4015-8b60-1579bbbf2135")));
  EXPECT_FALSE(list_->IsExcluded(BluetoothUUID("2902")));
  EXPECT_FALSE(list_->IsExcluded(BluetoothUUID("2903")));
  EXPECT_TRUE(
      list_->IsExcluded(BluetoothUUID("bad2ddcf-60db-45cd-bef9-fd72b153cf7c")));
  EXPECT_FALSE(
      list_->IsExcluded(BluetoothUUID("bad3ec61-3cc3-4954-9702-7977df514114")));
}

TEST_F(BluetoothBlocklistTest, VerifyDefaultExcludeReadList) {
  EXPECT_FALSE(list_->IsExcludedFromReads(BluetoothUUID("1800")));
  EXPECT_FALSE(list_->IsExcludedFromReads(BluetoothUUID("1801")));
  EXPECT_TRUE(list_->IsExcludedFromReads(BluetoothUUID("1812")));
  EXPECT_TRUE(list_->IsExcludedFromReads(
      BluetoothUUID("00001530-1212-efde-1523-785feabcd123")));
  EXPECT_TRUE(list_->IsExcludedFromReads(
      BluetoothUUID("f000ffc0-0451-4000-b000-000000000000")));
  EXPECT_TRUE(list_->IsExcludedFromReads(BluetoothUUID("00060000")));
  EXPECT_TRUE(list_->IsExcludedFromReads(BluetoothUUID("fff9")));
  EXPECT_TRUE(list_->IsExcludedFromReads(BluetoothUUID("fffd")));
  EXPECT_TRUE(list_->IsExcludedFromReads(BluetoothUUID("fde2")));
  EXPECT_FALSE(list_->IsExcludedFromReads(BluetoothUUID("2a02")));
  EXPECT_TRUE(list_->IsExcludedFromReads(BluetoothUUID("2a03")));
  EXPECT_TRUE(list_->IsExcludedFromReads(BluetoothUUID("2a25")));
  EXPECT_TRUE(list_->IsExcludedFromReads(
      BluetoothUUID("bad1c9a2-9a5b-4015-8b60-1579bbbf2135")));
  EXPECT_FALSE(list_->IsExcludedFromReads(BluetoothUUID("2902")));
  EXPECT_FALSE(list_->IsExcludedFromReads(BluetoothUUID("2903")));
  EXPECT_TRUE(list_->IsExcludedFromReads(
      BluetoothUUID("bad2ddcf-60db-45cd-bef9-fd72b153cf7c")));
  EXPECT_TRUE(list_->IsExcludedFromReads(
      BluetoothUUID("bad3ec61-3cc3-4954-9702-7977df514114")));
}

TEST_F(BluetoothBlocklistTest, VerifyDefaultExcludeWriteList) {
  EXPECT_FALSE(list_->IsExcludedFromWrites(BluetoothUUID("1800")));
  EXPECT_FALSE(list_->IsExcludedFromWrites(BluetoothUUID("1801")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("1812")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(
      BluetoothUUID("00001530-1212-efde-1523-785feabcd123")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(
      BluetoothUUID("f000ffc0-0451-4000-b000-000000000000")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("00060000")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("fff9")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("fffd")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("fde2")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("2a02")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("2a03")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("2a25")));
  EXPECT_FALSE(list_->IsExcludedFromWrites(
      BluetoothUUID("bad1c9a2-9a5b-4015-8b60-1579bbbf2135")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("2902")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(BluetoothUUID("2903")));
  EXPECT_TRUE(list_->IsExcludedFromWrites(
      BluetoothUUID("bad2ddcf-60db-45cd-bef9-fd72b153cf7c")));
  EXPECT_FALSE(list_->IsExcludedFromWrites(
      BluetoothUUID("bad3ec61-3cc3-4954-9702-7977df514114")));
}

TEST_F(BluetoothBlocklistTest, NonExcludedManufacturerDataFilter) {
  list_->Add(/*company_identifier=*/0x1,
             /*prefix=*/{{0x01, 0x3f}, {0x00, 0x00}});

  // filter is not a strict subset, data different.
  {
    blink::mojom::WebBluetoothCompanyPtr company_identifier =
        blink::mojom::WebBluetoothCompany::New(0x1);
    std::vector<blink::mojom::WebBluetoothDataFilterPtr>
        manufacturer_data_filter;
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x02, 0x3f));
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x02, 0xff));
    EXPECT_FALSE(
        list_->IsExcluded(company_identifier, manufacturer_data_filter));
  }

  // filter is not a strict subset, filter length is shorter.
  {
    blink::mojom::WebBluetoothCompanyPtr company_identifier =
        blink::mojom::WebBluetoothCompany::New(0x1);
    std::vector<blink::mojom::WebBluetoothDataFilterPtr>
        manufacturer_data_filter;
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x01, 0x3f));
    EXPECT_FALSE(
        list_->IsExcluded(company_identifier, manufacturer_data_filter));
  }

  // filter is not a strict subset, filter mask is not a subset of blocked data
  // mask.
  {
    blink::mojom::WebBluetoothCompanyPtr company_identifier =
        blink::mojom::WebBluetoothCompany::New(0x1);
    std::vector<blink::mojom::WebBluetoothDataFilterPtr>
        manufacturer_data_filter;
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x01, 0x0f));
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x02, 0xff));
    EXPECT_FALSE(
        list_->IsExcluded(company_identifier, manufacturer_data_filter));
  }

  // filter is not a strict subset, filter length is longer but filter mask is
  // not a subset of blocked data mask.
  {
    blink::mojom::WebBluetoothCompanyPtr company_identifier =
        blink::mojom::WebBluetoothCompany::New(0x1);
    std::vector<blink::mojom::WebBluetoothDataFilterPtr>
        manufacturer_data_filter;
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x01, 0x0f));
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x02, 0xff));
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x03, 0xff));
    EXPECT_FALSE(
        list_->IsExcluded(company_identifier, manufacturer_data_filter));
  }

  // Different company_identifier
  {
    blink::mojom::WebBluetoothCompanyPtr company_identifier =
        blink::mojom::WebBluetoothCompany::New(0x2);
    std::vector<blink::mojom::WebBluetoothDataFilterPtr>
        manufacturer_data_filter;
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x01, 0xff));
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x02, 0xff));
    EXPECT_FALSE(
        list_->IsExcluded(company_identifier, manufacturer_data_filter));
  }
}

TEST_F(BluetoothBlocklistTest, ExcludedManufacturerDataFilter) {
  list_->Add(/*company_identifier=*/0x1,
             /*prefix=*/{{0x01, 0x3f}, {0x00, 0x00}});

  // filter is a strict subset, filter length is the same.
  {
    blink::mojom::WebBluetoothCompanyPtr company_identifier =
        blink::mojom::WebBluetoothCompany::New(0x1);
    std::vector<blink::mojom::WebBluetoothDataFilterPtr>
        manufacturer_data_filter;
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x01, 0x3f));
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x02, 0xff));
    EXPECT_TRUE(
        list_->IsExcluded(company_identifier, manufacturer_data_filter));
  }

  // filter is a strict subset, filter length is longer.
  {
    blink::mojom::WebBluetoothCompanyPtr company_identifier =
        blink::mojom::WebBluetoothCompany::New(0x1);
    std::vector<blink::mojom::WebBluetoothDataFilterPtr>
        manufacturer_data_filter;
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x01, 0x3f));
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x02, 0xff));
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x03, 0xff));
    EXPECT_TRUE(
        list_->IsExcluded(company_identifier, manufacturer_data_filter));
  }

  // filter is a strict subset, filter mask is subset of blocked data mask.
  {
    blink::mojom::WebBluetoothCompanyPtr company_identifier =
        blink::mojom::WebBluetoothCompany::New(0x1);
    std::vector<blink::mojom::WebBluetoothDataFilterPtr>
        manufacturer_data_filter;
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x01, 0xff));
    manufacturer_data_filter.push_back(
        blink::mojom::WebBluetoothDataFilter::New(0x02, 0xff));
    EXPECT_TRUE(
        list_->IsExcluded(company_identifier, manufacturer_data_filter));
  }
}

TEST_F(BluetoothBlocklistTest, ExcludedManufacturerData) {
  list_->Add(/*company_identifier=*/0x1,
             /*prefix=*/{{0x01, 0x0f}});

  // Data prefix matches.
  {
    device::BluetoothDevice::ManufacturerId manufacturer_id = 0x1;
    device::BluetoothDevice::ManufacturerData manufacturer_data = {
        0x1,
        0x39,
    };
    EXPECT_TRUE(list_->IsExcluded(manufacturer_id, manufacturer_data));
  }

  // Data pattern matches.
  list_->ResetToDefaultValuesForTest();
  list_->Add(/*company_identifier=*/0x1,
             /*prefix=*/{{0x01, 0x0f}, {0x00, 0x00}});
  {
    device::BluetoothDevice::ManufacturerId manufacturer_id = 0x1;
    device::BluetoothDevice::ManufacturerData manufacturer_data = {
        0x1,
        0x39,
    };
    EXPECT_TRUE(list_->IsExcluded(manufacturer_id, manufacturer_data));
  }
}

TEST_F(BluetoothBlocklistTest, NonExcludedManufacturerData) {
  list_->Add(/*company_identifier=*/0x1,
             /*prefix=*/{{0x01, 0x0f}, {0x00, 0x00}});

  // Data prefix not doesn't match.
  {
    device::BluetoothDevice::ManufacturerId manufacturer_id = 0x1;
    device::BluetoothDevice::ManufacturerData manufacturer_data = {
        0x2,
        0x47,
    };
    EXPECT_FALSE(list_->IsExcluded(manufacturer_id, manufacturer_data));
  }

  // Manufacturer data is shorter.
  {
    device::BluetoothDevice::ManufacturerId manufacturer_id = 0x1;
    device::BluetoothDevice::ManufacturerData manufacturer_data = {
        0x1,
    };
    EXPECT_FALSE(list_->IsExcluded(manufacturer_id, manufacturer_data));
  }

  // Empty manufacturer data.
  {
    device::BluetoothDevice::ManufacturerId manufacturer_id = 0x1;
    device::BluetoothDevice::ManufacturerData manufacturer_data;
    EXPECT_FALSE(list_->IsExcluded(manufacturer_id, manufacturer_data));
  }

  // Different manufacturer id.
  {
    device::BluetoothDevice::ManufacturerId manufacturer_id = 0x2;
    device::BluetoothDevice::ManufacturerData manufacturer_data = {
        0x1,
        0x47,
    };
    EXPECT_FALSE(list_->IsExcluded(manufacturer_id, manufacturer_data));
  }
}

}  // namespace content
