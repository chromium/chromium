// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/ble_scan_parser/ble_scan_parser.h"

#include <stdint.h>

#include <utility>

#include "base/containers/span.h"
#include "base/containers/span_rust.h"
#include "base/notreached.h"
#include "device/bluetooth/bluez/ble_scan_parser/cxx.rs.h"
#include "device/bluetooth/bluez/ble_scan_parser/scan_record.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bluez {
namespace {

using ble_scan_parser_bridge::UuidFormat;

device::BluetoothUUID ParseUuid(base::span<const uint8_t> bytes,
                                UuidFormat format) {
  std::array<uint8_t, 16> uuid_bytes;
  bool result = ble_scan_parser_bridge::parse_uuid_for_test(
      base::SpanToRustSlice(bytes), format, uuid_bytes);
  return result ? device::BluetoothUUID(uuid_bytes) : device::BluetoothUUID();
}

bool ParseServiceUuids(base::span<const uint8_t> bytes,
                       UuidFormat format,
                       std::vector<device::BluetoothUUID>& out) {
  ble_scan_parser_bridge::UuidListBuilderForTest builder;
  bool result = ble_scan_parser_bridge::parse_service_uuids_for_test(
      base::SpanToRustSlice(bytes), format, builder);
  out = std::move(builder.uuids);
  return result;
}

using BleScanParserTest = testing::Test;

TEST(BleScanParserImplTest, ParseBadUuidLengthReturnsEmptyString) {
  std::vector<uint8_t> bad_uuid(0xab, 5);
  EXPECT_FALSE(ParseUuid(bad_uuid, UuidFormat::With16Bits).IsValid());
  EXPECT_FALSE(ParseUuid(bad_uuid, UuidFormat::With32Bits).IsValid());
  EXPECT_FALSE(ParseUuid(bad_uuid, UuidFormat::With128Bits).IsValid());
}

TEST(BleScanParserImplTest, Parse16BitUuid) {
  const uint8_t kUuid16[] = {0xab, 0xcd};
  const device::BluetoothUUID kExpected("0000CDAB-0000-1000-8000-00805F9B34FB");
  EXPECT_EQ(kExpected, ParseUuid(kUuid16, UuidFormat::With16Bits));
}

TEST(BleScanParserImplTest, Parse32BitUuid) {
  const uint8_t kUuid32[] = {0xab, 0xcd, 0xef, 0x01};
  const device::BluetoothUUID kExpected("01EFCDAB-0000-1000-8000-00805F9B34FB");
  EXPECT_EQ(kExpected, ParseUuid(kUuid32, UuidFormat::With32Bits));
}

TEST(BleScanParserImplTest, Parse128BitUuid) {
  const uint8_t kUuid128[] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89,
                              0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};
  const device::BluetoothUUID kExpected("89674523-01EF-CDAB-8967-452301EFCDAB");
  EXPECT_EQ(kExpected, ParseUuid(kUuid128, UuidFormat::With128Bits));
}

