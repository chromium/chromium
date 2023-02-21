// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/isolation_info.h"

#include <iostream>
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace net {

// `IsolationInfoEnabledFeatureFlagsTestingParam ` allows enabling and disabling
// the feature flags that control the key schemes for NetworkAnonymizationKey.
// This allows us to test the possible combinations of flags that will be
// allowed for experimentation.
//
// Presently, only one flag is used, but future experiments will add more.
struct IsolationInfoEnabledFeatureFlagsTestingParam {
  const bool enableDoubleKeyAndCrossSiteBitNetworkAnonymizationKey;
};

const IsolationInfoEnabledFeatureFlagsTestingParam kFlagsParam[] = {
    // 0. Double-keying is enabled for NetworkAnonymizationKey.
    {/*enableDoubleKeyAndCrossSiteBitNetworkAnonymizationKey=*/false},
    // 1. Double-keying + cross-site-bit is enabled for NetworkAnonymizationKey.
    {/*enableDoubleKeyAndCrossSiteBitNetworkAnonymizationKey=*/true}};

namespace {

class IsolationInfoTest : public testing::Test,
                          public testing::WithParamInterface<
                              IsolationInfoEnabledFeatureFlagsTestingParam> {
 public:
  IsolationInfoTest() {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};

    if (IsDoubleKeyAndCrossSiteBitNetworkAnonymizationKeyEnabled()) {
      enabled_features.push_back(
          net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
    } else {
      disabled_features.push_back(
          net::features::kEnableCrossSiteFlagNetworkAnonymizationKey);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  static bool IsDoubleKeyAndCrossSiteBitNetworkAnonymizationKeyEnabled() {
    return GetParam().enableDoubleKeyAndCrossSiteBitNetworkAnonymizationKey;
  }

  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://a.foo.test"));
  const url::Origin kSite1 = url::Origin::Create(GURL("https://foo.test"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://b.bar.test"));
  const url::Origin kSite2 = url::Origin::Create(GURL("https://bar.test"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("https://c.baz.test"));
  const url::Origin kOpaqueOrigin;

  const absl::optional<std::set<net::SchemefulSite>> kPartyContextNull =
      absl::nullopt;
  const absl::optional<std::set<net::SchemefulSite>> kPartyContextEmpty =
      std::set<net::SchemefulSite>();
  const absl::optional<std::set<net::SchemefulSite>> kPartyContext1 =
      std::set<net::SchemefulSite>{net::SchemefulSite(kOrigin1)};
  const absl::optional<std::set<net::SchemefulSite>> kPartyContext2 =
      std::set<net::SchemefulSite>{net::SchemefulSite(kOrigin2)};
  const absl::optional<std::set<net::SchemefulSite>> kPartyContext3 =
      std::set<net::SchemefulSite>{net::SchemefulSite(kOrigin3)};
  const absl::optional<std::set<net::SchemefulSite>> kPartyContextMultiple =
      std::set<net::SchemefulSite>{net::SchemefulSite(kOrigin1),
                                   net::SchemefulSite(kOrigin2)};

  const base::UnguessableToken kNonce1 = base::UnguessableToken::Create();
  const base::UnguessableToken kNonce2 = base::UnguessableToken::Create();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         IsolationInfoTest,
                         /*IsolationInfoEnabledFeatureFlagsTestingParam */
                         testing::ValuesIn(kFlagsParam));

void DuplicateAndCompare(const IsolationInfo& isolation_info) {
  absl::optional<IsolationInfo> duplicate_isolation_info =
      IsolationInfo::CreateIfConsistent(
          isolation_info.request_type(), isolation_info.top_frame_origin(),
          net::IsolationInfo::IsFrameSiteEnabled()
              ? isolation_info.frame_origin()
              : absl::nullopt,
          isolation_info.site_for_cookies(), isolation_info.party_context(),
          isolation_info.nonce().has_value() ? &isolation_info.nonce().value()
                                             : nullptr);

  ASSERT_TRUE(duplicate_isolation_info);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(*duplicate_isolation_info));
}

TEST_P(IsolationInfoTest, IsFrameSiteEnabled) {
  EXPECT_TRUE(IsolationInfo::IsFrameSiteEnabled());
}

TEST_P(IsolationInfoTest, DebugString) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1), kPartyContextEmpty, &kNonce1);
  std::vector<std::string> parts;
  parts.push_back(
      "request_type: kMainFrame; top_frame_origin: https://a.foo.test; ");
  if (IsolationInfo::IsFrameSiteEnabled()) {
    parts.push_back("frame_origin: https://b.bar.test; ");
  }
  parts.push_back("network_anonymization_key: ");
  parts.push_back(isolation_info.network_anonymization_key().ToDebugString());
  parts.push_back("; network_isolation_key: ");
  parts.push_back(isolation_info.network_isolation_key().ToDebugString());
  parts.push_back("; party_context: {}; nonce: ");
  parts.push_back(isolation_info.nonce().value().ToString());
  parts.push_back(
      "; site_for_cookies: SiteForCookies: {site=https://foo.test; "
      "schemefully_same=true}");
  EXPECT_EQ(isolation_info.DebugString(), base::StrCat(parts));
}

TEST_P(IsolationInfoTest, CreateNetworkAnonymizationKeyForIsolationInfo) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1), kPartyContextEmpty, &kNonce1);
  NetworkAnonymizationKey nak =
      isolation_info.CreateNetworkAnonymizationKeyForIsolationInfo(
          kOrigin1, kOrigin2, &kNonce1);

  IsolationInfo same_site_isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin1), kPartyContextEmpty, &kNonce1);

  // Top frame should be populated regardless of scheme.
  EXPECT_EQ(nak.GetTopFrameSite(), SchemefulSite(kOrigin1));
  EXPECT_EQ(isolation_info.top_frame_origin(), kOrigin1);
  EXPECT_EQ(isolation_info.network_anonymization_key().GetTopFrameSite(),
            SchemefulSite(kOrigin1));

  // Nonce should be empty regardless of scheme
  EXPECT_EQ(nak.GetNonce().value(), kNonce1);
  EXPECT_EQ(isolation_info.network_anonymization_key().GetNonce().value(),
            kNonce1);
  EXPECT_EQ(isolation_info.nonce().value(), kNonce1);

  if (!IsDoubleKeyAndCrossSiteBitNetworkAnonymizationKeyEnabled()) {
    // Triple-keyed IsolationInfo + double-keyed NetworkAnonymizationKey case.
    EXPECT_EQ(isolation_info.frame_origin(), kOrigin2);
  } else {
    // Triple-keyed IsolationInfo + double-keyed + cross site bit
    // NetworkAnonymizationKey case.
    EXPECT_EQ(isolation_info.frame_origin(), kOrigin2);
    EXPECT_EQ(isolation_info.network_anonymization_key().GetIsCrossSite(),
              true);
    EXPECT_EQ(
        same_site_isolation_info.network_anonymization_key().GetIsCrossSite(),
        false);
  }
}

