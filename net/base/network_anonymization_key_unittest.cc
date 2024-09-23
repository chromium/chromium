// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_anonymization_key.h"

#include <optional>

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "network_anonymization_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace net {

class NetworkAnonymizationKeyTest : public testing::Test {
 protected:
  const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
  const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));
  const SchemefulSite kDataSite = SchemefulSite(GURL("data:foo"));
  const base::UnguessableToken kNonce = base::UnguessableToken::Create();
};

TEST_F(NetworkAnonymizationKeyTest, CreateFromNetworkIsolationKey) {
  SchemefulSite site_a = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite site_b = SchemefulSite(GURL("http://b.test/"));
  SchemefulSite opaque = SchemefulSite(url::Origin());
  base::UnguessableToken nik_nonce = base::UnguessableToken::Create();

  NetworkIsolationKey populated_cross_site_nik(site_a, site_b, nik_nonce);
  NetworkIsolationKey populated_same_site_nik(site_a, site_a, nik_nonce);
  NetworkIsolationKey populated_same_site_opaque_nik(opaque, opaque, nik_nonce);
  NetworkIsolationKey empty_nik;

  NetworkAnonymizationKey nak_from_same_site_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          populated_same_site_nik);
  NetworkAnonymizationKey nak_from_cross_site_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          populated_cross_site_nik);
  NetworkAnonymizationKey nak_from_same_site_opaque_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(
          populated_same_site_opaque_nik);
  NetworkAnonymizationKey nak_from_empty_nik =
      NetworkAnonymizationKey::CreateFromNetworkIsolationKey(empty_nik);

  // NAKs created when there is no top frame site on the NIK should create an
  // empty NAK.
  EXPECT_TRUE(nak_from_empty_nik.IsEmpty());

  // Top site should be populated correctly.
  EXPECT_EQ(nak_from_same_site_nik.GetTopFrameSite(), site_a);
  EXPECT_EQ(nak_from_cross_site_nik.GetTopFrameSite(), site_a);
  EXPECT_EQ(nak_from_same_site_opaque_nik.GetTopFrameSite(), opaque);

  // Nonce should be populated correctly.
  EXPECT_EQ(nak_from_same_site_nik.GetNonce(), nik_nonce);
  EXPECT_EQ(nak_from_cross_site_nik.GetNonce(), nik_nonce);
  EXPECT_EQ(nak_from_same_site_opaque_nik.GetNonce(), nik_nonce);

  // Is cross site boolean should be populated correctly.
  EXPECT_TRUE(nak_from_same_site_nik.IsSameSite());
  EXPECT_TRUE(nak_from_cross_site_nik.IsCrossSite());
  EXPECT_TRUE(nak_from_same_site_opaque_nik.IsSameSite());

  // Double-keyed + cross site bit NAKs created from different third party
  // cross site contexts should be the different.
  EXPECT_FALSE(nak_from_same_site_nik == nak_from_cross_site_nik);
}

TEST_F(NetworkAnonymizationKeyTest, CreateSameSite) {
  SchemefulSite site = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite opaque = SchemefulSite(url::Origin());
  NetworkAnonymizationKey key;

  key = NetworkAnonymizationKey::CreateSameSite(site);
  EXPECT_EQ(key.GetTopFrameSite(), site);
  EXPECT_FALSE(key.GetNonce().has_value());
  EXPECT_TRUE(key.IsSameSite());

  key = NetworkAnonymizationKey::CreateSameSite(opaque);
  EXPECT_EQ(key.GetTopFrameSite(), opaque);
  EXPECT_FALSE(key.GetNonce().has_value());
  EXPECT_TRUE(key.IsSameSite());
}

TEST_F(NetworkAnonymizationKeyTest, CreateCrossSite) {
  SchemefulSite site = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite opaque = SchemefulSite(url::Origin());
  NetworkAnonymizationKey key;

  key = NetworkAnonymizationKey::CreateCrossSite(site);
  EXPECT_EQ(key.GetTopFrameSite(), site);
  EXPECT_FALSE(key.GetNonce().has_value());
  EXPECT_TRUE(key.IsCrossSite());

  key = NetworkAnonymizationKey::CreateCrossSite(opaque);
  EXPECT_EQ(key.GetTopFrameSite(), opaque);
  EXPECT_FALSE(key.GetNonce().has_value());
  EXPECT_TRUE(key.IsCrossSite());
}

