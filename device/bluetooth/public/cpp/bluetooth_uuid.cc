// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

#include <stddef.h>

#include <ostream>
#include <string>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <objbase.h>

#include "base/win/win_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace device {

namespace {

const char kCommonUuidPostfix[] = "-0000-1000-8000-00805f9b34fb";
const char kCommonUuidPrefix[] = "0000";

// Returns the canonical, 128-bit canonical, and the format of the UUID
// in |canonical|, |canonical_128|, and |format| based on |uuid|.
void GetCanonicalUuid(std::string uuid,
                      std::string* canonical,
                      std::string* canonical_128,
                      BluetoothUUID::Format* format) {
  // Initialize the values for the failure case.
  canonical->clear();
  canonical_128->clear();
  *format = BluetoothUUID::kFormatInvalid;

  if (uuid.empty())
    return;

  if (uuid.size() < 11 &&
      base::StartsWith(uuid, "0x", base::CompareCase::SENSITIVE)) {
    uuid = uuid.substr(2);
  }

  if (!(uuid.size() == 4 || uuid.size() == 8 || uuid.size() == 36))
    return;

  for (size_t i = 0; i < uuid.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (uuid[i] != '-')
        return;
    } else {
      if (!base::IsHexDigit(uuid[i]))
        return;
      uuid[i] = base::ToLowerASCII(uuid[i]);
    }
  }

  canonical->assign(uuid);
  if (uuid.size() == 4) {
    canonical_128->assign(kCommonUuidPrefix + uuid + kCommonUuidPostfix);
    *format = BluetoothUUID::kFormat16Bit;
  } else if (uuid.size() == 8) {
    canonical_128->assign(uuid + kCommonUuidPostfix);
    *format = BluetoothUUID::kFormat32Bit;
  } else {
    canonical_128->assign(uuid);
    *format = BluetoothUUID::kFormat128Bit;
  }
}

}  // namespace

BluetoothUUID::BluetoothUUID(const std::string& uuid) {
  GetCanonicalUuid(uuid, &value_, &canonical_value_, &format_);
}

BluetoothUUID::BluetoothUUID(base::span<const uint8_t> uuid_in_bytes) {
  if (uuid_in_bytes.size() != 16) {
    value_.clear();
    canonical_value_.clear();
    format_ = BluetoothUUID::kFormatInvalid;
    return;
  }

  canonical_value_.assign(base::StringPrintf(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      uuid_in_bytes[0], uuid_in_bytes[1], uuid_in_bytes[2], uuid_in_bytes[3],
      uuid_in_bytes[4], uuid_in_bytes[5], uuid_in_bytes[6], uuid_in_bytes[7],
      uuid_in_bytes[8], uuid_in_bytes[9], uuid_in_bytes[10], uuid_in_bytes[11],
      uuid_in_bytes[12], uuid_in_bytes[13], uuid_in_bytes[14],
      uuid_in_bytes[15]));
  value_.assign(canonical_value_);
  format_ = BluetoothUUID::kFormat128Bit;
}

#if BUILDFLAG(IS_WIN)
BluetoothUUID::BluetoothUUID(GUID uuid) {
  auto buffer = base::win::WStringFromGUID(uuid);
  DCHECK_EQ('{', buffer[0]);
  DCHECK_EQ('}', buffer[37]);

  GetCanonicalUuid(base::WideToUTF8(buffer.substr(1, 36)), &value_,
                   &canonical_value_, &format_);
  DCHECK_EQ(kFormat128Bit, format_);
}
#endif  // BUILDFLAG(IS_WIN)

BluetoothUUID::BluetoothUUID() : format_(kFormatInvalid) {}

BluetoothUUID::~BluetoothUUID() = default;

#if BUILDFLAG(IS_WIN)
// static
GUID BluetoothUUID::GetCanonicalValueAsGUID(std::string_view uuid) {
  DCHECK_EQ(36u, uuid.size());
  std::u16string braced_uuid = u'{' + base::UTF8ToUTF16(uuid) + u'}';
  GUID guid;
  CHECK_EQ(NOERROR, ::CLSIDFromString(base::as_wcstr(braced_uuid), &guid));
  return guid;
}
#endif  // BUILDFLAG(IS_WIN)

bool BluetoothUUID::IsValid() const {
  return format_ != kFormatInvalid;
}

std::vector<uint8_t> BluetoothUUID::GetBytes() const {
  if (!IsValid())
    return std::vector<uint8_t>();

  std::string_view input(canonical_value());

  std::vector<uint8_t> bytes(16);
  base::span<uint8_t> out(bytes);

  //           0         1         2         3
  //           012345678901234567890123456789012345
  // Example: "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
  //           12345678-1234-5678-9abc-def123456789
  bool success =
      (input.size() == 36) && (input[8] == '-') && (input[13] == '-') &&
      (input[18] == '-') && (input[23] == '-') &&
      base::HexStringToSpan(input.substr(0, 8), out.subspan<0, 4>()) &&
      base::HexStringToSpan(input.substr(9, 4), out.subspan<4, 2>()) &&
      base::HexStringToSpan(input.substr(14, 4), out.subspan<6, 2>()) &&
      base::HexStringToSpan(input.substr(19, 4), out.subspan<8, 2>()) &&
      base::HexStringToSpan(input.substr(24, 12), out.subspan<10, 6>());

  DCHECK(success);

  return bytes;
}

bool BluetoothUUID::operator<(const BluetoothUUID& uuid) const {
  return canonical_value_ < uuid.canonical_value_;
}

bool BluetoothUUID::operator==(const BluetoothUUID& uuid) const {
  return canonical_value_ == uuid.canonical_value_;
}

bool BluetoothUUID::operator!=(const BluetoothUUID& uuid) const {
  return canonical_value_ != uuid.canonical_value_;
}

std::ostream& operator<<(std::ostream& os, BluetoothUUID uuid) {
  return os << uuid.canonical_value();
}

}  // namespace device
