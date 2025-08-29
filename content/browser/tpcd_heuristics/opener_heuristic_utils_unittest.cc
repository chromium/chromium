// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tpcd_heuristics/opener_heuristic_utils.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/common/features.h"
#include "content/browser/btm/btm_bounce_detector.h"
#include "content/public/browser/btm_redirect_info.h"
#include "content/public/browser/cookie_access_details.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using content::Btm3PcSettingsCallback;
using content::BtmDataAccessType;
using content::BtmRedirectChainInfoPtr;
using content::BtmRedirectInfo;
using content::BtmRedirectInfoPtr;

using ChainPair =
    std::pair<BtmRedirectChainInfoPtr, std::vector<BtmRedirectInfoPtr>>;

void AppendChainPair(std::vector<ChainPair>& vec,
                     std::vector<BtmRedirectInfoPtr> redirects,
                     BtmRedirectChainInfoPtr chain) {
  vec.emplace_back(std::move(chain), std::move(redirects));
}

std::vector<BtmRedirectInfoPtr> MakeServerRedirects(
    std::vector<std::string> urls,
    BtmDataAccessType access_type = BtmDataAccessType::kReadWrite) {
  std::vector<BtmRedirectInfoPtr> redirects;
  for (const auto& url : urls) {
    redirects.push_back(BtmRedirectInfo::CreateForServer(
        /*redirector_url=*/GURL(url),
        /*redirector_source_id=*/ukm::AssignNewSourceId(),
        /*access_type=*/access_type,
        /*time=*/base::Time::Now(),
        /*was_response_cached=*/false,
        /*response_code=*/net::HTTP_FOUND,
        /*server_bounce_delay=*/base::TimeDelta()));
  }
  return redirects;
}

BtmRedirectInfoPtr MakeClientRedirect(
    std::string url,
    BtmDataAccessType access_type = BtmDataAccessType::kReadWrite,
    bool has_sticky_activation = false,
    bool has_web_authn_assertion = false) {
  return BtmRedirectInfo::CreateForClient(
      /*redirector_url=*/GURL(url),
      /*redirector_source_id=*/ukm::AssignNewSourceId(),
      /*access_type=*/access_type,
      /*time=*/base::Time::Now(),
      /*client_bounce_delay=*/base::Seconds(1),
      /*has_sticky_activation=*/has_sticky_activation,
      /*web_authn_assertion_request_succeeded*/ has_web_authn_assertion);
}

Btm3PcSettingsCallback GetAre3pcsAllowedCallback() {
  return base::BindRepeating([] { return false; });
}
}  // namespace

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

TEST(BtmRedirectContextTest, GetRedirectHeuristicURLs_NoRequirements) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      content_settings::features::kTpcdHeuristicsGrants,
      {{"TpcdRedirectHeuristicRequireABAFlow", "false"}});

  GURL first_party_url = GURL("http://a.test/");
  ukm::SourceId first_party_source_id = ukm::AssignNewSourceId();
  GURL current_interaction_url = GURL("http://b.test/");
  ukm::SourceId current_interaction_source_id = ukm::AssignNewSourceId();
  GURL no_current_interaction_url("http://c.test/");

  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);

  context.AppendCommitted(
      std::make_pair(first_party_url, first_party_source_id),
      {MakeServerRedirects({"http://c.test"})}, current_interaction_url,
      current_interaction_source_id, false);
  context.AppendCommitted(
      MakeClientRedirect("http://b.test/", BtmDataAccessType::kNone,
                         /*has_sticky_activation=*/true),
      {}, first_party_url, first_party_source_id, false);

  ASSERT_EQ(context.size(), 2u);

  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction =
          GetRedirectHeuristicURLs(context, first_party_url, std::nullopt,
                                   /*require_current_interaction=*/false);
  EXPECT_THAT(
      sites_to_url_and_current_interaction,
      testing::UnorderedElementsAre(
          std::pair<std::string, std::pair<GURL, bool>>(
              "b.test", std::make_pair(current_interaction_url, true)),
          std::pair<std::string, std::pair<GURL, bool>>(
              "c.test", std::make_pair(no_current_interaction_url, false))));
}

TEST(BtmRedirectContextTest, GetRedirectHeuristicURLs_RequireABAFlow) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      content_settings::features::kTpcdHeuristicsGrants,
      {{"TpcdRedirectHeuristicRequireABAFlow", "true"}});

  GURL first_party_url = GURL("http://a.test/");
  ukm::SourceId first_party_source_id = ukm::AssignNewSourceId();
  GURL aba_url("http://b.test/");
  GURL no_aba_url("http://c.test/");

  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);

  context.AppendCommitted(
      std::make_pair(first_party_url, first_party_source_id),
      {MakeServerRedirects({"http://b.test", "http://c.test"})},
      first_party_url, first_party_source_id, false);

  ASSERT_EQ(context.size(), 2u);

  std::set<std::string> allowed_sites = {GetSiteForBtm(aba_url)};

  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction =
          GetRedirectHeuristicURLs(context, first_party_url, allowed_sites,
                                   /*require_current_interaction=*/false);
  EXPECT_THAT(sites_to_url_and_current_interaction,
              testing::UnorderedElementsAre(
                  std::pair<std::string, std::pair<GURL, bool>>(
                      "b.test", std::make_pair(aba_url, false))));
}

TEST(BtmRedirectContextTest,
     GetRedirectHeuristicURLs_RequireCurrentInteraction) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      content_settings::features::kTpcdHeuristicsGrants,
      {{"TpcdRedirectHeuristicRequireABAFlow", "false"}});

  GURL first_party_url = GURL("http://a.test/");
  ukm::SourceId first_party_source_id = ukm::AssignNewSourceId();
  GURL current_interaction_url = GURL("http://b.test/");
  ukm::SourceId current_interaction_source_id = ukm::AssignNewSourceId();
  GURL no_current_interaction_url("http://c.test/");

  std::vector<ChainPair> chains;
  BtmRedirectContext context(
      base::BindRepeating(AppendChainPair, std::ref(chains)), base::DoNothing(),
      GetAre3pcsAllowedCallback(), GURL(), ukm::kInvalidSourceId,
      /*redirect_prefix_count=*/0);

  context.AppendCommitted(
      std::make_pair(first_party_url, first_party_source_id),
      {MakeServerRedirects({"http://c.test"})}, current_interaction_url,
      current_interaction_source_id, false);
  context.AppendCommitted(
      MakeClientRedirect("http://b.test/", BtmDataAccessType::kNone,
                         /*has_sticky_activation=*/false, true),
      {}, first_party_url, first_party_source_id, false);

  ASSERT_EQ(context.size(), 2u);

  std::map<std::string, std::pair<GURL, bool>>
      sites_to_url_and_current_interaction =
          GetRedirectHeuristicURLs(context, first_party_url, std::nullopt,
                                   /*require_current_interaction=*/true);
  EXPECT_THAT(
      sites_to_url_and_current_interaction,
      testing::UnorderedElementsAre(
          std::pair<std::string, std::pair<GURL, bool>>(
              "b.test", std::make_pair(current_interaction_url, true))));
}

}  // namespace content