TEST_F(NetworkAnonymizationKeyTest, CreateFromFrameSite) {
  SchemefulSite site_a = SchemefulSite(GURL("http://a.test/"));
  SchemefulSite site_b = SchemefulSite(GURL("http://b.test/"));
  SchemefulSite opaque_1 = SchemefulSite(url::Origin());
  SchemefulSite opaque_2 = SchemefulSite(url::Origin());
  base::UnguessableToken nonce = base::UnguessableToken::Create();

  NetworkAnonymizationKey nak_from_same_site =
      NetworkAnonymizationKey::CreateFromFrameSite(site_a, site_a, nonce);
  NetworkAnonymizationKey nak_from_cross_site =
      NetworkAnonymizationKey::CreateFromFrameSite(site_a, site_b, nonce);
  NetworkAnonymizationKey nak_from_same_site_opaque =
      NetworkAnonymizationKey::CreateFromFrameSite(opaque_1, opaque_1, nonce);
  NetworkAnonymizationKey nak_from_cross_site_opaque =
      NetworkAnonymizationKey::CreateFromFrameSite(opaque_1, opaque_2, nonce);

  // Top site should be populated correctly.
  EXPECT_EQ(nak_from_same_site.GetTopFrameSite(), site_a);
  EXPECT_EQ(nak_from_cross_site.GetTopFrameSite(), site_a);
  EXPECT_EQ(nak_from_same_site_opaque.GetTopFrameSite(), opaque_1);
  EXPECT_EQ(nak_from_cross_site_opaque.GetTopFrameSite(), opaque_1);

  // Nonce should be populated correctly.
  EXPECT_EQ(nak_from_same_site.GetNonce(), nonce);
  EXPECT_EQ(nak_from_cross_site.GetNonce(), nonce);
  EXPECT_EQ(nak_from_same_site_opaque.GetNonce(), nonce);
  EXPECT_EQ(nak_from_cross_site_opaque.GetNonce(), nonce);

  // Is cross site boolean should be populated correctly.
  EXPECT_TRUE(nak_from_same_site.IsSameSite());
  EXPECT_TRUE(nak_from_cross_site.IsCrossSite());
  EXPECT_TRUE(nak_from_same_site_opaque.IsSameSite());
  EXPECT_TRUE(nak_from_cross_site_opaque.IsCrossSite());

  // NAKs created from different third party cross site contexts should be
  // different.
  EXPECT_NE(nak_from_same_site, nak_from_cross_site);
  EXPECT_NE(nak_from_same_site_opaque, nak_from_cross_site_opaque);
}

TEST_F(NetworkAnonymizationKeyTest, IsEmpty) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/std::nullopt);

  EXPECT_TRUE(empty_key.IsEmpty());
  EXPECT_FALSE(populated_key.IsEmpty());
}

TEST_F(NetworkAnonymizationKeyTest, CreateTransient) {
  NetworkAnonymizationKey transient_key1 =
      NetworkAnonymizationKey::CreateTransient();
  NetworkAnonymizationKey transient_key2 =
      NetworkAnonymizationKey::CreateTransient();

  EXPECT_TRUE(transient_key1.IsTransient());
  EXPECT_TRUE(transient_key2.IsTransient());
  EXPECT_FALSE(transient_key1 == transient_key2);
}

TEST_F(NetworkAnonymizationKeyTest, IsTransient) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/std::nullopt);
  NetworkAnonymizationKey data_top_frame_key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kDataSite,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/std::nullopt);
  NetworkAnonymizationKey populated_key_with_nonce =
      NetworkAnonymizationKey::CreateFromParts(
          /*top_frame_site=*/kTestSiteA,
          /*is_cross_site*/ false, base::UnguessableToken::Create());
  NetworkAnonymizationKey data_frame_key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/std::nullopt);

  NetworkAnonymizationKey from_create_transient =
      NetworkAnonymizationKey::CreateTransient();

  EXPECT_TRUE(empty_key.IsTransient());
  EXPECT_FALSE(populated_key.IsTransient());
  EXPECT_TRUE(data_top_frame_key.IsTransient());
  EXPECT_TRUE(populated_key_with_nonce.IsTransient());
  EXPECT_TRUE(from_create_transient.IsTransient());

  NetworkAnonymizationKey populated_double_key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/std::nullopt);
  EXPECT_FALSE(data_frame_key.IsTransient());
  EXPECT_FALSE(populated_double_key.IsTransient());
}

