// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/isolation_info.h"

#include <iostream>
#include <optional>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "net/base/features.h"
#include "net/base/isolation_info.pb.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/base/network_isolation_partition.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace net {

namespace {

class IsolationInfoTest : public testing::Test {
 public:
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://a.foo.test"));
  const url::Origin kSite1 = url::Origin::Create(GURL("https://foo.test"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://b.bar.test"));
  const url::Origin kSite2 = url::Origin::Create(GURL("https://bar.test"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("https://c.baz.test"));
  const url::Origin kOpaqueOrigin;

  const base::UnguessableToken kNonce1 = base::UnguessableToken::Create();
  const base::UnguessableToken kNonce2 = base::UnguessableToken::Create();
};

void DuplicateAndCompare(const IsolationInfo& isolation_info) {
  std::optional<IsolationInfo> duplicate_isolation_info =
      IsolationInfo::CreateIfConsistent(
          isolation_info.request_type(), isolation_info.top_frame_origin(),
          isolation_info.frame_origin(), isolation_info.site_for_cookies(),
          isolation_info.nonce(), isolation_info.GetNetworkIsolationPartition(),
          isolation_info.frame_ancestor_relation());

  ASSERT_TRUE(duplicate_isolation_info);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(*duplicate_isolation_info));
}

TEST_F(IsolationInfoTest, DebugString) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kSubFrame, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1), kNonce1);
  std::vector<std::string> parts;
  parts.push_back(
      "request_type: kSubFrame; top_frame_origin: https://a.foo.test; ");
  parts.push_back("frame_origin: https://b.bar.test; ");
  parts.push_back("network_anonymization_key: ");
  parts.push_back(isolation_info.network_anonymization_key().ToDebugString());
  parts.push_back("; network_isolation_key: ");
  parts.push_back(isolation_info.network_isolation_key().ToDebugString());
  parts.push_back("; nonce: ");
  parts.push_back(isolation_info.nonce().value().ToString());
  parts.push_back(
      "; site_for_cookies: SiteForCookies: {site=https://foo.test; "
      "schemefully_same=true}");
  parts.push_back("; frame_ancestor_relation: (none)");
  EXPECT_EQ(isolation_info.DebugString(), base::StrCat(parts));

  // Check again with a non-null frame_ancestor_relation;
  parts.pop_back();
  parts.push_back("; frame_ancestor_relation: cross-site");
  isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1),
                            kNonce1, NetworkIsolationPartition::kGeneral,
                            IsolationInfo::FrameAncestorRelation::kCrossSite);

  EXPECT_EQ(isolation_info.DebugString(), base::StrCat(parts));
}

