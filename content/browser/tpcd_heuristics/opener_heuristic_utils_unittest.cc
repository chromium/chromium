// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tpcd_heuristics/opener_heuristic_utils.h"

#include "base/test/scoped_feature_list.h"
#include "content/public/browser/cookie_access_details.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TEST(OpenerHeuristicUtilsTest, GetPopupProvider) {
  // Any google.com subdomain.
  EXPECT_EQ(GetPopupProvider(GURL("https://accounts.google.com/")),
            PopupProvider::kGoogle);
  EXPECT_EQ(GetPopupProvider(GURL("https://www.google.com/")),
            PopupProvider::kGoogle);
  // Also match http (just in case).
  EXPECT_EQ(GetPopupProvider(GURL("http://www.google.com/")),
            PopupProvider::kGoogle);

  // If not a known provider, return kUnknown.
  EXPECT_EQ(GetPopupProvider(GURL("https://www.example.com/")),
            PopupProvider::kUnknown);
}

TEST(IsAdTaggedCookieForHeuristics, ReturnsCorrectlyInExperiment) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      network::features::kSkipTpcdMitigationsForAds,
      {{"SkipTpcdMitigationsForAdsHeuristics", "true"}});

  CookieAccessDetails details;
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kFalse);

  details.cookie_setting_overrides.Put(
      net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kTrue);
}

TEST(IsAdTaggedCookieForHeuristics, ReturnsCorrectlyWithoutExperimentFeature) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(network::features::kSkipTpcdMitigationsForAds);

  CookieAccessDetails details;
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kUnknown);

  details.cookie_setting_overrides.Put(
      net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kUnknown);
}

TEST(IsAdTaggedCookieForHeuristics, ReturnsCorrectlyWithoutExperimentParam) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      network::features::kSkipTpcdMitigationsForAds,
      {{"SkipTpcdMitigationsForAdsHeuristics", "false"}});

  CookieAccessDetails details;
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kUnknown);

  details.cookie_setting_overrides.Put(
      net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);
  EXPECT_EQ(IsAdTaggedCookieForHeuristics(details), OptionalBool::kUnknown);
}

}  // namespace content
