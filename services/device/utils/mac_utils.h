// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_UTILS_MAC_UTILS_H_
#define SERVICES_DEVICE_UTILS_MAC_UTILS_H_

#include "base/apple/foundation_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

std::string HexErrorCode(IOReturn error_code);

template <class T>
absl::optional<T> GetIntegerProperty(io_service_t service,
                                     CFStringRef property) {
  static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                    std::is_same_v<T, int32_t>,
                "Unsupported template type");

  base::apple::ScopedCFTypeRef<CFNumberRef> cf_number(
      base::apple::CFCast<CFNumberRef>(IORegistryEntryCreateCFProperty(
          service, property, kCFAllocatorDefault, 0)));

  if (!cf_number)
    return absl::nullopt;
  if (CFGetTypeID(cf_number) != CFNumberGetTypeID())
    return absl::nullopt;

  T value;
  CFNumberType type;
  // Note below we use CFNumber signed type even when the template type is
  // unsigned type, this is due to no unsigned integer support in CFNumber.
  if constexpr (std::is_same_v<T, uint8_t>)
    type = kCFNumberSInt8Type;
  else if constexpr (std::is_same_v<T, uint16_t>)
    type = kCFNumberSInt16Type;
  else if constexpr (std::is_same_v<T, int32_t>)
    type = kCFNumberSInt32Type;
  else {
    NOTREACHED();
    return absl::nullopt;
  }
  if (!CFNumberGetValue(static_cast<CFNumberRef>(cf_number), type, &value))
    return absl::nullopt;
  return value;
}

template <class T>
absl::optional<T> GetStringProperty(io_service_t service,
                                    CFStringRef property) {
  static_assert(
      std::is_same_v<T, std::string> || std::is_same_v<T, std::u16string>,
      "Unsupported template type");

  base::apple::ScopedCFTypeRef<CFStringRef> ref(
      base::apple::CFCast<CFStringRef>(IORegistryEntryCreateCFProperty(
          service, property, kCFAllocatorDefault, 0)));

  if (!ref)
    return absl::nullopt;

  if constexpr (std::is_same_v<T, std::string>)
    return base::SysCFStringRefToUTF8(ref);
  if constexpr (std::is_same_v<T, std::u16string>)
    return base::SysCFStringRefToUTF16(ref);

  NOTREACHED();
  return absl::nullopt;
}
}  // namespace device

#endif  // SERVICES_DEVICE_UTILS_MAC_UTILS_H_
