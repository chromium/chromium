// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

#include <stddef.h>

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

#if BUILDFLAG(IS_WIN)
#include <objbase.h>

#include <array>
#endif  // BUILDFLAG(IS_WIN)

namespace device {

namespace {

constexpr std::string_view kCommonUuidPostfix = "-0000-1000-8000-00805f9b34fb";
constexpr std::string_view kCommonUuidPrefix = "0000";

// Returns the canonical, 128-bit canonical, and the format of the UUID
// in |canonical|, |canonical_128|, and |format| based on |uuid|.
void GetCanonicalUuid(std::string_view uuid,
                      std::string* canonical,
                      std::string* canonical_128,
                      BluetoothUUID::Format* format) {
  // Initialize the values for the failure case.
  canonical->clear();
  canonical_128->clear();
  *format = BluetoothUUID::kFormatInvalid;

  if (uuid.empty()) {
    return;
  }

  if (uuid.size() < 11 && uuid.starts_with("0x")) {
    uuid.remove_prefix(2);
  }

  if (!(uuid.size() == 4 || uuid.size() == 8 || uuid.size() == 36)) {
    return;
  }

  for (size_t i = 0; i < uuid.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (uuid[i] != '-') {
        return;
      }
    } else if (!base::IsHexDigit(uuid[i])) {
      return;
    }
  }

  if (uuid.size() == 4) {
    *canonical = base::ToLowerASCII(uuid);
    *canonical_128 =
        base::StrCat({kCommonUuidPrefix, *canonical, kCommonUuidPostfix});
    *format = BluetoothUUID::kFormat16Bit;
  } else if (uuid.size() == 8) {
    *canonical = base::ToLowerASCII(uuid);
    *canonical_128 = base::StrCat({*canonical, kCommonUuidPostfix});
    *format = BluetoothUUID::kFormat32Bit;
  } else {
    *canonical_128 = base::ToLowerASCII(uuid);
    *format = BluetoothUUID::kFormat128Bit;
  }
}

}  // namespace

BluetoothUUID::BluetoothUUID(std::string_view uuid) {
  GetCanonicalUuid(uuid, &value_, &canonical_value_, &format_);
}

BluetoothUUID::BluetoothUUID(base::span<const uint8_t> uuid_in_bytes) {
  if (uuid_in_bytes.size() != 16) {
    return;
  }

  canonical_value_ = absl::StrFormat(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      uuid_in_bytes[0], uuid_in_bytes[1], uuid_in_bytes[2], uuid_in_bytes[3],
      uuid_in_bytes[4], uuid_in_bytes[5], uuid_in_bytes[6], uuid_in_bytes[7],
      uuid_in_bytes[8], uuid_in_bytes[9], uuid_in_bytes[10], uuid_in_bytes[11],
      uuid_in_bytes[12], uuid_in_bytes[13], uuid_in_bytes[14],
      uuid_in_bytes[15]);
  format_ = BluetoothUUID::kFormat128Bit;
}

#if BUILDFLAG(IS_WIN)
BluetoothUUID::BluetoothUUID(const GUID& uuid)
    : format_(kFormat128Bit),
      canonical_value_(
          absl::StrFormat("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                          uuid.Data1,
                          uuid.Data2,
                          uuid.Data3,
                          uuid.Data4[0],
                          uuid.Data4[1],
                          uuid.Data4[2],
                          uuid.Data4[3],
                          uuid.Data4[4],
                          uuid.Data4[5],
                          uuid.Data4[6],
                          uuid.Data4[7])) {}
#endif  // BUILDFLAG(IS_WIN)

BluetoothUUID::BluetoothUUID() = default;
BluetoothUUID::BluetoothUUID(const BluetoothUUID&) = default;
BluetoothUUID& BluetoothUUID::operator=(const BluetoothUUID&) = default;

BluetoothUUID::BluetoothUUID(BluetoothUUID&& other) noexcept
    : format_(std::exchange(other.format_, kFormatInvalid)),
      value_(std::exchange(other.value_, std::string())),
      canonical_value_(std::exchange(other.canonical_value_, std::string())) {}

BluetoothUUID& BluetoothUUID::operator=(BluetoothUUID&& other) noexcept {
  if (this != &other) {
    format_ = std::exchange(other.format_, kFormatInvalid);
    value_ = std::exchange(other.value_, std::string());
    canonical_value_ = std::exchange(other.canonical_value_, std::string());
  }
  return *this;
}

BluetoothUUID::~BluetoothUUID() = default;

#if BUILDFLAG(IS_WIN)
// static
GUID BluetoothUUID::GetCanonicalValueAsGUID(std::string_view uuid) {
  CHECK_EQ(36u, uuid.size());
  std::array<wchar_t, 39> braced_uuid;
  braced_uuid[0] = L'{';
  for (size_t i = 0; i < uuid.size(); ++i) {
    braced_uuid[i + 1] =
        static_cast<wchar_t>(static_cast<unsigned char>(uuid[i]));
  }
  braced_uuid[37] = L'}';
  braced_uuid[38] = L'\0';
  GUID guid;
  CHECK_EQ(NOERROR, ::CLSIDFromString(braced_uuid.data(), &guid));
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

std::optional<uint16_t> BluetoothUUID::Get16BitValue() const {
  std::string_view canonical(canonical_value_);
  if (!canonical.starts_with(kCommonUuidPrefix) ||
      !canonical.ends_with(kCommonUuidPostfix)) {
    return std::nullopt;
  }
  uint32_t value;
  if (!base::HexStringToUInt(canonical.substr(kCommonUuidPrefix.size(), 4),
                             &value)) {
    return std::nullopt;
  }
  return static_cast<uint16_t>(value);
}

std::ostream& operator<<(std::ostream& os, const BluetoothUUID& uuid) {
  return os << uuid.canonical_value();
}

}  // namespace device
