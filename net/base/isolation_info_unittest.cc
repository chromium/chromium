// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/isolation_info.h"

#include "base/optional.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace net {

namespace {

void DuplicateAndCompare(const IsolationInfo& isolation_info) {
  base::Optional<IsolationInfo> duplicate_isolation_info =
      IsolationInfo::CreateIfConsistent(
          isolation_info.redirect_mode(), isolation_info.top_frame_origin(),
          isolation_info.frame_origin(), isolation_info.site_for_cookies(),
          isolation_info.opaque_and_non_transient());

  ASSERT_TRUE(duplicate_isolation_info);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(*duplicate_isolation_info));
}

class IsolationInfoTest : public testing::Test {
 public:
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://a.foo.test"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://b.bar.test"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("https://c.baz.test"));
  const url::Origin kOpaqueOrigin;
};

TEST_F(IsolationInfoTest, UpdateTopFrame) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RedirectMode::kUpdateTopFrame, kOrigin1, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin1));
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateTopFrame,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ("https://foo.test https://foo.test",
            isolation_info.network_isolation_key().ToString());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateTopFrame,
            redirected_isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin3, redirected_isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin3, redirected_isolation_info.frame_origin());
  EXPECT_TRUE(
      redirected_isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(redirected_isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ("https://baz.test https://baz.test",
            redirected_isolation_info.network_isolation_key().ToString());
  EXPECT_TRUE(redirected_isolation_info.site_for_cookies().IsFirstParty(
      kOrigin3.GetURL()));
  EXPECT_FALSE(redirected_isolation_info.opaque_and_non_transient());
}

TEST_F(IsolationInfoTest, UpdateFrameOnly) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RedirectMode::kUpdateFrameOnly, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1));
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateFrameOnly,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin2, isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ("https://foo.test https://bar.test",
            isolation_info.network_isolation_key().ToString());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateFrameOnly,
            redirected_isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, redirected_isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin3, redirected_isolation_info.frame_origin());
  EXPECT_TRUE(
      redirected_isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(redirected_isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ("https://foo.test https://baz.test",
            redirected_isolation_info.network_isolation_key().ToString());
  EXPECT_TRUE(redirected_isolation_info.site_for_cookies().IsFirstParty(
      kOrigin1.GetURL()));
  EXPECT_FALSE(redirected_isolation_info.opaque_and_non_transient());
}

TEST_F(IsolationInfoTest, UpdateNothing) {
  IsolationInfo isolation_info;
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_FALSE(isolation_info.top_frame_origin());
  EXPECT_FALSE(isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsEmpty());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_F(IsolationInfoTest, UpdateNothingWithSiteForCookies) {
  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RedirectMode::kUpdateNothing, kOrigin1, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin1));
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ("https://foo.test https://foo.test",
            isolation_info.network_isolation_key().ToString());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

// Test case of a subresource for cross-site subframe (which has an empty
// site-for-cookies).
TEST_F(IsolationInfoTest, UpdateNothingWithEmptySiteForCookies) {
  IsolationInfo isolation_info =
      IsolationInfo::Create(IsolationInfo::RedirectMode::kUpdateNothing,
                            kOrigin1, kOrigin2, SiteForCookies());
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin2, isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ("https://foo.test https://bar.test",
            isolation_info.network_isolation_key().ToString());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_F(IsolationInfoTest, CreateTransient) {
  IsolationInfo isolation_info = IsolationInfo::CreateTransient();
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_TRUE(isolation_info.top_frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_F(IsolationInfoTest, CreateOpaqueAndNonTransient) {
  IsolationInfo isolation_info = IsolationInfo::CreateOpaqueAndNonTransient();
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_TRUE(isolation_info.top_frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.frame_origin()->opaque());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_TRUE(
      isolation_info.network_isolation_key().GetTopFrameOrigin()->opaque());
  EXPECT_TRUE(
      isolation_info.network_isolation_key().GetFrameOrigin()->opaque());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_TRUE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_F(IsolationInfoTest, CreateForInternalRequest) {
  IsolationInfo isolation_info =
      IsolationInfo::CreateForInternalRequest(kOrigin1);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ("https://foo.test https://foo.test",
            isolation_info.network_isolation_key().ToString());
  EXPECT_TRUE(
      isolation_info.site_for_cookies().IsFirstParty(kOrigin1.GetURL()));
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);

  IsolationInfo redirected_isolation_info =
      isolation_info.CreateForRedirect(kOrigin3);
  EXPECT_TRUE(isolation_info.IsEqualForTesting(redirected_isolation_info));
}

TEST_F(IsolationInfoTest, CreatePartialUpdateTopFrame) {
  const NetworkIsolationKey kNIK(kOrigin1, kOrigin1);
  IsolationInfo isolation_info = IsolationInfo::CreatePartial(
      IsolationInfo::RedirectMode::kUpdateTopFrame, kNIK);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateTopFrame,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_EQ(kNIK, isolation_info.network_isolation_key());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);
}

TEST_F(IsolationInfoTest, CreatePartialUpdateFrameOnly) {
  const NetworkIsolationKey kNIK(kOrigin1, kOrigin2);
  IsolationInfo isolation_info = IsolationInfo::CreatePartial(
      IsolationInfo::RedirectMode::kUpdateFrameOnly, kNIK);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateFrameOnly,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin2, isolation_info.frame_origin());
  EXPECT_EQ(kNIK, isolation_info.network_isolation_key());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);
}

TEST_F(IsolationInfoTest, CreatePartialUpdateNothing) {
  const NetworkIsolationKey kNIK(kOrigin1, kOrigin2);
  IsolationInfo isolation_info = IsolationInfo::CreatePartial(
      IsolationInfo::RedirectMode::kUpdateNothing, kNIK);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin2, isolation_info.frame_origin());
  EXPECT_EQ(kNIK, isolation_info.network_isolation_key());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);
}

