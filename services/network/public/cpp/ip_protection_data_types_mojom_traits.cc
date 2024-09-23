// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_protection_data_types_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {
bool StructTraits<network::mojom::GeoHintDataView, ip_protection::GeoHint>::
    Read(network::mojom::GeoHintDataView data, ip_protection::GeoHint* out) {
  return data.ReadCountryCode(&out->country_code) &&
         data.ReadIsoRegion(&out->iso_region) &&
         data.ReadCityName(&out->city_name);
}

bool StructTraits<network::mojom::BlindSignedAuthTokenDataView,
                  ip_protection::BlindSignedAuthToken>::
    Read(network::mojom::BlindSignedAuthTokenDataView data,
         ip_protection::BlindSignedAuthToken* out) {
  return data.ReadToken(&out->token) && data.ReadExpiration(&out->expiration) &&
         data.ReadGeoHint(&out->geo_hint);
}

}  // namespace mojo
