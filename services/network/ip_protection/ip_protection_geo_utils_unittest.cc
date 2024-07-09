// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection/ip_protection_geo_utils.h"

#include <string>

#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

class IpProtectionGeoUtilsTest : public testing::Test {};

TEST_F(IpProtectionGeoUtilsTest, GetGeoIdFromGeoHint_ValidInput) {
  network::mojom::GeoHintPtr geo_hint =
      network::mojom::GeoHint::New("US", "US-CA", "MOUNTAIN VIEW");

  std::string geo_id = GetGeoIdFromGeoHint(std::move(geo_hint));
  EXPECT_EQ(geo_id, "US,US-CA,MOUNTAIN VIEW");
}

TEST_F(IpProtectionGeoUtilsTest, GetGeoIdFromGeoHint_CountryCodeOnly) {
  network::mojom::GeoHintPtr geo_hint = network::mojom::GeoHint::New();
  geo_hint->country_code = "US";

  std::string geo_id = GetGeoIdFromGeoHint(std::move(geo_hint));
  EXPECT_EQ(geo_id, "US");
}

TEST_F(IpProtectionGeoUtilsTest, GetGeoIdFromGeoHint_EmptyGeoHintPtr) {
  network::mojom::GeoHintPtr geo_hint = network::mojom::GeoHint::New();

  std::string geo_id = GetGeoIdFromGeoHint(std::move(geo_hint));
  EXPECT_EQ(geo_id, "");
}

TEST_F(IpProtectionGeoUtilsTest, GetGeoIdFromGeoHint_NullGeoHintPtr) {
  std::string geo_id = GetGeoIdFromGeoHint(nullptr);
  EXPECT_EQ(geo_id, "");
}

TEST_F(IpProtectionGeoUtilsTest, GetGeoHintFromGeoIdForTesting_CompleteGeoId) {
  network::mojom::GeoHintPtr geo_hint =
      GetGeoHintFromGeoIdForTesting("US,US-CA,MOUNTAIN VIEW");
  EXPECT_TRUE(geo_hint.Equals(
      network::mojom::GeoHint::New("US", "US-CA", "MOUNTAIN VIEW")));
}

TEST_F(IpProtectionGeoUtilsTest,
       GetGeoHintFromGeoIdForTesting_CountryOnlyGeoId) {
  network::mojom::GeoHintPtr geo_hint = GetGeoHintFromGeoIdForTesting("US");
  auto expected_geo_hint = network::mojom::GeoHint::New();
  expected_geo_hint->country_code = "US";
  EXPECT_TRUE(geo_hint.Equals(expected_geo_hint));
}

TEST_F(IpProtectionGeoUtilsTest, GetGeoHintFromGeoIdForTesting_EmptyGeoId) {
  network::mojom::GeoHintPtr geo_hint = GetGeoHintFromGeoIdForTesting("");
  EXPECT_TRUE(geo_hint.is_null());
}

}  // namespace network
