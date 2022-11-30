// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_info.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HTTPRequestInfoTest, IsConsistent) {
  // TODO(brgoldstein): refactor this test with new testing config enum when
  // you update NetworkAnonymizationKey tests and IsolationInfo tests.

  // Triple keyed NIK and NAK.
  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<base::test::FeatureRef> disabled_features_1 = {};
  disabled_features_1.push_back(
      net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);
  disabled_features_1.push_back(
      net::features::kEnableDoubleKeyNetworkAnonymizationKey);
  disabled_features_1.push_back(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
  scoped_feature_list_.InitWithFeatures({}, disabled_features_1);

  const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
  const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));

  net::HttpRequestInfo empty_request_info;

  net::HttpRequestInfo request_info_different_nik_nak;
  request_info_different_nik_nak.network_isolation_key =
      NetworkIsolationKey(kTestSiteA, kTestSiteB);
  request_info_different_nik_nak.network_anonymization_key =
      NetworkAnonymizationKey(kTestSiteB, kTestSiteA);

  net::HttpRequestInfo triple_keys_request_info;
  triple_keys_request_info.network_isolation_key =
      NetworkIsolationKey(kTestSiteA, kTestSiteB);
  triple_keys_request_info.network_anonymization_key =
      NetworkAnonymizationKey(kTestSiteA, kTestSiteB);

  net::HttpRequestInfo triple_nik_double_nak_request_info;
  triple_nik_double_nak_request_info.network_isolation_key =
      NetworkIsolationKey(kTestSiteA, kTestSiteB);

  EXPECT_FALSE(request_info_different_nik_nak.IsConsistent());
  EXPECT_TRUE(triple_keys_request_info.IsConsistent());

  // Double key NIK and triple key NAK.
  scoped_feature_list_.Reset();
  std::vector<base::test::FeatureRef> enabled_features_2 = {};
  std::vector<base::test::FeatureRef> disabled_features_2 = {};
  enabled_features_2.push_back(
      net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);
  disabled_features_2.push_back(
      net::features::kEnableDoubleKeyNetworkAnonymizationKey);
  disabled_features_2.push_back(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
  scoped_feature_list_.InitWithFeatures(enabled_features_2,
                                        disabled_features_2);
  // This is not a valid key confiugrations so
  // NetworkAnonymizationKey::CreateFromNetworkIsolationKey should always
  // DCHECK.
  EXPECT_DEATH_IF_SUPPORTED(triple_keys_request_info.IsConsistent(), "");

  // Triple key NIK and double key NAK.
  scoped_feature_list_.Reset();
  std::vector<base::test::FeatureRef> enabled_features_3 = {};
  std::vector<base::test::FeatureRef> disabled_features_3 = {};
  disabled_features_3.push_back(
      net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);
  enabled_features_3.push_back(
      net::features::kEnableDoubleKeyNetworkAnonymizationKey);
  disabled_features_3.push_back(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
  scoped_feature_list_.InitWithFeatures(enabled_features_3,
                                        disabled_features_3);

  EXPECT_FALSE(triple_keys_request_info.IsConsistent());

  // Triple key NIK and double key with cross site flag NAK.
  scoped_feature_list_.Reset();
  std::vector<base::test::FeatureRef> enabled_features_4 = {};
  std::vector<base::test::FeatureRef> disabled_features_4 = {};
  disabled_features_3.push_back(
      net::features::kForceIsolationInfoFrameOriginToTopLevelFrame);
  enabled_features_3.push_back(
      net::features::kEnableDoubleKeyNetworkAnonymizationKey);
  enabled_features_4.push_back(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
  scoped_feature_list_.InitWithFeatures(enabled_features_4,
                                        disabled_features_4);

  EXPECT_FALSE(triple_keys_request_info.IsConsistent());
  EXPECT_FALSE(triple_nik_double_nak_request_info.IsConsistent());

  net::HttpRequestInfo triple_nik_double_xsite_bit_nak_request_info;
  triple_nik_double_xsite_bit_nak_request_info.network_isolation_key =
      NetworkIsolationKey(kTestSiteA, kTestSiteB);
  triple_nik_double_xsite_bit_nak_request_info.network_anonymization_key =
      NetworkAnonymizationKey(kTestSiteA, kTestSiteB, true);

  EXPECT_TRUE(triple_nik_double_xsite_bit_nak_request_info.IsConsistent());
}
}  // namespace net
