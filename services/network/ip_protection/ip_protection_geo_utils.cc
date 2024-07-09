// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection/ip_protection_geo_utils.h"

#include <string>

#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

std::string GetGeoIdFromGeoHint(network::mojom::GeoHintPtr geo_hint) {
  if (!geo_hint) {
    return "";  // If nullptr, return empty string.
  }

  std::string geo_id = geo_hint->country_code;
  if (!geo_hint->iso_region.empty()) {
    geo_id += "," + geo_hint->iso_region;
  }
  if (!geo_hint->city_name.empty()) {
    geo_id += "," + geo_hint->city_name;
  }

  return geo_id;
}

network::mojom::GeoHintPtr GetGeoHintFromGeoIdForTesting(
    const std::string& geo_id) {
  if (geo_id.empty()) {
    return nullptr;  // Return nullptr if the geo_id is empty.
  }
  auto geo_hint = network::mojom::GeoHint::New();
  std::stringstream geo_id_stream(geo_id);
  std::string segment;

  // Extract country code.
  if (std::getline(geo_id_stream, segment, ',')) {
    geo_hint->country_code = segment;
  }

  // Extract ISO region.
  if (std::getline(geo_id_stream, segment, ',')) {
    geo_hint->iso_region = segment;
  }

  // Extract city name.
  if (std::getline(geo_id_stream, segment, ',')) {
    geo_hint->city_name = segment;
  }

  return geo_hint;
}

}  // namespace network