TEST_F(IsolationInfoTest, RequestTypeMainFrame) {
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin1,
                            kOrigin1, SiteForCookies::FromOrigin(kOrigin1));
  EXPECT_EQ(IsolationInfo::RequestType::kMainFrame,
            isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());

  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_EQ("https://foo.test https://foo.test",
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_FALSE(isolation_info.nonce().has_value());
  EXPECT_TRUE(isolation_info.IsMainFrameRequest());
  EXPECT_TRUE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_EQ(IsolationInfo::RequestType::kMainFrame,
            redirected_isolation_info.request_type());
  EXPECT_EQ(kOrigin3, redirected_isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin3, redirected_isolation_info.frame_origin());
  EXPECT_TRUE(
      redirected_isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(redirected_isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ(
      "https://baz.test https://baz.test",
      redirected_isolation_info.network_isolation_key().ToCacheKeyString());

  EXPECT_TRUE(redirected_isolation_info.site_for_cookies().IsFirstParty(
      kOrigin3.GetURL()));
  EXPECT_FALSE(redirected_isolation_info.nonce().has_value());
  EXPECT_TRUE(redirected_isolation_info.IsMainFrameRequest());
  EXPECT_TRUE(redirected_isolation_info.IsOutermostMainFrameRequest());
}

TEST_F(IsolationInfoTest, RequestTypeSubFrame) {
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1));
  EXPECT_EQ(IsolationInfo::RequestType::kSubFrame,
            isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin2, isolation_info.frame_origin());
  EXPECT_EQ("https://foo.test https://bar.test",
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_FALSE(isolation_info.nonce().has_value());
  EXPECT_FALSE(isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_EQ(IsolationInfo::RequestType::kSubFrame,
            redirected_isolation_info.request_type());
  EXPECT_EQ(kOrigin1, redirected_isolation_info.top_frame_origin());

  EXPECT_EQ(kOrigin3, redirected_isolation_info.frame_origin());
  EXPECT_EQ(
      "https://foo.test https://baz.test",
      redirected_isolation_info.network_isolation_key().ToCacheKeyString());

  EXPECT_TRUE(
      redirected_isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(redirected_isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(redirected_isolation_info.site_for_cookies().IsFirstParty(
      kOrigin1.GetURL()));
  EXPECT_FALSE(redirected_isolation_info.nonce().has_value());
  EXPECT_FALSE(redirected_isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(redirected_isolation_info.IsOutermostMainFrameRequest());
}

TEST_F(IsolationInfoTest, RequestTypeMainFrameWithNonce) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin1), kNonce1);
  EXPECT_EQ(IsolationInfo::RequestType::kMainFrame,
            isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ(std::nullopt,
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_EQ(kNonce1, isolation_info.nonce().value());
  EXPECT_TRUE(isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_EQ(IsolationInfo::RequestType::kMainFrame,
            redirected_isolation_info.request_type());
  EXPECT_EQ(kOrigin3, redirected_isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin3, redirected_isolation_info.frame_origin());
  EXPECT_TRUE(
      redirected_isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(redirected_isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ(
      std::nullopt,
      redirected_isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(redirected_isolation_info.site_for_cookies().IsFirstParty(
      kOrigin3.GetURL()));
  EXPECT_EQ(kNonce1, redirected_isolation_info.nonce().value());
  EXPECT_TRUE(redirected_isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(redirected_isolation_info.IsOutermostMainFrameRequest());
}

TEST_F(IsolationInfoTest,
       RequestTypeMainFrameWithNonGeneralNetworkIsolationPartition) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin1), /*nonce=*/std::nullopt,
      NetworkIsolationPartition::kProtectedAudienceSellerWorklet);
  EXPECT_EQ(IsolationInfo::RequestType::kMainFrame,
            isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_EQ(NetworkIsolationPartition::kProtectedAudienceSellerWorklet,
            isolation_info.GetNetworkIsolationPartition());
  EXPECT_EQ(
      NetworkIsolationPartition::kProtectedAudienceSellerWorklet,
      isolation_info.network_anonymization_key().network_isolation_partition());
  EXPECT_EQ(
      NetworkIsolationPartition::kProtectedAudienceSellerWorklet,
      isolation_info.network_isolation_key().GetNetworkIsolationPartition());
  EXPECT_EQ("https://foo.test https://foo.test 1",
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_FALSE(isolation_info.nonce().has_value());
  EXPECT_TRUE(isolation_info.IsMainFrameRequest());
  EXPECT_TRUE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_EQ(IsolationInfo::RequestType::kMainFrame,
            redirected_isolation_info.request_type());
  EXPECT_EQ(kOrigin3, redirected_isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin3, redirected_isolation_info.frame_origin());
  EXPECT_EQ(NetworkIsolationPartition::kProtectedAudienceSellerWorklet,
            redirected_isolation_info.GetNetworkIsolationPartition());
  EXPECT_EQ(NetworkIsolationPartition::kProtectedAudienceSellerWorklet,
            redirected_isolation_info.network_anonymization_key()
                .network_isolation_partition());
  EXPECT_EQ(NetworkIsolationPartition::kProtectedAudienceSellerWorklet,
            redirected_isolation_info.network_isolation_key()
                .GetNetworkIsolationPartition());
  EXPECT_TRUE(
      redirected_isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(redirected_isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ(
      "https://baz.test https://baz.test 1",
      redirected_isolation_info.network_isolation_key().ToCacheKeyString());

  EXPECT_TRUE(redirected_isolation_info.site_for_cookies().IsFirstParty(
      kOrigin3.GetURL()));
  EXPECT_FALSE(redirected_isolation_info.nonce().has_value());
  EXPECT_TRUE(redirected_isolation_info.IsMainFrameRequest());
  EXPECT_TRUE(redirected_isolation_info.IsOutermostMainFrameRequest());
}

TEST_F(IsolationInfoTest, RequestTypeSubFrameWithNonce) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kSubFrame, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1), kNonce1);
  EXPECT_EQ(IsolationInfo::RequestType::kSubFrame,
            isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin2, isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ(std::nullopt,
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_EQ(kNonce1, isolation_info.nonce().value());
  EXPECT_FALSE(isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_EQ(IsolationInfo::RequestType::kSubFrame,
            redirected_isolation_info.request_type());
  EXPECT_EQ(kOrigin1, redirected_isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin3, redirected_isolation_info.frame_origin());
  EXPECT_TRUE(
      redirected_isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(redirected_isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ(
      std::nullopt,
      redirected_isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(redirected_isolation_info.site_for_cookies().IsFirstParty(
      kOrigin1.GetURL()));
  EXPECT_EQ(kNonce1, redirected_isolation_info.nonce().value());
  EXPECT_FALSE(redirected_isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(redirected_isolation_info.IsOutermostMainFrameRequest());
}

TEST_F(IsolationInfoTest, RequestTypeOther) {
  IsolationInfo isolation_info;
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_FALSE(isolation_info.top_frame_origin());
  EXPECT_FALSE(isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsEmpty());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.nonce());
  EXPECT_FALSE(isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(isolation_info.IsOutermostMainFrameRequest());
  EXPECT_FALSE(isolation_info.frame_ancestor_relation());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_F(IsolationInfoTest, RequestTypeOtherWithSiteForCookies) {
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, kOrigin1,
                            kOrigin1, SiteForCookies::FromOrigin(kOrigin1));
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_EQ("https://foo.test https://foo.test",
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_FALSE(isolation_info.nonce());
  EXPECT_FALSE(isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

// Test case of a subresource for cross-site subframe (which has an empty
// site-for-cookies).
TEST_F(IsolationInfoTest, RequestTypeOtherWithEmptySiteForCookies) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kOther, kOrigin1, kOrigin2, SiteForCookies());
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin2, isolation_info.frame_origin());
  EXPECT_EQ("https://foo.test https://bar.test",
            isolation_info.network_isolation_key().ToCacheKeyString());

  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.nonce());
  EXPECT_FALSE(isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_F(IsolationInfoTest, CreateTransient) {
  IsolationInfo isolation_info =
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt);
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_TRUE(isolation_info.top_frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.nonce());
  EXPECT_FALSE(isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_F(IsolationInfoTest, CreateTransientWithNonce) {
  IsolationInfo isolation_info = IsolationInfo::CreateTransient(kNonce1);
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_TRUE(isolation_info.top_frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  ASSERT_TRUE(isolation_info.nonce().has_value());
  EXPECT_EQ(isolation_info.nonce().value(), kNonce1);
  EXPECT_FALSE(isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));

  IsolationInfo new_info_same_nonce = IsolationInfo::CreateTransient(kNonce1);
  ASSERT_TRUE(new_info_same_nonce.nonce().has_value());
  EXPECT_EQ(new_info_same_nonce.nonce().value(), kNonce1);

  // The new NIK is distinct from the first one because it uses a new opaque
  // origin, even if the nonce is the same.
  EXPECT_NE(isolation_info.network_isolation_key(),
            new_info_same_nonce.network_isolation_key());
}

TEST_F(IsolationInfoTest, CreateForInternalRequest) {
  IsolationInfo isolation_info =
      IsolationInfo::CreateForInternalRequest(kOrigin1);
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_EQ("https://foo.test https://foo.test",
            isolation_info.network_isolation_key().ToCacheKeyString());

  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_FALSE(isolation_info.nonce());
  EXPECT_FALSE(isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

// Test that in the UpdateNothing case, the SiteForCookies does not have to
// match the frame origin, unlike in the HTTP/HTTPS case.
TEST_F(IsolationInfoTest, CustomSchemeRequestTypeOther) {
  // Have to register the scheme, or url::Origin::Create() will return an
  // opaque origin.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("foo", url::SCHEME_WITH_HOST);

  const GURL kCustomOriginUrl = GURL("foo://a.foo.com");
  const url::Origin kCustomOrigin = url::Origin::Create(kCustomOriginUrl);

  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kOther, kCustomOrigin, kOrigin1,
      SiteForCookies::FromOrigin(kCustomOrigin));
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_EQ(kCustomOrigin, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_EQ("foo://a.foo.com https://foo.test",
            isolation_info.network_isolation_key().ToCacheKeyString());

  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsFirstParty(kCustomOriginUrl));
  EXPECT_FALSE(isolation_info.nonce());
  EXPECT_FALSE(isolation_info.IsMainFrameRequest());
  EXPECT_FALSE(isolation_info.IsOutermostMainFrameRequest());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin2);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

// Success cases are covered by other tests, so only need a separate test to
// cover the failure cases.
TEST_F(IsolationInfoTest, CreateIfConsistentFails) {
  // Main frames with inconsistent SiteForCookies.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin2),
      /*nonce=*/std::nullopt, NetworkIsolationPartition::kGeneral,
      IsolationInfo::FrameAncestorRelation::kSameOrigin));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kMainFrame, kOpaqueOrigin, kOpaqueOrigin,
      SiteForCookies::FromOrigin(kOrigin1),
      /*nonce=*/std::nullopt, NetworkIsolationPartition::kGeneral,
      IsolationInfo::FrameAncestorRelation::kSameOrigin));

  // Sub frame with inconsistent SiteForCookies.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kSubFrame, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin2)));

  // Sub resources with inconsistent SiteForCookies.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin2)));

  // Correctly have empty/non-empty origins:
  EXPECT_TRUE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, std::nullopt, std::nullopt,
      SiteForCookies()));

  // Incorrectly have empty/non-empty origins:
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, std::nullopt, kOrigin1,
      SiteForCookies()));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kSubFrame, std::nullopt, kOrigin2,
      SiteForCookies()));

  // Empty frame origins are incorrect.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, kOrigin1, std::nullopt,
      SiteForCookies()));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kSubFrame, kOrigin1, std::nullopt,
      SiteForCookies()));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, std::nullopt,
      SiteForCookies::FromOrigin(kOrigin1),
      /*nonce=*/std::nullopt, NetworkIsolationPartition::kGeneral,
      IsolationInfo::FrameAncestorRelation::kSameOrigin));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1)));

  // No origins with non-null SiteForCookies.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, std::nullopt, std::nullopt,
      SiteForCookies::FromOrigin(kOrigin1)));

  // No origins with non-null nonce.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, std::nullopt, std::nullopt,
      SiteForCookies(), kNonce1));

  // Non-kSameOrigin `frame_ancestor_relation` for kMainFrame RequestType.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1), /*nonce=*/std::nullopt,
      NetworkIsolationPartition::kGeneral,
      IsolationInfo::FrameAncestorRelation::kSameSite));

  // kSameOrigin `frame_ancestor_relation` with cross-site origins on a
  // subresource request.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1), /*nonce=*/std::nullopt,
      NetworkIsolationPartition::kGeneral,
      IsolationInfo::FrameAncestorRelation::kSameOrigin));
}