TEST(BleScanParserImplTest, Parse16BitServiceUuids) {
  std::vector<device::BluetoothUUID> expected = {
      device::BluetoothUUID("0000CDAB-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("000001EF-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("00004523-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("00008967-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("0000CDAB-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("000001EF-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("00004523-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("00008967-0000-1000-8000-00805F9B34FB"),
  };

  const uint8_t kUuids[] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89,
                            0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};

  std::vector<device::BluetoothUUID> actual;
  EXPECT_TRUE(ParseServiceUuids(kUuids, UuidFormat::With16Bits, actual));
  EXPECT_EQ(expected, actual);
}

TEST(BleScanParserImplTest, Parse32BitServiceUuids) {
  std::vector<device::BluetoothUUID> expected = {
      device::BluetoothUUID("01EFCDAB-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("89674523-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("01EFCDAB-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("89674523-0000-1000-8000-00805F9B34FB"),
  };

  const uint8_t kUuids[] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89,
                            0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};

  std::vector<device::BluetoothUUID> actual;
  EXPECT_TRUE(ParseServiceUuids(kUuids, UuidFormat::With32Bits, actual));
  EXPECT_EQ(expected, actual);
}

TEST(BleScanParserImplTest, Parse128BitServiceUuids) {
  std::vector<device::BluetoothUUID> expected = {
      device::BluetoothUUID("89674523-01EF-CDAB-8967-452301EFCDAB"),
      device::BluetoothUUID("89674523-01EF-CDAB-01EF-CDAB89674523"),
  };

  const uint8_t kUuids[] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89,
                            0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89,
                            0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01,
                            0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};

  std::vector<device::BluetoothUUID> actual;
  EXPECT_TRUE(ParseServiceUuids(kUuids, UuidFormat::With128Bits, actual));
  EXPECT_EQ(expected, actual);
}

TEST(BleScanParserImplTest, ParseBadServiceUuids) {
  std::vector<device::BluetoothUUID> actual;

  const uint8_t kBadData[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                              0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                              0x0c, 0x0d, 0x0e, 0x0f, 0x01};

  // The length of `kBadData` is not a multiple of 2, 4, or 16 bytes. Any
  // attempt to parse this should fail.
  EXPECT_FALSE(ParseServiceUuids(kBadData, UuidFormat::With16Bits, actual));
  EXPECT_FALSE(ParseServiceUuids(kBadData, UuidFormat::With32Bits, actual));
  EXPECT_FALSE(ParseServiceUuids(kBadData, UuidFormat::With128Bits, actual));
}

TEST(BleScanParserImplTest, ParseBleAdvertisingScan) {
  std::vector<device::BluetoothUUID> expected_service_uuids = {
      device::BluetoothUUID("0000ABCD-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("0000EF01-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("ABCDEF01-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("23456789-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("ABCDEF01-2345-6789-ABCD-EF0123456789"),
  };

  std::vector<uint8_t> service_data = {0xa1, 0xb2, 0xc3, 0xd4, 0xe5};
  ScanRecord::ServiceDataMap expected_service_data_map;
  expected_service_data_map[device::BluetoothUUID(
      "0000DCAB-0000-1000-8000-00805F9B34FB")] = service_data;

  std::vector<uint8_t> manufacturer_data = {0x1a, 0x2b, 0x3c, 0x4d};
  ScanRecord::ManufacturerDataMap expected_manufacturer_data_map;
  expected_manufacturer_data_map[0xd00d] = manufacturer_data;

  const uint8_t kRawData[] = {
      // Length of the rest of the section, field type, data.
      // Advertising flag = 0x42
      0x02, 0x01, 0x42,
      // 16-bit service UUIDs 0000abcd-... and 0000ef01-...
      0x05, 0x02, 0xcd, 0xab, 0x01, 0xef,
      // TX power = 0x1b
      0x02, 0x0a, 0x1b,
      // 32-bit service UUIDs abcdef01-... and 23456789-...
      0x09, 0x05, 0x01, 0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23,
      // Local name 'Steve'
      0x06, 0x08, 0x53, 0x74, 0x65, 0x76, 0x65,
      // 128-bit service UUID abcdef01-2345-6789-abcd-ef0123456789
      0x11, 0x06, 0x89, 0x67, 0x45, 0x23, 0x01, 0xef, 0xcd, 0xab, 0x89, 0x67,
      0x45, 0x23, 0x01, 0xef, 0xcd, 0xab,
      // Service data map 0000dcab-... => { 0xa1, 0xb2, 0xc3, 0xd4, 0xe5 }
      0x08, 0x16, 0xab, 0xdc, 0xa1, 0xb2, 0xc3, 0xd4, 0xe5,
      // Manufacturer data map 0xd00d => { 0x1a, 0x2b, 0x3c, 0x4d }
      0x07, 0xff, 0x0d, 0xd0, 0x1a, 0x2b, 0x3c, 0x4d};

  std::optional<ScanRecord> actual = ParseBleScan(kRawData);
  ASSERT_TRUE(actual);
  EXPECT_EQ(0x42, actual->advertising_flags);
  EXPECT_EQ(0x1b, actual->tx_power);
  EXPECT_EQ("Steve", actual->advertisement_name);
  EXPECT_EQ(expected_service_uuids, actual->service_uuids);
  EXPECT_EQ(expected_service_data_map, actual->service_data_map);
  EXPECT_EQ(expected_manufacturer_data_map, actual->manufacturer_data_map);
}

TEST(BleScanParserImplTest, ParseEmptyBleScan) {
  std::optional<ScanRecord> actual = ParseBleScan({});
  ASSERT_TRUE(actual);
  EXPECT_EQ(-1, actual->advertising_flags);
  EXPECT_EQ(0, actual->tx_power);
  EXPECT_EQ("", actual->advertisement_name);
  EXPECT_TRUE(actual->service_uuids.empty());
  EXPECT_TRUE(actual->service_data_map.empty());
  EXPECT_TRUE(actual->manufacturer_data_map.empty());
}

TEST(BleScanParserImplTest, ParseBleScanWithUnknownDataType) {
  const uint8_t kRawData[] = {
      // Length of the rest of the section, field type, data.
      // Advertising flag = 0x42
      0x02,
      0x01,
      0x42,
      // TX power = 0x1b
      0x02,
      0x0a,
      0x1b,
      // Local name 'Steve'
      0x06,
      0x08,
      0x53,
      0x74,
      0x65,
      0x76,
      0x65,
      // 0x00 is not a data type supported by the current parser. It should be
      // ignored and not treated as a parse failure.
      0x02,
      0x00,
      0x00,
  };
  std::optional<ScanRecord> actual = ParseBleScan(kRawData);
  ASSERT_TRUE(actual);
  EXPECT_EQ(0x42, actual->advertising_flags);
  EXPECT_EQ(0x1b, actual->tx_power);
  EXPECT_EQ("Steve", actual->advertisement_name);
  EXPECT_TRUE(actual->service_uuids.empty());
  EXPECT_TRUE(actual->service_data_map.empty());
  EXPECT_TRUE(actual->manufacturer_data_map.empty());
}

TEST(BleScanParserImplTest, ParseBleScanWithBadLengthPacket) {
  {
    const uint8_t kRawData[] = {
        // Length of the rest of the section, field type, data.
        // Advertising flag = 0x42
        0x02, 0x01, 0x42,
        // TX power = 0x1b
        0x02, 0x0a, 0x1b,
        // Local name 'Steve'
        0x06, 0x08, 0x53, 0x74, 0x65, 0x76, 0x65,
        // A packet length of 0 should be considered invalid and cause parsing
        // to fail.
        0x00};
    ASSERT_FALSE(ParseBleScan(kRawData));
  }

  {
    const uint8_t kRawData[] = {
        // Length of the rest of the section, field type, data.
        // Advertising flag = 0x42
        0x02, 0x01, 0x42,
        // TX power = 0x1b
        0x02, 0x0a, 0x1b,
        // Local name 'Steve'
        0x06, 0x08, 0x53, 0x74, 0x65, 0x76, 0x65,
        // A packet length of 1 should also be considered invalid and cause
        // parsing to fail.
        // 0x01 is under the minimum packet length.
        0x01, 0x00};
    ASSERT_FALSE(ParseBleScan(kRawData));
  }

  {
    const uint8_t kRawData[] = {
        // The packet is longer than the data.
        0xff,
    };
    ASSERT_FALSE(ParseBleScan(kRawData));
  }
}

TEST(BleScanParserImplTest, ParseBleScanWithBad16BitServiceUuid) {
  const uint8_t kRawData[] = {
      // 16-bit service UUID missing the final byte.
      0x04, 0x02, 0xcd, 0xab, 0x01,
  };

  ASSERT_FALSE(ParseBleScan(kRawData));
}

TEST(BleScanParserImplTest, ParseBleScanWithBad32BitServiceUuid) {
  const uint8_t kRawData[] = {
      // 32-bit service UUID missing the final byte.
      0x08, 0x05, 0x01, 0xef, 0xcd, 0xab, 0x89, 0x67, 0x45,
  };
  ASSERT_FALSE(ParseBleScan(kRawData));
}

TEST(BleScanParserImplTest, ParseBleScanWithBad128BitServiceUuid) {
  const uint8_t kRawData[] = {
      // 128-bit service UUID missing the final byte.
      0x10, 0x06, 0x89, 0x67, 0x45, 0x23, 0x01, 0xef, 0xcd,
      0xab, 0x89, 0x67, 0x45, 0x23, 0x01, 0xef, 0xcd,
  };
  ASSERT_FALSE(ParseBleScan(kRawData));
}

TEST(BleScanParserImplTest, ParseBleScanWithBadServiceDataMap) {
  const uint8_t kRawData[] = {
      // A service data map entry has a 16-bit UUID followed by data. The entry
      // has an incomplete 16-bit UUID, so it should fail to parse.
      0x02,
      0x16,
      0xab,
  };
  ASSERT_FALSE(ParseBleScan(kRawData));
}

TEST(BleScanParserImplTest, ParseBleScanWithBadManufacturerDataMap) {
  const uint8_t kRawData[] = {
      // A manufacturer data map entry has a 16-bit manufacturer code followed
      // by data. The entry has only 8 bits of the manufacturer code, so it
      // should fail to parse.
      0x02,
      0xff,
      0x0d,
  };
  ASSERT_FALSE(ParseBleScan(kRawData));
}

TEST(BleScanParserImplTest, ParseBleScanWithMultiByteFlags) {
  const uint8_t kRawData[] = {
      // Length of the rest of the section, field type, data.
      // Advertising flag = 0x42. Additional trailing bytes should be ignored;
      // only the first data byte should be used for the flags.
      0x03,
      0x01,
      0x42,
      0x43,
      // TX power = 0x1b
      0x02,
      0x0a,
      0x1b,
      // Local name 'Steve'
      0x06,
      0x08,
      0x53,
      0x74,
      0x65,
      0x76,
      0x65,
  };
  std::optional<ScanRecord> actual = ParseBleScan(kRawData);
  ASSERT_TRUE(actual);
  EXPECT_EQ(0x42, actual->advertising_flags);
  EXPECT_EQ(0x1b, actual->tx_power);
  EXPECT_EQ("Steve", actual->advertisement_name);
  EXPECT_TRUE(actual->service_uuids.empty());
  EXPECT_TRUE(actual->service_data_map.empty());
  EXPECT_TRUE(actual->manufacturer_data_map.empty());
}

TEST(BleScanParserImplTest, ParseBleScanWithMultipleFlags) {
  const uint8_t kRawData[] = {
      // Length of the rest of the section, field type, data.
      // Advertising flag = 0x42
      0x02,
      0x01,
      0x42,
      // Another advertising flag = 0x43. The last one seen should be used.
      0x02,
      0x01,
      0x43,
      // TX power = 0x1b
      0x02,
      0x0a,
      0x1b,
      // Local name 'Steve'
      0x06,
      0x08,
      0x53,
      0x74,
      0x65,
      0x76,
      0x65,
  };
  std::optional<ScanRecord> actual = ParseBleScan(kRawData);
  ASSERT_TRUE(actual);
  EXPECT_EQ(0x43, actual->advertising_flags);
  EXPECT_EQ(0x1b, actual->tx_power);
  EXPECT_EQ("Steve", actual->advertisement_name);
  EXPECT_TRUE(actual->service_uuids.empty());
  EXPECT_TRUE(actual->service_data_map.empty());
  EXPECT_TRUE(actual->manufacturer_data_map.empty());
}

TEST(BleScanParserImplTest, ParseBleScanWithMultiByteTxPower) {
  const uint8_t kRawData[] = {
      // Length of the rest of the section, field type, data.
      // Advertising flag = 0x42
      0x02,
      0x01,
      0x42,
      // TX power = 0x1b, Additional trailing bytes should be ignored; only the
      // first data byte should be used for the TX power.
      0x03,
      0x0a,
      0x1b,
      0x1c,
      // Local name 'Steve'
      0x06,
      0x08,
      0x53,
      0x74,
      0x65,
      0x76,
      0x65,
  };
  std::optional<ScanRecord> actual = ParseBleScan(kRawData);
  ASSERT_TRUE(actual);
  EXPECT_EQ(0x42, actual->advertising_flags);
  EXPECT_EQ(0x1b, actual->tx_power);
  EXPECT_EQ("Steve", actual->advertisement_name);
  EXPECT_TRUE(actual->service_uuids.empty());
  EXPECT_TRUE(actual->service_data_map.empty());
  EXPECT_TRUE(actual->manufacturer_data_map.empty());
}

TEST(BleScanParserImplTest, ParseBleScanWithMultipleTxPowers) {
  const uint8_t kRawData[] = {
      // Length of the rest of the section, field type, data.
      // Advertising flag = 0x42
      0x02,
      0x01,
      0x42,
      // TX power = 0x1b
      0x02,
      0x0a,
      0x1b,
      // TX power = 0x1c. The last one seen should be used.
      0x02,
      0x0a,
      0x1c,
      // Local name 'Steve'
      0x06,
      0x08,
      0x53,
      0x74,
      0x65,
      0x76,
      0x65,
  };
  std::optional<ScanRecord> actual = ParseBleScan(kRawData);
  ASSERT_TRUE(actual);
  EXPECT_EQ(0x42, actual->advertising_flags);
  EXPECT_EQ(0x1c, actual->tx_power);
  EXPECT_EQ("Steve", actual->advertisement_name);
  EXPECT_TRUE(actual->service_uuids.empty());
  EXPECT_TRUE(actual->service_data_map.empty());
  EXPECT_TRUE(actual->manufacturer_data_map.empty());
}

TEST(BleScanParserImplTest, ParseBleScanWithMultipleAdvertisementNames) {
  const uint8_t kRawData[] = {
      // Length of the rest of the section, field type, data.
      // Advertising flag = 0x42
      0x02,
      0x01,
      0x42,
      // TX power = 0x1b
      0x02,
      0x0a,
      0x1b,
      // Local name 'Steve'
      0x06,
      0x08,
      0x53,
      0x74,
      0x65,
      0x76,
      0x65,
      // Local name 'Hello'. The last one seen should be used.
      0x06,
      0x08,
      0x48,
      0x65,
      0x6c,
      0x6c,
      0x6f,
  };
  std::optional<ScanRecord> actual = ParseBleScan(kRawData);
  ASSERT_TRUE(actual);
  EXPECT_EQ(0x42, actual->advertising_flags);
  EXPECT_EQ(0x1b, actual->tx_power);
  EXPECT_EQ("Hello", actual->advertisement_name);
  EXPECT_TRUE(actual->service_uuids.empty());
  EXPECT_TRUE(actual->service_data_map.empty());
  EXPECT_TRUE(actual->manufacturer_data_map.empty());
}

TEST(BleScanParserImplTest, ParseBleScanWithNonUtf8AdvertisementName) {
  const uint8_t kRawData[] = {
      // Length of the rest of the section, field type, data.
      // Advertising flag = 0x42
      0x02,
      0x01,
      0x42,
      // TX power = 0x1b
      0x02,
      0x0a,
      0x1b,
      // Local name 'U+1FFFE'
      0x05,
      0x08,
      // Invalid encoding of U+1FFFE (0x8F instead of 0x9F)
      0xF0,
      0x8F,
      0xBF,
      0xBE,
  };
  std::optional<ScanRecord> actual = ParseBleScan(kRawData);
  ASSERT_TRUE(actual);
  EXPECT_EQ(0x42, actual->advertising_flags);
  EXPECT_EQ(0x1b, actual->tx_power);
  EXPECT_EQ("\xF0\x8F\xBF\xBE", actual->advertisement_name);
  EXPECT_TRUE(actual->service_uuids.empty());
  EXPECT_TRUE(actual->service_data_map.empty());
  EXPECT_TRUE(actual->manufacturer_data_map.empty());
}

}  // namespace
}  // namespace bluez
