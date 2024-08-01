// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_DATA_TYPES_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_DATA_TYPES_H_

#include "base/time/time.h"

namespace network {
// A GeoHint represents a course location of a user. Values are based on
// RFC 8805 geolocation.
struct GeoHint {
  // Country code of the geo. Example: "US".
  std::string country_code;

  // ISO region of the geo. Example: "US-CA".
  std::string iso_region;

  // City name of the geo. Example: "MOUNTAIN VIEW".
  std::string city_name;

  bool operator==(const GeoHint& geo_hint) const = default;
};

// A blind-signed auth token, suitable for use with IP protection proxies.
struct BlindSignedAuthToken {
  // The token value, for inclusion in a header.
  std::string token;

  // The expiration time of this token.
  base::Time expiration;

  // The GeoHint which specifies the coarse geolocation of the token.
  GeoHint geo_hint;

  bool operator==(const BlindSignedAuthToken& token) const = default;
};

// The proxy layer to fetch batches of tokens for.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IpProtectionProxyLayer {
  kProxyA = 0,
  kProxyB = 1,
  kMaxValue = kProxyB
};
}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_DATA_TYPES_H_