TEST_F(IsolationInfoTest, Serialization) {
  EXPECT_FALSE(IsolationInfo::Deserialize(""));
  EXPECT_FALSE(IsolationInfo::Deserialize("garbage"));

  const IsolationInfo kPositiveTestCases[] = {
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1)),
      // Null party context
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1)),
      // Empty party context
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1)),
      // Multiple party context entries.
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1)),
      // Without SiteForCookies
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies()),
      // Request type kOther
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, kOrigin1,
                            kOrigin1, SiteForCookies::FromOrigin(kOrigin1)),
      // Request type kMainframe
      IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin1,
                            kOrigin1, SiteForCookies::FromOrigin(kOrigin1),
                            /*nonce=*/std::nullopt,
                            NetworkIsolationPartition::kGeneral,
                            IsolationInfo::FrameAncestorRelation::kSameOrigin),
      // Non-general NetworkIsolationPartition
      IsolationInfo::Create(
          IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin1,
          SiteForCookies::FromOrigin(kOrigin1), /*nonce=*/std::nullopt,
          NetworkIsolationPartition::kProtectedAudienceSellerWorklet,
          IsolationInfo::FrameAncestorRelation::kSameOrigin),
      // Non-none IsolationInfo::FrameAncestorRelation
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, kOrigin1,
                            kOrigin1, SiteForCookies::FromOrigin(kOrigin1),
                            /*nonce=*/std::nullopt,
                            NetworkIsolationPartition::kGeneral,
                            IsolationInfo::FrameAncestorRelation::kSameOrigin),
  };
  for (const auto& info : kPositiveTestCases) {
    auto rt = IsolationInfo::Deserialize(info.Serialize());
    ASSERT_TRUE(rt);
    EXPECT_TRUE(rt->IsEqualForTesting(info));
  }

  const IsolationInfo kNegativeTestCases[] = {
      IsolationInfo::CreateTransient(/*nonce=*/std::nullopt),
      // With nonce (i.e transient).
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1),
                            kNonce1),
      // With an opaque frame origin. The opaque frame site will cause it to be
      // considered transient and fail to serialize.
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            url::Origin(),
                            SiteForCookies::FromOrigin(kOrigin1)),
  };
  for (const auto& info : kNegativeTestCases) {
    EXPECT_TRUE(info.Serialize().empty());
  }
}

