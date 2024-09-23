// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_info.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(HTTPRequestInfoTest, IsConsistent) {
  const SchemefulSite kTestSiteA = SchemefulSite(GURL("http://a.test/"));
  const SchemefulSite kTestSiteB = SchemefulSite(GURL("http://b.test/"));

  HttpRequestInfo with_anon_nak;
  with_anon_nak.network_isolation_key =
      NetworkIsolationKey(kTestSiteA, kTestSiteB);
  EXPECT_FALSE(with_anon_nak.IsConsistent());

  HttpRequestInfo cross_site;
  cross_site.network_isolation_key =
      NetworkIsolationKey(kTestSiteA, kTestSiteB);
  cross_site.network_anonymization_key =
      NetworkAnonymizationKey::CreateCrossSite(kTestSiteA);
  EXPECT_TRUE(cross_site.IsConsistent());
}
}  // namespace net
