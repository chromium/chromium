// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/ios_enterprise_interstitial.h"

#import "base/strings/strcat.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

namespace {

constexpr char kBlockDecisionHistogram[] =
    "interstitial.enterprise_block.decision";
constexpr char kWarnDecisionHistogram[] =
    "interstitial.enterprise_warn.decision";
constexpr char kTestUrl[] = "http://example.com";
constexpr char kTestMessage[] = "Test message";

class IOSEnterpriseInterstitialTest : public PlatformTest {
 public:
  security_interstitials::UnsafeResource CreateBlockUnsafeResource() {
    auto resource = CreateUnsafeResource();
    resource.threat_type =
        safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK;
    return resource;
  }

  security_interstitials::UnsafeResource CreateWarnUnsafeResource() {
    auto resource = CreateUnsafeResource();
    resource.threat_type =
        safe_browsing::SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN;
    return resource;
  }

  security_interstitials::UnsafeResource CreateUnsafeResource() {
    auto resource = security_interstitials::UnsafeResource();
    resource.url = GURL("https://example.com");
    resource.weak_web_state = web_state_.GetWeakPtr();
    return resource;
  }

  void AddCustomMessageToResource(
      security_interstitials::UnsafeResource& unsafe_resource) {
    safe_browsing::MatchedUrlNavigationRule::CustomMessage cm;
    auto* custom_segments = cm.add_message_segments();
    custom_segments->set_text(kTestMessage);
    custom_segments->set_link(kTestUrl);

    safe_browsing::RTLookupResponse response;
    auto* threat_info = response.add_threat_info();
    *threat_info->mutable_matched_url_navigation_rule()
         ->mutable_custom_message() = cm;

    unsafe_resource.rt_lookup_response = response;
  }

 protected:
  web::FakeWebState web_state_;
};

}  // namespace

TEST_F(IOSEnterpriseInterstitialTest, EnterpriseBlock_MetricsRecorded) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kBlockDecisionHistogram, 0);

  auto test_page = IOSEnterpriseInterstitial::CreateBlockingPage(
      CreateBlockUnsafeResource());
  EXPECT_TRUE(test_page->ShouldCreateNewNavigation());
  EXPECT_EQ(test_page->request_url(), GURL("https://example.com"));

  test_page->HandleCommand(security_interstitials::CMD_DONT_PROCEED);

  histograms.ExpectTotalCount(kBlockDecisionHistogram, 1);
  histograms.ExpectBucketCount(
      kBlockDecisionHistogram,
      security_interstitials::MetricsHelper::DONT_PROCEED, 1);
}

TEST_F(IOSEnterpriseInterstitialTest, EnterpriseWarn_MetricsRecorded) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kWarnDecisionHistogram, 0);

  auto test_page =
      IOSEnterpriseInterstitial::CreateWarningPage(CreateWarnUnsafeResource());
  EXPECT_TRUE(test_page->ShouldCreateNewNavigation());
  EXPECT_EQ(test_page->request_url(), GURL("https://example.com"));

  test_page->HandleCommand(security_interstitials::CMD_PROCEED);

  histograms.ExpectTotalCount(kWarnDecisionHistogram, 1);
  histograms.ExpectBucketCount(kWarnDecisionHistogram,
                               security_interstitials::MetricsHelper::PROCEED,
                               1);
}

TEST_F(IOSEnterpriseInterstitialTest, CustomMessageDisplayed) {
  std::string expected_primary_paragraph =
      base::StrCat({"Your administrator says: \"<a target=\"_blank\" href=\"",
                    kTestUrl, "\">", kTestMessage, "</a>\""});

  auto block_resource = CreateBlockUnsafeResource();
  AddCustomMessageToResource(block_resource);
  EXPECT_NE(IOSEnterpriseInterstitial::CreateBlockingPage(block_resource)
                ->GetHtmlContents()
                .find(expected_primary_paragraph),
            std::string::npos);

  auto warn_resource = CreateWarnUnsafeResource();
  AddCustomMessageToResource(warn_resource);
  EXPECT_NE(IOSEnterpriseInterstitial::CreateWarningPage(warn_resource)
                ->GetHtmlContents()
                .find(expected_primary_paragraph),
            std::string::npos);
}

}  // namespace enterprise_connectors