TEST_F(IsolationInfoTest, CreatePartialTransient) {
  const NetworkIsolationKey kNIK = NetworkIsolationKey::CreateTransient();
  IsolationInfo isolation_info = IsolationInfo::CreatePartial(
      IsolationInfo::RedirectMode::kUpdateNothing, kNIK);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_EQ(*kNIK.GetTopFrameOrigin(), isolation_info.top_frame_origin());
  EXPECT_EQ(*kNIK.GetFrameOrigin(), isolation_info.frame_origin());
  EXPECT_EQ(kNIK, isolation_info.network_isolation_key());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);
}

TEST_F(IsolationInfoTest, CreatePartialOpaqueAndNonTransient) {
  const NetworkIsolationKey kNIK =
      NetworkIsolationKey::CreateOpaqueAndNonTransient();
  IsolationInfo isolation_info = IsolationInfo::CreatePartial(
      IsolationInfo::RedirectMode::kUpdateNothing, kNIK);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_EQ(*kNIK.GetTopFrameOrigin(), isolation_info.top_frame_origin());
  EXPECT_EQ(*kNIK.GetFrameOrigin(), isolation_info.frame_origin());
  EXPECT_EQ(kNIK, isolation_info.network_isolation_key());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_TRUE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);
}

TEST_F(IsolationInfoTest, CreatePartialEmpty) {
  IsolationInfo isolation_info = IsolationInfo::CreatePartial(
      IsolationInfo::RedirectMode::kUpdateNothing, NetworkIsolationKey());
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_FALSE(isolation_info.top_frame_origin());
  EXPECT_FALSE(isolation_info.frame_origin());
  EXPECT_EQ(NetworkIsolationKey(), isolation_info.network_isolation_key());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);
}

TEST_F(IsolationInfoTest,
       CreatePartialEmptyNoFrameOriginRedirectModeUpdateTopFrame) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  const NetworkIsolationKey kNIK(kOrigin1, kOrigin1);
  EXPECT_FALSE(kNIK.GetFrameOrigin());
  IsolationInfo isolation_info = IsolationInfo::CreatePartial(
      IsolationInfo::RedirectMode::kUpdateTopFrame, kNIK);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateTopFrame,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_EQ(kNIK, isolation_info.network_isolation_key());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);
}

TEST_F(IsolationInfoTest,
       CreatePartialEmptyNoFrameOriginRedirectModeUpdateFrameOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  const NetworkIsolationKey kNIK(kOrigin1, kOrigin2);
  EXPECT_FALSE(kNIK.GetFrameOrigin());
  IsolationInfo isolation_info = IsolationInfo::CreatePartial(
      IsolationInfo::RedirectMode::kUpdateFrameOnly, kNIK);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateFrameOnly,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  ASSERT_TRUE(isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.frame_origin()->opaque());
  EXPECT_EQ(kNIK, isolation_info.network_isolation_key());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);
}

TEST_F(IsolationInfoTest,
       CreatePartialEmptyNoFrameOriginRedirectModeUpdateNothing) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  const NetworkIsolationKey kNIK(kOrigin1, kOrigin2);
  EXPECT_FALSE(kNIK.GetFrameOrigin());
  IsolationInfo isolation_info = IsolationInfo::CreatePartial(
      IsolationInfo::RedirectMode::kUpdateNothing, kNIK);
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_EQ(kOrigin1, isolation_info.top_frame_origin());
  ASSERT_TRUE(isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.frame_origin()->opaque());
  EXPECT_EQ(kNIK, isolation_info.network_isolation_key());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

  DuplicateAndCompare(isolation_info);
}

