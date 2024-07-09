// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_GEO_UTILS_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_GEO_UTILS_H_

#include <string>

#include "base/component_export.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace network {
// GeoId is a string representation of a mojo GeoHint. A GeoId is constructed by
// concatenating values of the GeoHint in order of increasing granularity.
// If a finer granularity is missing, a trailing commas is not appended.
// Ex. mojo GeoHint{"US", "US-CA", "MOUNTAIN VIEW"} => "US,US-CA,MOUNTAIN VIEW".
// Ex. mojo GeoHint{"US"} => "US"

// Returns a formatted version of the mojo GeoHint. In the case
// of a nullptr or empty `GeoHintPtr`, an empty string will be returned.
COMPONENT_EXPORT(NETWORK_SERVICE)
std::string GetGeoIdFromGeoHint(network::mojom::GeoHintPtr geo_hint);

// Constructs a mojo GeoHint from a GeoId string. The function requires a
// correctly formatted GeoId string. It DOES NOT handle invalid formats.
COMPONENT_EXPORT(NETWORK_SERVICE)
network::mojom::GeoHintPtr GetGeoHintFromGeoIdForTesting(
    const std::string& geo_id);

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_GEO_UTILS_H_