TEST_F(IsolationInfoTest,
       DeserializationAcceptsValidNetworkIsolationPartitionOnly) {
  proto::IsolationInfo info;
  info.set_request_type(1);
  info.set_top_frame_origin(kOrigin1.Serialize());
  info.set_frame_origin(kOrigin2.Serialize());

  // We can deserialize a missing NetworkIsolationPartition.
  auto deserialized = IsolationInfo::Deserialize(info.SerializeAsString());
  ASSERT_TRUE(deserialized);
  EXPECT_EQ(deserialized->GetNetworkIsolationPartition(),
            NetworkIsolationPartition::kGeneral);

  // We can deserialize the max value of NetworkIsolationPartition.
  info.set_network_isolation_partition(
      static_cast<int32_t>(NetworkIsolationPartition::kMaxValue));
  deserialized = IsolationInfo::Deserialize(info.SerializeAsString());
  ASSERT_TRUE(deserialized);
  EXPECT_EQ(deserialized->GetNetworkIsolationPartition(),
            NetworkIsolationPartition::kMaxValue);

  // We can deserialize the min value of NetworkIsolationPartition.
  info.set_network_isolation_partition(0);
  deserialized = IsolationInfo::Deserialize(info.SerializeAsString());
  ASSERT_TRUE(deserialized);
  EXPECT_EQ(deserialized->GetNetworkIsolationPartition(),
            NetworkIsolationPartition::kGeneral);

  // We can't deserialize a negative value.
  info.set_network_isolation_partition(-1);
  EXPECT_FALSE(IsolationInfo::Deserialize(info.SerializeAsString()));

  // We can't deserialize a too large value.
  info.set_network_isolation_partition(
      static_cast<int32_t>(NetworkIsolationPartition::kMaxValue) + 1);
  EXPECT_FALSE(IsolationInfo::Deserialize(info.SerializeAsString()));
}

