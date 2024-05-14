// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_UTILS_MAC_UTILS_H_
#define SERVICES_DEVICE_UTILS_MAC_UTILS_H_

#include <optional>

#include "base/apple/foundation_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"

namespace device {

std::string HexErrorCode(IOReturn error_code);

template <class T>
std::optional<T> GetIntegerProperty(io_service_t service,
                                    CFStringRef property) {
  static_assert(std::is_same_v<T, uint8_t> || std::is_same_v<T, uint16_t> ||
                    std::is_same_v<T, int32_t>,
                "Unsupported template type");

  base::apple::ScopedCFTypeRef<CFNumberRef> cf_number(
      base::apple::CFCast<CFNumberRef>(IORegistryEntryCreateCFProperty(
          service, property, kCFAllocatorDefault, 0)));

  if (!cf_number)
    return std::nullopt;
  if (CFGetTypeID(cf_number.get()) != CFNumberGetTypeID()) {
    return std::nullopt;
  }

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
    NOTREACHED_IN_MIGRATION();
    return std::nullopt;
  }
  if (!CFNumberGetValue(static_cast<CFNumberRef>(cf_number.get()), type,
                        &value)) {
    return std::nullopt;
  }
  return value;
}

template <class T>
std::optional<T> GetStringProperty(io_service_t service, CFStringRef property) {
  static_assert(
      std::is_same_v<T, std::string> || std::is_same_v<T, std::u16string>,
      "Unsupported template type");

  base::apple::ScopedCFTypeRef<CFStringRef> ref(
      base::apple::CFCast<CFStringRef>(IORegistryEntryCreateCFProperty(
          service, property, kCFAllocatorDefault, 0)));

  if (!ref)
    return std::nullopt;

  if constexpr (std::is_same_v<T, std::string>)
    return base::SysCFStringRefToUTF8(ref.get());
  if constexpr (std::is_same_v<T, std::u16string>)
    return base::SysCFStringRefToUTF16(ref.get());

  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}
}  // namespace device

#endif  // SERVICES_DEVICE_UTILS_MAC_UTILS_H_