// Test that in the UpdateNothing case, the SiteForCookies does not have to
// match the frame origin, unlike in the HTTP/HTTPS case.
TEST_F(IsolationInfoTest, CustomSchemeUpdateNothing) {
  // Have to register the scheme, or url::Origin::Create() will return an opaque
  // origin.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("foo", url::SCHEME_WITH_HOST);

  const GURL kCustomOriginUrl = GURL("foo://a.foo.com");
  const url::Origin kCustomOrigin = url::Origin::Create(kCustomOriginUrl);

  IsolationInfo isolation_info = IsolationInfo::Create(
      IsolationInfo::RedirectMode::kUpdateNothing, kCustomOrigin, kOrigin1,
      SiteForCookies::FromOrigin(kCustomOrigin));
  EXPECT_EQ(IsolationInfo::RedirectMode::kUpdateNothing,
            isolation_info.redirect_mode());
  EXPECT_EQ(kCustomOrigin, isolation_info.top_frame_origin());
  EXPECT_EQ(kOrigin1, isolation_info.frame_origin());
  EXPECT_TRUE(isolation_info.network_isolation_key().IsFullyPopulated());
  EXPECT_FALSE(isolation_info.network_isolation_key().IsTransient());
  EXPECT_EQ("foo://a.foo.com https://foo.test",
            isolation_info.network_isolation_key().ToString());
  EXPECT_TRUE(isolation_info.site_for_cookies().IsFirstParty(kCustomOriginUrl));
  EXPECT_FALSE(isolation_info.opaque_and_non_transient());

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
      IsolationInfo::RedirectMode::kUpdateTopFrame, kOrigin1, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin2),
      false /* opaque_and_non_transient */));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateTopFrame, kOpaqueOrigin,
      kOpaqueOrigin, SiteForCookies::FromOrigin(kOrigin1),
      false /* opaque_and_non_transient */));

  // Sub frame with inconsistent SiteForCookies.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateFrameOnly, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin2),
      false /* opaque_and_non_transient */));

  // Sub resources with inconsistent SiteForCookies.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateNothing, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin1),
      false /* opaque_and_non_transient */));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateNothing, kOrigin1, kOrigin2,
      SiteForCookies::FromOrigin(kOrigin2),
      false /* opaque_and_non_transient */));

  // |opaque_and_non_transient| for wrong RedirectModes.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateTopFrame, kOpaqueOrigin,
      kOpaqueOrigin, SiteForCookies(), true /* opaque_and_non_transient */));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateFrameOnly, kOpaqueOrigin,
      kOpaqueOrigin, SiteForCookies(), true /* opaque_and_non_transient */));

  // |opaque_and_non_transient| with empty origins.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateNothing, base::nullopt, base::nullopt,
      SiteForCookies(), true /* opaque_and_non_transient */));

  // |opaque_and_non_transient| with non-opaque origins.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateNothing, kOrigin1, kOrigin1,
      SiteForCookies(), true /* opaque_and_non_transient */));

  // Incorrectly have empty/non-empty origins:
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateNothing, base::nullopt, kOrigin1,
      SiteForCookies(), false /* opaque_and_non_transient */));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateNothing, kOrigin1, base::nullopt,
      SiteForCookies(), false /* opaque_and_non_transient */));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateTopFrame, base::nullopt, kOrigin1,
      SiteForCookies::FromOrigin(kOrigin1),
      false /* opaque_and_non_transient */));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateTopFrame, kOrigin1, base::nullopt,
      SiteForCookies::FromOrigin(kOrigin1),
      false /* opaque_and_non_transient */));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateFrameOnly, base::nullopt, kOrigin2,
      SiteForCookies(), false /* opaque_and_non_transient */));
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateFrameOnly, kOrigin1, base::nullopt,
      SiteForCookies(), false /* opaque_and_non_transient */));

  // No origins with non-null SiteForCookies.
  EXPECT_FALSE(IsolationInfo::CreateIfConsistent(
      IsolationInfo::RedirectMode::kUpdateNothing, base::nullopt, base::nullopt,
      SiteForCookies::FromOrigin(kOrigin1),
      false /* opaque_and_non_transient */));
}

}  // namespace

}  // namespace net
