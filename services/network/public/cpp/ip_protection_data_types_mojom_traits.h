// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_IP_PROTECTION_DATA_TYPES_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_IP_PROTECTION_DATA_TYPES_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/time.mojom-shared.h"
#include "services/network/ip_protection/ip_protection_data_types.h"
#include "services/network/public/mojom/network_context.mojom-shared.h"

namespace mojo {
// Converts network::mojom::GeoHint to/from GeoHint,
// so that GeoHint can be used throughout the codebase without any
// direct reference to network::mojom::GeoHint.
template <>
struct StructTraits<network::mojom::GeoHintDataView, network::GeoHint> {
  static const std::string& country_code(const network::GeoHint& r) {
    return r.country_code;
  }
  static const std::string& iso_region(const network::GeoHint& r) {
    return r.iso_region;
  }
  static const std::string& city_name(const network::GeoHint& r) {
    return r.city_name;
  }

  // If Read() returns false, Mojo will discard the message.
  static bool Read(network::mojom::GeoHintDataView data, network::GeoHint* out);
};

// Converts network::mojom::BlindSignedAuthToken to/from BlindSignedAuthToken,
// so that BlindSignedAuthToken can be used throughout the codebase without any
// direct reference to network::mojom::BlindSignedAuthToken.
template <>
struct StructTraits<network::mojom::BlindSignedAuthTokenDataView,
                    network::BlindSignedAuthToken> {
  static const std::string& token(const network::BlindSignedAuthToken& r) {
    return r.token;
  }
  static const base::Time& expiration(const network::BlindSignedAuthToken& r) {
    return r.expiration;
  }
  static const network::GeoHint& geo_hint(
      const network::BlindSignedAuthToken& r) {
    return r.geo_hint;
  }

  // If Read() returns false, Mojo will discard the message.
  static bool Read(network::mojom::BlindSignedAuthTokenDataView data,
                   network::BlindSignedAuthToken* out);
};
}  // namespace mojo
#endif  // SERVICES_NETWORK_PUBLIC_CPP_IP_PROTECTION_DATA_TYPES_MOJOM_TRAITS_H_