TEST_P(IsolationInfoTest, RequestTypeMainFrame) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin1), kPartyContextEmpty);
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
  EXPECT_EQ(kPartyContextEmpty, isolation_info.party_context());
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
  EXPECT_EQ(kPartyContextEmpty, redirected_isolation_info.party_context());
  EXPECT_FALSE(redirected_isolation_info.nonce().has_value());
}

TEST_P(IsolationInfoTest, RequestTypeSubFrame) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kSubFrame, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1), kPartyContext1);
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
  EXPECT_EQ(kPartyContext1, isolation_info.party_context());
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
  EXPECT_EQ(kPartyContext1, isolation_info.party_context());
  EXPECT_FALSE(redirected_isolation_info.nonce().has_value());
}

TEST_P(IsolationInfoTest, RequestTypeMainFrameWithNonce) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin1), kPartyContextEmpty, &kNonce1);
  EXPECT_EQ(IsolationInfo::RequestType::kMainFrame,
            isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ(absl::nullopt,
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_EQ(kPartyContextEmpty, isolation_info.party_context());
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
      absl::nullopt,
      redirected_isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(redirected_isolation_info.site_for_cookies().IsFirstParty(
      kOrigin3.GetURL()));
  EXPECT_EQ(kPartyContextEmpty, redirected_isolation_info.party_context());
  EXPECT_EQ(kNonce1, redirected_isolation_info.nonce().value());
}

TEST_P(IsolationInfoTest, RequestTypeSubFrameWithNonce) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kSubFrame, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1), kPartyContext1, &kNonce1);
  EXPECT_EQ(IsolationInfo::RequestType::kSubFrame,
            isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin2, isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ(absl::nullopt,
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_EQ(kPartyContext1, isolation_info.party_context());
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
      absl::nullopt,
      redirected_isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(redirected_isolation_info.site_for_cookies().IsFirstParty(
      kOrigin1.GetURL()));
  EXPECT_EQ(kPartyContext1, redirected_isolation_info.party_context());
  EXPECT_EQ(kNonce1, redirected_isolation_info.nonce().value());
}

TEST_P(IsolationInfoTest, RequestTypeOther) {
  IsolationInfo isolation_info;
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_FALSE(isolation_info.top_frame_origin());
  EXPECT_FALSE(isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsEmpty());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.party_context());
  EXPECT_FALSE(isolation_info.nonce());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_P(IsolationInfoTest, RequestTypeOtherWithSiteForCookies) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kOther, kOrigin1, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin1), kPartyContextEmpty);
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_EQ("https://foo.test https://foo.test",
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_EQ(kPartyContextEmpty, isolation_info.party_context());
  EXPECT_FALSE(isolation_info.nonce());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

// Test case of a subresource for cross-site subframe (which has an empty
// site-for-cookies).
TEST_P(IsolationInfoTest, RequestTypeOtherWithEmptySiteForCookies) {
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, kOrigin1,
                            kOrigin2, SiteForCookies(), kPartyContext2);
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin2, isolation_info.frame_origin());
  EXPECT_EQ("https://foo.test https://bar.test",
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_EQ(kPartyContext2, isolation_info.party_context());
  EXPECT_FALSE(isolation_info.nonce());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_P(IsolationInfoTest, CreateTransient) {
  IsolationInfo isolation_info = IsolationInfo::CreateTransient();
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_TRUE(isolation_info.top_frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.party_context());
  EXPECT_FALSE(isolation_info.nonce());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_P(IsolationInfoTest, CreateForInternalRequest) {
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
  EXPECT_EQ(kPartyContextEmpty, isolation_info.party_context());
  EXPECT_FALSE(isolation_info.nonce());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

// Test that in the UpdateNothing case, the SiteForCookies does not have to
// match the frame origin, unlike in the HTTP/HTTPS case.
TEST_P(IsolationInfoTest, CustomSchemeRequestTypeOther) {
  // Have to register the scheme, or url::Origin::Create() will return an
  // opaque origin.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("foo", url::SCHEME_WITH_HOST);

  const GURL kCustomOriginUrl = GURL("foo://a.foo.com");
  const url::Origin kCustomOrigin = url::Origin::Create(kCustomOriginUrl);

  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RequestType::kOther, kCustomOrigin, kOrigin1,
      SiteForCookies::FromOrigin(kCustomOrigin), kPartyContext1);
  EXPECT_EQ(IsolationInfo::RequestType::kOther, isolation_info.request_type());
  EXPECT_EQ(kCustomOrigin, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_EQ("foo://a.foo.com https://foo.test",
            isolation_info.network_isolation_key().ToCacheKeyString());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsFirstParty(kCustomOriginUrl));
  EXPECT_EQ(kPartyContext1, isolation_info.party_context());
  EXPECT_FALSE(isolation_info.nonce());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin2);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

// Success cases are covered by other tests, so only need a separate test to
// cover the failure cases.
TEST_P(IsolationInfoTest, CreateIfConsistentFails) {
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
      IsolationInfo::RequestType::kOther, absl::nullopt, absl::nullopt,
      SiteForCookies()));

  // Incorrectly have empty/non-empty origins:
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, absl::nullopt, kOrigin1,
      SiteForCookies()));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kSubFrame, absl::nullopt, kOrigin2,
      SiteForCookies()));

  // Empty frame origins are incorrect.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, kOrigin1, absl::nullopt,
      SiteForCookies()));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kSubFrame, kOrigin1, absl::nullopt,
      SiteForCookies()));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kMainFrame, kOrigin1, absl::nullopt,
      SiteForCookies::FromOrigin(kOrigin1)));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1)));

  // No origins with non-null SiteForCookies.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, absl::nullopt, absl::nullopt,
      SiteForCookies::FromOrigin(kOrigin1)));

  // No origins with non-null party_context.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, absl::nullopt, absl::nullopt,
      SiteForCookies(), kPartyContextEmpty));

  // No origins with non-null nonce.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RequestType::kOther, absl::nullopt, absl::nullopt,
      SiteForCookies(), absl::nullopt /* party_context */, &kNonce1));
}

