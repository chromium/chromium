// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/ble_scan_parser_impl.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace data_decoder {

template <typename Traits>
class BleScanParserImplTest : public testing::Test {
 protected:
  // These helpers could be static, but at some point in the future, calling a
  // static method via -> might be a clang-tidy warning. The alternative is
  // writing out BleScanParserImplTest::ParseUuid, et cetera, and nobody wants
  // that.
  device::BluetoothUUID ParseUuid(base::span<const uint8_t> bytes,
                                  UuidFormat format) {
    return Traits::ParseUuid(bytes, format);
  }

  bool ParseServiceUuids(base::span<const uint8_t> bytes,
                         UuidFormat format,
                         std::vector<device::BluetoothUUID>& out) {
    return Traits::ParseServiceUuids(bytes, format, out);
  }

  mojom::ScanRecordPtr ParseBleScan(base::span<const uint8_t> bytes) {
    return Traits::ParseBleScan(bytes);
  }
};

struct CxxParserTraits {
  static device::BluetoothUUID ParseUuid(base::span<const uint8_t> bytes,
                                         UuidFormat format) {
    return BleScanParserImpl::ParseUuid(bytes, format);
  }

  static bool ParseServiceUuids(base::span<const uint8_t> bytes,
                                UuidFormat format,
                                std::vector<device::BluetoothUUID>& out) {
    return BleScanParserImpl::ParseServiceUuids(bytes, format, &out);
  }

  static mojom::ScanRecordPtr ParseBleScan(base::span<const uint8_t> bytes) {
    return BleScanParserImpl::ParseBleScan(bytes);
  }
};

using ParserImpls = ::testing::Types<CxxParserTraits>;
TYPED_TEST_SUITE(BleScanParserImplTest, ParserImpls);

TYPED_TEST(BleScanParserImplTest, ParseBadUuidLengthReturnsEmptyString) {
  std::vector<uint8_t> bad_uuid(0xab, 5);
  EXPECT_FALSE(this->ParseUuid(bad_uuid, UuidFormat::kFormat16Bit).IsValid());
  EXPECT_FALSE(this->ParseUuid(bad_uuid, UuidFormat::kFormat32Bit).IsValid());
  EXPECT_FALSE(this->ParseUuid(bad_uuid, UuidFormat::kFormat128Bit).IsValid());
}

TYPED_TEST(BleScanParserImplTest, Parse16BitUuid) {
  const uint8_t kUuid16[] = {0xab, 0xcd};
  const device::BluetoothUUID kExpected("0000CDAB-0000-1000-8000-00805F9B34FB");
  EXPECT_EQ(kExpected, this->ParseUuid(kUuid16, UuidFormat::kFormat16Bit));
}

TYPED_TEST(BleScanParserImplTest, Parse32BitUuid) {
  const uint8_t kUuid32[] = {0xab, 0xcd, 0xef, 0x01};
  const device::BluetoothUUID kExpected("01EFCDAB-0000-1000-8000-00805F9B34FB");
  EXPECT_EQ(kExpected, this->ParseUuid(kUuid32, UuidFormat::kFormat32Bit));
}

TYPED_TEST(BleScanParserImplTest, Parse128BitUuid) {
  const uint8_t kUuid128[] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89,
                              0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};
  const device::BluetoothUUID kExpected("89674523-01EF-CDAB-8967-452301EFCDAB");
  EXPECT_EQ(kExpected, this->ParseUuid(kUuid128, UuidFormat::kFormat128Bit));
}

TYPED_TEST(BleScanParserImplTest, Parse16BitServiceUuids) {
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
  EXPECT_TRUE(
      this->ParseServiceUuids(kUuids, UuidFormat::kFormat16Bit, actual));
  EXPECT_EQ(expected, actual);
}

TYPED_TEST(BleScanParserImplTest, Parse32BitServiceUuids) {
  std::vector<device::BluetoothUUID> expected = {
      device::BluetoothUUID("01EFCDAB-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("89674523-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("01EFCDAB-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("89674523-0000-1000-8000-00805F9B34FB"),
  };

  const uint8_t kUuids[] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89,
                            0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};

  std::vector<device::BluetoothUUID> actual;
  EXPECT_TRUE(
      this->ParseServiceUuids(kUuids, UuidFormat::kFormat32Bit, actual));
  EXPECT_EQ(expected, actual);
}

TYPED_TEST(BleScanParserImplTest, Parse128BitServiceUuids) {
  std::vector<device::BluetoothUUID> expected = {
      device::BluetoothUUID("89674523-01EF-CDAB-8967-452301EFCDAB"),
      device::BluetoothUUID("89674523-01EF-CDAB-01EF-CDAB89674523"),
  };

  const uint8_t kUuids[] = {0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89,
                            0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89,
                            0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01,
                            0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89};

  std::vector<device::BluetoothUUID> actual;
  EXPECT_TRUE(
      this->ParseServiceUuids(kUuids, UuidFormat::kFormat128Bit, actual));
  EXPECT_EQ(expected, actual);
}

TYPED_TEST(BleScanParserImplTest, ParseBleAdvertisingScan) {
  std::vector<device::BluetoothUUID> expected_service_uuids = {
      device::BluetoothUUID("0000ABCD-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("0000EF01-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("ABCDEF01-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("23456789-0000-1000-8000-00805F9B34FB"),
      device::BluetoothUUID("ABCDEF01-2345-6789-ABCD-EF0123456789"),
  };

  std::vector<uint8_t> service_data = {0xa1, 0xb2, 0xc3, 0xd4, 0xe5};
  base::flat_map<device::BluetoothUUID, std::vector<uint8_t>>
      expected_service_data_map;
  expected_service_data_map[device::BluetoothUUID(
      "0000DCAB-0000-1000-8000-00805F9B34FB")] = service_data;

  std::vector<uint8_t> manufacturer_data = {0x1a, 0x2b, 0x3c, 0x4d};
  base::flat_map<uint16_t, std::vector<uint8_t>> expected_manufacturer_data_map;
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

  mojom::ScanRecordPtr actual = this->ParseBleScan(kRawData);
  ASSERT_TRUE(actual);
  EXPECT_EQ(0x42, actual->advertising_flags);
  EXPECT_EQ(0x1b, actual->tx_power);
  EXPECT_EQ("Steve", actual->advertisement_name);
  EXPECT_EQ(expected_service_uuids, actual->service_uuids);
  EXPECT_EQ(expected_service_data_map, actual->service_data_map);
  EXPECT_EQ(expected_manufacturer_data_map, actual->manufacturer_data_map);
}

}  // namespace data_decoder
