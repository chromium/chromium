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

  const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
  const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));

  net::HttpRequestInfo triple_nik_double_nak_request_info;
  triple_nik_double_nak_request_info.network_isolation_key =
      NetworkIsolationKey(kTestSiteA, kTestSiteB);

  // Triple key NIK and double key NAK.
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);

  EXPECT_FALSE(triple_nik_double_nak_request_info.IsConsistent());

  net::HttpRequestInfo triple_nik_double_xsite_bit_nak_request_info;
  triple_nik_double_xsite_bit_nak_request_info.network_isolation_key =
      NetworkIsolationKey(kTestSiteA, kTestSiteB);
  triple_nik_double_xsite_bit_nak_request_info.network_anonymization_key =
      NetworkAnonymizationKey(kTestSiteA, kTestSiteB, true);

  EXPECT_TRUE(triple_nik_double_xsite_bit_nak_request_info.IsConsistent());
}
}  // namespace net