TEST_P(IsolationInfoTest, CreateForRedirectPartyContext) {
  // RequestTypeMainFrame, PartyContext is empty
  {
    IsolationInfo isolation_info = IsolationInfo::Create(
        IsolationInfo::RequestType::kMainFrame, kOrigin1, kOrigin1,
        SiteForCookies::FromOrigin(kOrigin1), kPartyContextEmpty);
    IsolationInfo redirected_isolation_info =
        isolation_info.CreateForRedirect(kOrigin3);
    EXPECT_EQ(kPartyContextEmpty, redirected_isolation_info.party_context());
  }
  // RequestTypeSubFrame, PartyContext is empty
  {
    IsolationInfo isolation_info = IsolationInfo::Create(
        IsolationInfo::RequestType::kSubFrame, kOrigin1, kOrigin2,
        SiteForCookies::FromOrigin(kOrigin1), kPartyContextEmpty);
    IsolationInfo redirected_isolation_info =
        isolation_info.CreateForRedirect(kOrigin3);
    EXPECT_EQ(kPartyContextEmpty, redirected_isolation_info.party_context());
  }
  // RequestTypeSubFrame, PartyContext not empty
  {
    IsolationInfo isolation_info = IsolationInfo::Create(
        IsolationInfo::RequestType::kSubFrame, kOrigin1, kOrigin2,
        SiteForCookies::FromOrigin(kOrigin1), kPartyContext1);
    IsolationInfo redirected_isolation_info =
        isolation_info.CreateForRedirect(kOrigin3);
    EXPECT_EQ(kPartyContext1, redirected_isolation_info.party_context());
  }
  // RequestTypeOther, PartyContext not empty
  {
    IsolationInfo isolation_info =
        IsolationInfo::Create(IsolationInfo::RequestType::kOther, kOrigin1,
                              kOrigin2, SiteForCookies(), kPartyContext2);
    IsolationInfo redirected_isolation_info =
        isolation_info.CreateForRedirect(kOrigin3);
    EXPECT_EQ(kPartyContext2, redirected_isolation_info.party_context());
  }
}

