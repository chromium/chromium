// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/ios_enterprise_interstitial.h"

#import "base/strings/strcat.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace enterprise_connectors {

using base::test::ios::kSpinDelaySeconds;
using base::test::ios::WaitUntilConditionOrTimeout;
using safe_browsing::SBThreatType;
using safe_browsing::ThreatSource;

namespace {

constexpr char kBlockDecisionHistogram[] =
    "interstitial.enterprise_block.decision";
constexpr char kBlockInteractionHistogram[] =
    "interstitial.enterprise_block.interaction";
constexpr char kWarnDecisionHistogram[] =
    "interstitial.enterprise_warn.decision";
constexpr char kWarnInteractionHistogram[] =
    "interstitial.enterprise_warn.interaction";
constexpr char kTestUrl[] = "http://example.com";
constexpr char kTestMessage[] = "Test message";

class IOSEnterpriseInterstitialTest : public PlatformTest {
 public:
  IOSEnterpriseInterstitialTest()
      : profile_(TestProfileIOS::Builder().Build()) {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetBrowserState(profile_.get());
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    web_state_.SetBrowserState(profile_.get());

    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
  }

  security_interstitials::UnsafeResource CreateBlockUnsafeResource() {
    auto resource = CreateUnsafeResource();
    resource.threat_type = SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK;
    return resource;
  }

  security_interstitials::UnsafeResource CreateWarnUnsafeResource() {
    auto resource = CreateUnsafeResource();
    resource.threat_type = SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN;
    return resource;
  }

  security_interstitials::UnsafeResource CreateUnsafeResource() {
    auto resource = security_interstitials::UnsafeResource();
    resource.url = GURL(kTestUrl);
    resource.weak_web_state = web_state_.GetWeakPtr();
    resource.threat_source = ThreatSource::URL_REAL_TIME_CHECK;
    return resource;
  }

  void AddCustomMessageToResource(
      security_interstitials::UnsafeResource& unsafe_resource,
      safe_browsing::RTLookupResponse::ThreatInfo::VerdictType verdict_type) {
    safe_browsing::MatchedUrlNavigationRule::CustomMessage cm;
    auto* custom_segments = cm.add_message_segments();
    custom_segments->set_text(kTestMessage);
    custom_segments->set_link(kTestUrl);

    safe_browsing::RTLookupResponse response;
    auto* threat_info = response.add_threat_info();
    threat_info->set_verdict_type(verdict_type);
    *threat_info->mutable_matched_url_navigation_rule()
         ->mutable_custom_message() = cm;

    unsafe_resource.rt_lookup_response = response;
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<ProfileIOS> profile_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
};

}  // namespace

TEST_F(IOSEnterpriseInterstitialTest, EnterpriseBlock_MetricsRecorded) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kBlockDecisionHistogram, 0);

  auto test_page = IOSEnterpriseInterstitial::CreateBlockingPage(
      CreateBlockUnsafeResource());
  EXPECT_TRUE(test_page->ShouldCreateNewNavigation());
  EXPECT_EQ(test_page->request_url(), GURL(kTestUrl));

  test_page->HandleCommand(security_interstitials::CMD_DONT_PROCEED);

  histograms.ExpectTotalCount(kBlockDecisionHistogram, 2);
  histograms.ExpectBucketCount(kBlockDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);
  histograms.ExpectBucketCount(
      kBlockDecisionHistogram,
      security_interstitials::MetricsHelper::DONT_PROCEED, 1);

  histograms.ExpectUniqueSample(
      kBlockInteractionHistogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

TEST_F(IOSEnterpriseInterstitialTest, EnterpriseWarn_MetricsRecorded) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kWarnDecisionHistogram, 0);

  auto test_page =
      IOSEnterpriseInterstitial::CreateWarningPage(CreateWarnUnsafeResource());
  EXPECT_TRUE(test_page->ShouldCreateNewNavigation());
  EXPECT_EQ(test_page->request_url(), GURL(kTestUrl));

  test_page->HandleCommand(security_interstitials::CMD_PROCEED);

  histograms.ExpectTotalCount(kWarnDecisionHistogram, 2);
  histograms.ExpectBucketCount(kWarnDecisionHistogram,
                               security_interstitials::MetricsHelper::SHOW, 1);

  histograms.ExpectBucketCount(kWarnDecisionHistogram,
                               security_interstitials::MetricsHelper::PROCEED,
                               1);

  histograms.ExpectUniqueSample(
      kWarnInteractionHistogram,
      security_interstitials::MetricsHelper::TOTAL_VISITS, 1);
}