TEST_F(IsolationInfoTest, DeserializationHandlesInvalidRequestType) {
  proto::IsolationInfo info;
  info.set_request_type(1337);
  info.set_top_frame_origin(kOrigin1.Serialize());
  info.set_frame_origin(kOrigin2.Serialize());

  EXPECT_EQ(IsolationInfo::Deserialize(info.SerializeAsString()), std::nullopt);

  info.set_request_type(-42);
  EXPECT_EQ(IsolationInfo::Deserialize(info.SerializeAsString()), std::nullopt);
}

TEST_F(IsolationInfoTest, DeserializationWithFrameAncestorRelation) {
  proto::IsolationInfo info;
  info.set_request_type(0);
  info.set_top_frame_origin(kOrigin1.Serialize());
  info.set_frame_origin(kOrigin2.Serialize());

  // When there is no FrameAncestorRelation and request type is kMainFrame, one
  // is added as a "recovery" to make things consistent.
  // TODO(crbug.com/420876079): Remove this recovery when all callsites are
  // required to supply a consistent FrameAncestorRelation upon IsolationInfo
  // creation.
  auto deserialized = IsolationInfo::Deserialize(info.SerializeAsString());
  ASSERT_TRUE(deserialized);
  EXPECT_EQ(deserialized->frame_ancestor_relation(),
            IsolationInfo::FrameAncestorRelation::kSameOrigin);

  // When there is no FrameAncestorRelation and request type is NOT kMainFrame,
  // FrameAncestorRelation should be nullopt.
  info.set_request_type(1);
  deserialized = IsolationInfo::Deserialize(info.SerializeAsString());
  ASSERT_TRUE(deserialized);
  EXPECT_EQ(deserialized->frame_ancestor_relation(), std::nullopt);
}