TEST_P(IsolationInfoTest, Serialization) {
  EXPECT_FALSE(IsolationInfo::Deserialize(""));
  EXPECT_FALSE(IsolationInfo::Deserialize("garbage"));

  const IsolationInfo kPositiveTestCases[] = {
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1),
                            kPartyContext1),
      // Null party context
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1),
                            kPartyContextNull),
      // Empty party context
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1),
                            kPartyContextEmpty),
      // Multiple party context entries.
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1),
                            kPartyContextMultiple),
      // Without SiteForCookies
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies(), absl::nullopt),
      // Request type kOther
      IsolationInfo::Create(IsolationInfo::RequestType::kOther, kOrigin1,
                            kOrigin1, SiteForCookies::FromOrigin(kOrigin1),
                            absl::nullopt),
      // Request type kMainframe
      IsolationInfo::Create(IsolationInfo::RequestType::kMainFrame, kOrigin1,
                            kOrigin1, SiteForCookies::FromOrigin(kOrigin1),
                            absl::nullopt),
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
                            kPartyContext1, &kNonce1),
      // With an opaque origin (i.e transient)
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            url::Origin(), SiteForCookies::FromOrigin(kOrigin1),
                            absl::nullopt),
  };
  const IsolationInfo kNegativeWhenDoubleKeyEnabledTestCases[] = {
      IsolationInfo::CreateTransient(),
      // With nonce (i.e transient).
      IsolationInfo::Create(IsolationInfo::RequestType::kSubFrame, kOrigin1,
                            kOrigin2, SiteForCookies::FromOrigin(kOrigin1),
                            kPartyContext1, &kNonce1),
  };
  for (const auto& info : kNegativeTestCases) {
    EXPECT_TRUE(info.Serialize().empty());
  }
}

}  // namespace

}  // namespace net