TEST_F(IOSEnterpriseInterstitialTest, CustomMessageDisplayed) {
  std::string expected_primary_paragraph =
      base::StrCat({"Your administrator says: \"<a target=\"_blank\" href=\"",
                    kTestUrl, "\">", kTestMessage, "</a>\""});

  auto block_resource = CreateBlockUnsafeResource();
  AddCustomMessageToResource(
      block_resource, safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
  EXPECT_NE(IOSEnterpriseInterstitial::CreateBlockingPage(block_resource)
                ->GetHtmlContents()
                .find(expected_primary_paragraph),
            std::string::npos);

  auto warn_resource = CreateWarnUnsafeResource();
  AddCustomMessageToResource(warn_resource,
                             safe_browsing::RTLookupResponse::ThreatInfo::WARN);
  EXPECT_NE(IOSEnterpriseInterstitial::CreateWarningPage(warn_resource)
                ->GetHtmlContents()
                .find(expected_primary_paragraph),
            std::string::npos);
}

// Tests that the interstitial handles a do not proceed command, navigating back
// to safety.
TEST_F(IOSEnterpriseInterstitialTest, HandleDoNotProceedCommand) {
  SafeBrowsingUrlAllowList::FromWebState(&web_state_)
      ->AddPendingUnsafeNavigationDecision(
          GURL(kTestUrl), SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK);

  // Insert a safe navigation so that the page can navigate back to safety, then
  // add a navigation for the committed interstitial page.
  GURL safe_url("http://www.safe.test");
  navigation_manager_->AddItem(safe_url, ui::PAGE_TRANSITION_TYPED);
  navigation_manager_->AddItem(GURL(kTestUrl), ui::PAGE_TRANSITION_LINK);
  ASSERT_EQ(1, navigation_manager_->GetLastCommittedItemIndex());
  ASSERT_TRUE(navigation_manager_->CanGoBack());

  auto test_page = IOSEnterpriseInterstitial::CreateBlockingPage(
      CreateBlockUnsafeResource());

  test_page->HandleCommand(security_interstitials::CMD_DONT_PROCEED);

  // Verify that the NavigationManager has navigated back.
  EXPECT_EQ(0, navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_FALSE(navigation_manager_->CanGoBack());

  // Verify that there are no pending unsafe navigation decisions for the unsafe
  // url.
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  EXPECT_FALSE(allow_list->IsUnsafeNavigationDecisionPending(GURL(kTestUrl)));
}

// Tests that the interstitial handles the don't proceed command by closing the
// WebState if there is no safe NavigationItem to navigate back to.
TEST_F(IOSEnterpriseInterstitialTest, HandleDontProceedCommandWithoutSafeItem) {
  // Send the don't proceed command.
  auto test_page = IOSEnterpriseInterstitial::CreateBlockingPage(
      CreateBlockUnsafeResource());

  test_page->HandleCommand(security_interstitials::CMD_DONT_PROCEED);

  // Wait for the WebState to be closed.
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kSpinDelaySeconds,
                                          /*run_message_loop=*/true, ^{
                                            return web_state_.IsClosed();
                                          }));
}

// Tests that the interstitial handles proceed command.
TEST_F(IOSEnterpriseInterstitialTest, HandProceedCommand) {
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  allow_list->AddPendingUnsafeNavigationDecision(
      GURL(kTestUrl), SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN);

  ASSERT_FALSE(allow_list->AreUnsafeNavigationsAllowed(GURL(kTestUrl)));
  ASSERT_FALSE(navigation_manager_->ReloadWasCalled());

  auto test_page =
      IOSEnterpriseInterstitial::CreateWarningPage(CreateWarnUnsafeResource());
  test_page->HandleCommand(security_interstitials::CMD_PROCEED);

  std::set<SBThreatType> allowed_threats;
  EXPECT_TRUE(allow_list->AreUnsafeNavigationsAllowed(GURL(kTestUrl),
                                                      &allowed_threats));
  EXPECT_THAT(
      allowed_threats,
      ::testing::ElementsAre(SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN));
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());

  EXPECT_FALSE(allow_list->IsUnsafeNavigationDecisionPending(GURL(kTestUrl)));
}

// Tests that the interstitial removes pending allow list decisions if
// destroyed.
TEST_F(IOSEnterpriseInterstitialTest, RemovePendingDecisionsUponDestruction) {
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  allow_list->AddPendingUnsafeNavigationDecision(
      GURL(kTestUrl), SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN);
  std::set<safe_browsing::SBThreatType> pending_threats;
  ASSERT_TRUE(allow_list->IsUnsafeNavigationDecisionPending(GURL(kTestUrl),
                                                            &pending_threats));
  EXPECT_THAT(
      pending_threats,
      ::testing::ElementsAre(SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN));

  auto test_page =
      IOSEnterpriseInterstitial::CreateWarningPage(CreateWarnUnsafeResource());
  // Destroying interstitial should remove pending allow list decisions.
  test_page = nullptr;
  EXPECT_FALSE(allow_list->IsUnsafeNavigationDecisionPending(GURL(kTestUrl)));
}

}  // namespace enterprise_connectors