TEST_F(IsolationInfoTest, OriginRelationToFrameAncestorRelation) {
  EXPECT_EQ(IsolationInfo::OriginRelationToFrameAncestorRelation(
                /*origin_relation_value=*/std::nullopt),
            std::nullopt);
  EXPECT_EQ(IsolationInfo::OriginRelationToFrameAncestorRelation(
                OriginRelation::kSameOrigin),
            IsolationInfo::FrameAncestorRelation::kSameOrigin);
  EXPECT_EQ(IsolationInfo::OriginRelationToFrameAncestorRelation(
                OriginRelation::kSameSite),
            IsolationInfo::FrameAncestorRelation::kSameSite);
  EXPECT_EQ(IsolationInfo::OriginRelationToFrameAncestorRelation(
                OriginRelation::kCrossSite),
            IsolationInfo::FrameAncestorRelation::kCrossSite);
}

TEST_F(IsolationInfoTest, ComputeNewFrameAncestorRelation) {
  // If cur_relation is nullopt, nullopt should be returned..
  EXPECT_EQ(IsolationInfo::ComputeNewFrameAncestorRelation(
                /*cur_relation=*/std::nullopt, kOrigin1, kOrigin2),
            std::nullopt);

  // kSameOrigin is replaced by kSameSite.
  EXPECT_EQ(
      IsolationInfo::ComputeNewFrameAncestorRelation(
          IsolationInfo::FrameAncestorRelation::kSameSite, kOrigin1, kSite1),
      IsolationInfo::FrameAncestorRelation::kSameSite);

  // kCrossSite is not replaced by kSameSite.
  EXPECT_EQ(
      IsolationInfo::ComputeNewFrameAncestorRelation(
          IsolationInfo::FrameAncestorRelation::kCrossSite, kOrigin1, kSite1),
      IsolationInfo::FrameAncestorRelation::kCrossSite);
}

TEST_F(IsolationInfoTest, ValidateFrameAncestorRelationForRedirects) {
  const struct {
    IsolationInfo::RequestType request_type;
    std::optional<IsolationInfo::FrameAncestorRelation> frame_ancestor_relation;
    std::string_view desc;
  } kTestCases[]{
      {IsolationInfo::RequestType::kSubFrame,
       IsolationInfo::FrameAncestorRelation::kSameOrigin,
       "kSubframe request frame ancestor relation remains kSameOrigin across "
       "redirect"},

      {IsolationInfo::RequestType::kSubFrame,
       IsolationInfo::FrameAncestorRelation::kCrossSite,
       "kSubframe request frame ancestor relation remains kCrossSite across "
       "redirect"},

      {IsolationInfo::RequestType::kSubFrame, std::nullopt,
       "kSubframe request frame ancestor relation remains nullopt across "
       "redirect"},

      {IsolationInfo::RequestType::kOther,
       IsolationInfo::FrameAncestorRelation::kSameOrigin,
       "kOther request frame ancestor relation remains kSameOrigin across "
       "redirect"},

      {IsolationInfo::RequestType::kOther,
       IsolationInfo::FrameAncestorRelation::kCrossSite,
       "kOther request frame ancestor relation remains kCrossSite across "
       "redirect"},

      {IsolationInfo::RequestType::kOther, std::nullopt,
       "kOther request frame ancestor relation remains nullopt across "
       "redirect"},

      {IsolationInfo::RequestType::kMainFrame,
       IsolationInfo::FrameAncestorRelation::kSameOrigin,
       "kMainFrame request frame ancestor relation remains kSameOrigin across "
       "redirect"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);
    IsolationInfo isolation_info = IsolationInfo::Create(
        test_case.request_type, kOrigin1, kOrigin1,
        SiteForCookies::FromOrigin(kOrigin1),
        /*nonce=*/std::nullopt, NetworkIsolationPartition::kGeneral,
        test_case.frame_ancestor_relation);
    EXPECT_EQ(isolation_info.frame_ancestor_relation(),
              test_case.frame_ancestor_relation);

    IsolationInfo intermediate_isolation_info =
        isolation_info.CreateForRedirect(kOrigin2);
    EXPECT_EQ(intermediate_isolation_info.frame_ancestor_relation(),
              test_case.frame_ancestor_relation);

    IsolationInfo final_isolation_info =
        intermediate_isolation_info.CreateForRedirect(kOrigin1);
    EXPECT_EQ(final_isolation_info.frame_ancestor_relation(),
              test_case.frame_ancestor_relation);
  }
}

}  // namespace

}  // namespace net
