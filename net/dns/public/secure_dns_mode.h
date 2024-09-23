// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_PUBLIC_SECURE_DNS_MODE_H_
#define NET_DNS_PUBLIC_SECURE_DNS_MODE_H_

#include <string_view>

#include "base/containers/fixed_flat_map.h"

namespace net {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
// The SecureDnsMode specifies what types of lookups (secure/insecure) should
// be performed and in what order when resolving a specific query. The int
// values should not be changed as they are logged.
enum class SecureDnsMode : int {
  // In OFF mode, no DoH lookups should be performed.
  kOff = 0,
  // In AUTOMATIC mode, DoH lookups should be performed first if DoH is
  // available, and insecure DNS lookups should be performed as a fallback.
  kAutomatic = 1,
  // In SECURE mode, only DoH lookups should be performed.
  kSecure = 2,
};

inline constexpr auto kSecureDnsModes =
    base::MakeFixedFlatMap<SecureDnsMode, std::string_view>(
        {{SecureDnsMode::kOff, "Off"},
         {SecureDnsMode::kAutomatic, "Automatic"},
         {SecureDnsMode::kSecure, "Secure"}});

}  // namespace net

#endif  // NET_DNS_PUBLIC_SECURE_DNS_MODE_H_