TEST_F(NetworkAnonymizationKeyTest, IsFullyPopulated) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/std::nullopt);
  EXPECT_TRUE(populated_key.IsFullyPopulated());
  EXPECT_FALSE(empty_key.IsFullyPopulated());
  NetworkAnonymizationKey empty_frame_site_key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/std::nullopt);
  EXPECT_TRUE(empty_frame_site_key.IsFullyPopulated());
}

TEST_F(NetworkAnonymizationKeyTest, Getters) {
  NetworkAnonymizationKey key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/true, kNonce);

  EXPECT_EQ(key.GetTopFrameSite(), kTestSiteA);
  EXPECT_EQ(key.GetNonce(), kNonce);

  EXPECT_TRUE(key.IsCrossSite());
}

TEST_F(NetworkAnonymizationKeyTest, ToDebugString) {
  NetworkAnonymizationKey key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/true, kNonce);
  NetworkAnonymizationKey empty_key;

  // `is_cross_site` holds the value the key is created with.
  std::string double_key_with_cross_site_flag_expected_string_value =
      kTestSiteA.GetDebugString() + " cross_site (with nonce " +
      kNonce.ToString() + ")";
  EXPECT_EQ(key.ToDebugString(),
            double_key_with_cross_site_flag_expected_string_value);
  EXPECT_EQ(empty_key.ToDebugString(), "null");
}

TEST_F(NetworkAnonymizationKeyTest, Equality) {
  NetworkAnonymizationKey key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/false, kNonce);
  NetworkAnonymizationKey key_duplicate =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/false, kNonce);
  EXPECT_TRUE(key == key_duplicate);
  EXPECT_FALSE(key != key_duplicate);
  EXPECT_FALSE(key < key_duplicate);

  NetworkAnonymizationKey key_cross_site =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/true, kNonce);

  // The `is_cross_site` flag changes the NAK.
  EXPECT_FALSE(key == key_cross_site);
  EXPECT_TRUE(key != key_cross_site);
  EXPECT_TRUE(key < key_cross_site);

  NetworkAnonymizationKey key_no_nonce =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/std::nullopt);
  EXPECT_FALSE(key == key_no_nonce);
  EXPECT_TRUE(key != key_no_nonce);
  EXPECT_FALSE(key < key_no_nonce);

  NetworkAnonymizationKey key_different_nonce =
      NetworkAnonymizationKey::CreateFromParts(
          /*top_frame_site=*/kTestSiteA,
          /*is_cross_site=*/false,
          /*nonce=*/base::UnguessableToken::Create());
  EXPECT_FALSE(key == key_different_nonce);
  EXPECT_TRUE(key != key_different_nonce);

  NetworkAnonymizationKey key_different_frame_site =
      NetworkAnonymizationKey::CreateFromParts(
          /*top_frame_site=*/kTestSiteA,
          /*is_cross_site=*/false, kNonce);

  EXPECT_TRUE(key == key_different_frame_site);
  EXPECT_FALSE(key != key_different_frame_site);
  EXPECT_FALSE(key < key_different_frame_site);

  NetworkAnonymizationKey key_different_top_level_site =
      NetworkAnonymizationKey::CreateFromParts(
          /*top_frame_site=*/kTestSiteB,
          /*is_cross_site=*/false, kNonce);
  EXPECT_FALSE(key == key_different_top_level_site);
  EXPECT_TRUE(key != key_different_top_level_site);
  EXPECT_TRUE(key < key_different_top_level_site);

  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey empty_key_duplicate;
  EXPECT_TRUE(empty_key == empty_key_duplicate);
  EXPECT_FALSE(empty_key != empty_key_duplicate);
  EXPECT_FALSE(empty_key < empty_key_duplicate);

  EXPECT_FALSE(empty_key == key);
  EXPECT_TRUE(empty_key != key);
  EXPECT_TRUE(empty_key < key);
}

