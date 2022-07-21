// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_anonymization_key.h"

#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace net {

namespace {
const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));
const SchemefulSite kDataSite = SchemefulSite(GURL("data:foo"));
const SchemefulSite kEmptySite;
const base::UnguessableToken kNonce = base::UnguessableToken::Create();
}  // namespace

class NetworkAnonymizationKeyTest : public testing::Test {
 public:
  NetworkAnonymizationKeyTest() = default;
  NetworkAnonymizationKeyTest(const NetworkAnonymizationKeyTest&) = delete;
  NetworkAnonymizationKeyTest& operator=(const NetworkAnonymizationKeyTest&) =
      delete;
  ~NetworkAnonymizationKeyTest() override = default;
};

TEST_F(NetworkAnonymizationKeyTest, IsEmpty) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);

  EXPECT_TRUE(empty_key.IsEmpty());
  EXPECT_FALSE(populated_key.IsEmpty());
}

TEST_F(NetworkAnonymizationKeyTest, IsTransient) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey data_frame_key(/*top_frame_site=*/kTestSiteA,
                                         /*frame_site=*/kDataSite,
                                         /*is_cross_site=*/false,
                                         /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey data_top_frame_key(/*top_frame_site=*/kDataSite,
                                             /*frame_site=*/kTestSiteB,
                                             /*is_cross_site=*/false,
                                             /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey populated_key_with_nonce(
      /*top_frame_site=*/kTestSiteA, /*frame_site=*/kTestSiteB,
      /*is_cross_site*/ false, base::UnguessableToken::Create());

  EXPECT_TRUE(empty_key.IsTransient());
  EXPECT_FALSE(populated_key.IsTransient());
  EXPECT_TRUE(data_frame_key.IsTransient());
  EXPECT_TRUE(data_top_frame_key.IsTransient());
  EXPECT_TRUE(populated_key_with_nonce.IsTransient());
}

TEST_F(NetworkAnonymizationKeyTest, IsFullyPopulated) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey empty_frame_site_key(/*top_frame_site=*/kTestSiteA,
                                               /*frame_site=*/kEmptySite,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/absl::nullopt);

  EXPECT_TRUE(populated_key.IsFullyPopulated());
  EXPECT_FALSE(empty_key.IsFullyPopulated());
}

TEST_F(NetworkAnonymizationKeyTest, Getters) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/true, kNonce);

  EXPECT_EQ(key.GetTopFrameSite(), kTestSiteA);
  EXPECT_EQ(key.GetFrameSite(), kTestSiteB);
  EXPECT_TRUE(key.GetIsCrossSite());
  EXPECT_EQ(key.GetNonce(), kNonce);
}

TEST_F(NetworkAnonymizationKeyTest, ToDebugString) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/true, kNonce);
  NetworkAnonymizationKey empty_key;

  std::string key_expected_string_value =
      kTestSiteA.GetDebugString() + " " + kTestSiteB.GetDebugString() +
      " cross_site (with nonce " + kNonce.ToString() + ")";
  EXPECT_EQ(key.ToDebugString(), key_expected_string_value);

  // The default NAK should have no nonce and be same_site (is_cross_site =
  // false).
  EXPECT_EQ(empty_key.ToDebugString(), "null null same_site");
}

TEST_F(NetworkAnonymizationKeyTest, Equality) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/false, kNonce);
  NetworkAnonymizationKey key_duplicate(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false, kNonce);
  EXPECT_TRUE(key == key_duplicate);
  EXPECT_FALSE(key != key_duplicate);
  EXPECT_FALSE(key < key_duplicate);

  NetworkAnonymizationKey key_cross_site(/*top_frame_site=*/kTestSiteA,
                                         /*frame_site=*/kTestSiteB,
                                         /*is_cross_site=*/true, kNonce);
  EXPECT_FALSE(key == key_cross_site);
  EXPECT_TRUE(key != key_cross_site);
  EXPECT_TRUE(key < key_cross_site);

  NetworkAnonymizationKey key_no_nonce(/*top_frame_site=*/kTestSiteA,
                                       /*frame_site=*/kTestSiteB,
                                       /*is_cross_site=*/false,
                                       /*nonce=*/absl::nullopt);
  EXPECT_FALSE(key == key_no_nonce);
  EXPECT_TRUE(key != key_no_nonce);
  EXPECT_FALSE(key < key_no_nonce);

  NetworkAnonymizationKey key_different_nonce(
      /*top_frame_site=*/kTestSiteA,
      /*frame_site=*/kTestSiteB,
      /*is_cross_site=*/false,
      /*nonce=*/base::UnguessableToken::Create());
  EXPECT_FALSE(key == key_different_nonce);
  EXPECT_TRUE(key != key_different_nonce);

  NetworkAnonymizationKey key_different_frame_site(
      /*top_frame_site=*/kTestSiteA, /*frame_site=*/kTestSiteA,
      /*is_cross_site=*/false, kNonce);
  EXPECT_FALSE(key == key_different_frame_site);
  EXPECT_TRUE(key != key_different_frame_site);
  EXPECT_FALSE(key < key_different_frame_site);

  NetworkAnonymizationKey key_different_top_level_site(
      /*top_frame_site=*/kTestSiteB, /*frame_site=*/kTestSiteB,
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

}  // namespace net