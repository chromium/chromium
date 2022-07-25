// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_anonymization_key.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace net {

class NetworkAnonymizationKeyTest : public testing::Test,
                                    public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    if (IsDoubleKeyEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          net::features::kEnableDoubleKeyNetworkAnonymizationKey);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          net::features::kEnableDoubleKeyNetworkAnonymizationKey);
    }
  }
  static bool IsDoubleKeyEnabled() { return GetParam(); }

 protected:
  const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
  const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));
  const SchemefulSite kDataSite = SchemefulSite(GURL("data:foo"));
  const base::UnguessableToken kNonce = base::UnguessableToken::Create();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    NetworkAnonymizationKeyTest,
    /*kEnableDoubleKeyNetworkAnonymizationKey*/ testing::Bool());

TEST_P(NetworkAnonymizationKeyTest, IsDoubleKeyingEnabled) {
  if (IsDoubleKeyEnabled()) {
    EXPECT_TRUE(NetworkAnonymizationKey::IsDoubleKeyingEnabled());
  } else {
    EXPECT_FALSE(NetworkAnonymizationKey::IsDoubleKeyingEnabled());
  }
}

TEST_P(NetworkAnonymizationKeyTest, IsEmpty) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);

  EXPECT_TRUE(empty_key.IsEmpty());
  EXPECT_FALSE(populated_key.IsEmpty());
}

TEST_P(NetworkAnonymizationKeyTest, IsTransient) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey data_top_frame_key(/*top_frame_site=*/kDataSite,
                                             /*frame_site=*/kTestSiteB,
                                             /*is_cross_site=*/false,
                                             /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey populated_key_with_nonce(
      /*top_frame_site=*/kTestSiteA, /*frame_site=*/kTestSiteB,
      /*is_cross_site*/ false, base::UnguessableToken::Create());
  NetworkAnonymizationKey data_frame_key(/*top_frame_site=*/kTestSiteA,
                                         /*frame_site=*/kDataSite,
                                         /*is_cross_site=*/false,
                                         /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey populated_double_key(/*top_frame_site=*/kTestSiteA,
                                               /*frame_site=*/absl::nullopt,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/absl::nullopt);

  EXPECT_TRUE(empty_key.IsTransient());
  EXPECT_FALSE(populated_key.IsTransient());
  EXPECT_TRUE(data_top_frame_key.IsTransient());
  EXPECT_TRUE(data_top_frame_key.IsTransient());
  EXPECT_TRUE(populated_key_with_nonce.IsTransient());

  if (IsDoubleKeyEnabled()) {
    EXPECT_FALSE(data_frame_key.IsTransient());
    EXPECT_FALSE(populated_double_key.IsTransient());
  } else {
    EXPECT_TRUE(data_frame_key.IsTransient());
    EXPECT_TRUE(populated_double_key.IsTransient());
  }
}

TEST_P(NetworkAnonymizationKeyTest, IsFullyPopulated) {
  NetworkAnonymizationKey empty_key;
  NetworkAnonymizationKey populated_key(/*top_frame_site=*/kTestSiteA,
                                        /*frame_site=*/kTestSiteB,
                                        /*is_cross_site=*/false,
                                        /*nonce=*/absl::nullopt);
  NetworkAnonymizationKey empty_frame_site_key(/*top_frame_site=*/kTestSiteA,
                                               /*frame_site=*/absl::nullopt,
                                               /*is_cross_site=*/false,
                                               /*nonce=*/absl::nullopt);
  EXPECT_TRUE(populated_key.IsFullyPopulated());
  EXPECT_FALSE(empty_key.IsFullyPopulated());
  if (IsDoubleKeyEnabled()) {
    EXPECT_TRUE(empty_frame_site_key.IsFullyPopulated());
  } else {
    EXPECT_FALSE(empty_frame_site_key.IsFullyPopulated());
  }
}

TEST_P(NetworkAnonymizationKeyTest, Getters) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/true, kNonce);

  EXPECT_EQ(key.GetTopFrameSite(), kTestSiteA);
  EXPECT_TRUE(key.GetIsCrossSite());
  EXPECT_EQ(key.GetNonce(), kNonce);
  if (IsDoubleKeyEnabled()) {
    EXPECT_EQ(key.GetFrameSite(), absl::nullopt);
  } else {
    EXPECT_EQ(key.GetFrameSite(), kTestSiteB);
  }
}

TEST_P(NetworkAnonymizationKeyTest, ToDebugString) {
  NetworkAnonymizationKey key(/*top_frame_site=*/kTestSiteA,
                              /*frame_site=*/kTestSiteB,
                              /*is_cross_site=*/true, kNonce);
  NetworkAnonymizationKey empty_key;

  if (IsDoubleKeyEnabled()) {
    std::string double_key_expected_string_value =
        kTestSiteA.GetDebugString() + " null" + " cross_site (with nonce " +
        kNonce.ToString() + ")";
    EXPECT_EQ(key.ToDebugString(), double_key_expected_string_value);
  } else {
    std::string key_expected_string_value =
        kTestSiteA.GetDebugString() + " " + kTestSiteB.GetDebugString() +
        " cross_site (with nonce " + kNonce.ToString() + ")";
    EXPECT_EQ(key.ToDebugString(), key_expected_string_value);
  }

  // By default, an empty NAK should have no nonce and be same_site
  // (is_cross_site = false).
  EXPECT_EQ(empty_key.ToDebugString(), "null null same_site");
}

TEST_P(NetworkAnonymizationKeyTest, Equality) {
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

  if (IsDoubleKeyEnabled()) {
    EXPECT_TRUE(key == key_different_frame_site);
    EXPECT_FALSE(key != key_different_frame_site);
  } else {
    EXPECT_FALSE(key == key_different_frame_site);
    EXPECT_TRUE(key != key_different_frame_site);
  }
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