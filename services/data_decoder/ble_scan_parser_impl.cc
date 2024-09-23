// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "services/data_decoder/ble_scan_parser_impl.h"

namespace data_decoder {

// Definitions of the data type flags:
// https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/
const int kDataTypeFlags = 0x01;
const int kDataTypeServiceUuids16BitPartial = 0x02;
const int kDataTypeServiceUuids16BitComplete = 0x03;
const int kDataTypeServiceUuids32BitPartial = 0x04;
const int kDataTypeServiceUuids32BitComplete = 0x05;
const int kDataTypeServiceUuids128BitPartial = 0x06;
const int kDataTypeServiceUuids128BitComplete = 0x07;
const int kDataTypeLocalNameShort = 0x08;
const int kDataTypeLocalNameComplete = 0x09;
const int kDataTypeTxPowerLevel = 0x0A;
const int kDataTypeServiceData = 0x16;
const int kDataTypeManufacturerData = 0xFF;

const char kUuidPrefix[] = "0000";
const char kUuidSuffix[] = "-0000-1000-8000-00805F9B34FB";

BleScanParserImpl::BleScanParserImpl() = default;

BleScanParserImpl::~BleScanParserImpl() = default;

void BleScanParserImpl::Parse(const std::vector<uint8_t>& advertisement_data,
                              ParseCallback callback) {
  std::move(callback).Run(ParseBleScan(advertisement_data));
}

mojom::ScanRecordPtr BleScanParserImpl::ParseBleScan(
    base::span<const uint8_t> advertisement_data) {
  uint8_t tx_power;
  std::string advertisement_name;
  std::vector<device::BluetoothUUID> service_uuids;
  base::flat_map<std::string, std::vector<uint8_t>> service_data_map;
  base::flat_map<uint16_t, std::vector<uint8_t>> manufacturer_data_map;

  int advertising_flags = -1;

  // A reference for BLE advertising data: https://bit.ly/2DUTnsk
  for (size_t i = 0; i < advertisement_data.size();) {
    uint8_t length = advertisement_data[i++];
    if (length <= 1 || length > advertisement_data.size() - i) {
      return nullptr;
    }

    // length includes the field_type byte.
    uint8_t data_length = length - 1;
    uint8_t field_type = advertisement_data[i++];

    switch (field_type) {
      case kDataTypeFlags:
        advertising_flags = advertisement_data[i];
        break;
      case kDataTypeServiceUuids16BitPartial:
      case kDataTypeServiceUuids16BitComplete:
        if (!ParseServiceUuids(advertisement_data.subspan(i, data_length),
                               UuidFormat::kFormat16Bit, &service_uuids)) {
          return nullptr;
        }
        break;
      case kDataTypeServiceUuids32BitPartial:
      case kDataTypeServiceUuids32BitComplete:
        if (!ParseServiceUuids(advertisement_data.subspan(i, data_length),
                               UuidFormat::kFormat32Bit, &service_uuids)) {
          return nullptr;
        }
        break;
      case kDataTypeServiceUuids128BitPartial:
      case kDataTypeServiceUuids128BitComplete:
        if (!ParseServiceUuids(advertisement_data.subspan(i, data_length),
                               UuidFormat::kFormat128Bit, &service_uuids)) {
          return nullptr;
        }
        break;
      case kDataTypeLocalNameShort:
      case kDataTypeLocalNameComplete: {
        base::span<const uint8_t> s =
            advertisement_data.subspan(i, data_length);
        advertisement_name = std::string(s.begin(), s.end());
        break;
      }
      case kDataTypeTxPowerLevel:
        tx_power = advertisement_data[i];
        break;
      case kDataTypeServiceData: {
        if (data_length < 4) {
          return nullptr;
        }

        base::span<const uint8_t> uuid = advertisement_data.subspan(i, 2);
        base::span<const uint8_t> data =
            advertisement_data.subspan(i + 2, data_length - 2);
        service_data_map[ParseUuid(uuid, UuidFormat::kFormat16Bit)] =
            std::vector<uint8_t>(data.begin(), data.end());
        break;
      }
      case kDataTypeManufacturerData: {
        if (data_length < 4) {
          return nullptr;
        }

        uint16_t manufacturer_key = (advertisement_data[i + 1] << 8);
        manufacturer_key += advertisement_data[i];
        base::span<const uint8_t> s =
            advertisement_data.subspan(i + 2, data_length - 2);
        manufacturer_data_map[manufacturer_key] =
            std::vector<uint8_t>(s.begin(), s.end());
        break;
      }
      default:
        // Just ignore. We don't handle other data types.
        break;
    }

    i += data_length;
  }

  return mojom::ScanRecord::New(advertising_flags, tx_power, advertisement_name,
                                service_uuids, service_data_map,
                                manufacturer_data_map);
}

std::string BleScanParserImpl::ParseUuid(base::span<const uint8_t> bytes,
                                         UuidFormat format) {
  size_t length = bytes.size();
  if (!(format == UuidFormat::kFormat16Bit && length == 2) &&
      !(format == UuidFormat::kFormat32Bit && length == 4) &&
      !(format == UuidFormat::kFormat128Bit && length == 16)) {
    return std::string();
  }

  std::vector<uint8_t> reversed(bytes.rbegin(), bytes.rend());
  std::string uuid = base::HexEncode(reversed);

  switch (format) {
    case UuidFormat::kFormat16Bit:
      return kUuidPrefix + uuid + kUuidSuffix;
    case UuidFormat::kFormat32Bit:
      return uuid + kUuidSuffix;
    case UuidFormat::kFormat128Bit:
      uuid.insert(8, 1, '-');
      uuid.insert(13, 1, '-');
      uuid.insert(18, 1, '-');
      uuid.insert(23, 1, '-');
      return uuid;
    case UuidFormat::kFormatInvalid:
      NOTREACHED_IN_MIGRATION();
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

bool BleScanParserImpl::ParseServiceUuids(
    base::span<const uint8_t> bytes,
    UuidFormat format,
    std::vector<device::BluetoothUUID>* service_uuids) {
  int uuid_length = 0;
  switch (format) {
    case UuidFormat::kFormat16Bit:
      uuid_length = 2;
      break;
    case UuidFormat::kFormat32Bit:
      uuid_length = 4;
      break;
    case UuidFormat::kFormat128Bit:
      uuid_length = 16;
      break;
    case UuidFormat::kFormatInvalid:
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  if (bytes.size() % uuid_length != 0) {
    return false;
  }

  for (size_t start = 0; start < bytes.size(); start += uuid_length) {
    service_uuids->push_back(device::BluetoothUUID(
        ParseUuid(bytes.subspan(start, uuid_length), format)));
  }

  return true;
}

}  // namespace data_decoder
