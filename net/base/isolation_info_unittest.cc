// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/isolation_info.h"

#include <iostream>
#include <optional>

#include "base/strings/strcat.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "isolation_info.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
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
          isolation_info.nonce());

  ASSERT_TRUE(duplicate_isolation_info);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(*duplicate_isolation_info));
}

TEST_F(IsolationInfoTest, DebugString) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1), kNonce1);
  std::vector<std::string> parts;
  parts.push_back(
      "request_type: kMainFrame; top_frame_origin: https://a.foo.test; ");
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
}

TEST_F(IsolationInfoTest, RequestTypeOther) {
  IsolationInfo isolation_info;
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_FALSE(isolation_info.top_frame_origin());
  EXPECT_FALSE(isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsEmpty());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.nonce());

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

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_F(IsolationInfoTest, CreateTransient) {
  IsolationInfo isolation_info = IsolationInfo::CreateTransient();
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_TRUE(isolation_info.top_frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.nonce());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_F(IsolationInfoTest, CreateTransientWithNonce) {
  IsolationInfo isolation_info =
      IsolationInfo::CreateTransientWithNonce(kNonce1);
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_TRUE(isolation_info.top_frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  ASSERT_TRUE(isolation_info.nonce().has_value());
  EXPECT_EQ(isolation_info.nonce().value(), kNonce1);

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));

  IsolationInfo new_info_same_nonce =
      IsolationInfo::CreateTransientWithNonce(kNonce1);
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
      SiteForCookies::FromOrigin(kOrigin2)));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kMainFrame, kOpaqueOrigin, kOpaqueOrigin,
      SiteForCookies::FromOrigin(kOrigin1)));

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
      SiteForCookies::FromOrigin(kOrigin1)));
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
                            kOrigin1, SiteForCookies::FromOrigin(kOrigin1)),
  };
  for (const auto& info : kPositiveTestCases) {
    auto rt = IsolationInfo::Deserialize(info.Serialize());
    ASSERT_TRUE(rt);
    EXPECT_TRUE(rt->IsEqualForTesting(info));
  }

  const IsolationInfo kNegativeTestCases[] = {
      IsolationInfo::CreateTransient(),
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

}  // namespace

}  // namespace net