TEST_F(NetworkAnonymizationKeyTest, ValueRoundTripCrossSite) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  NetworkAnonymizationKey original_key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/true);
  base::Value value;
  ASSERT_TRUE(original_key.ToValue(&value));

  // Fill initial value with opaque data, to make sure it's overwritten.
  NetworkAnonymizationKey from_value_key = NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(value, &from_value_key));
  EXPECT_EQ(original_key, from_value_key);
}

TEST_F(NetworkAnonymizationKeyTest, ValueRoundTripSameSite) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  NetworkAnonymizationKey original_key =
      NetworkAnonymizationKey::CreateFromParts(/*top_frame_site=*/kTestSiteA,
                                               /*is_cross_site=*/false);
  base::Value value;
  ASSERT_TRUE(original_key.ToValue(&value));

  // Fill initial value with opaque data, to make sure it's overwritten.
  NetworkAnonymizationKey from_value_key = NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(value, &from_value_key));
  EXPECT_EQ(original_key, from_value_key);
}

TEST_F(NetworkAnonymizationKeyTest, TransientValueRoundTrip) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  NetworkAnonymizationKey original_key =
      NetworkAnonymizationKey::CreateTransient();
  base::Value value;
  ASSERT_FALSE(original_key.ToValue(&value));
}

TEST_F(NetworkAnonymizationKeyTest, EmptyValueRoundTrip) {
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  NetworkAnonymizationKey original_key;
  base::Value value;
  ASSERT_TRUE(original_key.ToValue(&value));

  // Fill initial value with opaque data, to make sure it's overwritten.
  NetworkAnonymizationKey from_value_key = NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(value, &from_value_key));
  EXPECT_EQ(original_key, from_value_key);
}

TEST(NetworkAnonymizationKeyFeatureShiftTest,
     ValueRoundTripKeySchemeMissmatch) {
  base::test::ScopedFeatureList scoped_feature_list_;
  const SchemefulSite kOpaqueSite = SchemefulSite(GURL("data:text/html,junk"));
  const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
  const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));
  NetworkAnonymizationKey expected_failure_nak = NetworkAnonymizationKey();

  // Create a cross site double key + cross site flag NetworkAnonymizationKey.
  NetworkAnonymizationKey original_cross_site_double_key =
      NetworkAnonymizationKey::CreateFromParts(kTestSiteA, false);
  base::Value cross_site_double_key_value;
  ASSERT_TRUE(
      original_cross_site_double_key.ToValue(&cross_site_double_key_value));

  // Check that deserializing a double keyed NetworkAnonymizationKey (a
  // one-element list) fails, using the serialized site from
  // `cross_site_double_key_value` to build it.
  base::Value serialized_site =
      cross_site_double_key_value.GetList()[0].Clone();
  base::Value::List double_key_list;
  double_key_list.Append(serialized_site.Clone());
  base::Value double_key_value = base::Value(std::move(double_key_list));
  EXPECT_FALSE(NetworkAnonymizationKey::FromValue(double_key_value,
                                                  &expected_failure_nak));

  // Check that deserializing a triple keyed value (a 2-element list containing
  // two sites) fails.
  base::Value::List triple_key_list;
  triple_key_list.Append(serialized_site.Clone());
  triple_key_list.Append(std::move(serialized_site));
  base::Value triple_key_value = base::Value(std::move(triple_key_list));
  EXPECT_FALSE(NetworkAnonymizationKey::FromValue(triple_key_value,
                                                  &expected_failure_nak));

  // Convert the successful value back to a NAK and verify.
  NetworkAnonymizationKey from_value_cross_site_double_key =
      NetworkAnonymizationKey();
  EXPECT_TRUE(NetworkAnonymizationKey::FromValue(
      cross_site_double_key_value, &from_value_cross_site_double_key));
  EXPECT_EQ(original_cross_site_double_key, from_value_cross_site_double_key);
}

}  // namespace net
