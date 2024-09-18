// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_blocking_page.h"

#import "base/containers/contains.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/values.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/metrics_helper.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using base::test::ios::kSpinDelaySeconds;
using base::test::ios::WaitUntilConditionOrTimeout;
using safe_browsing::SBThreatType;
using security_interstitials::IOSSecurityInterstitialPage;
using security_interstitials::MetricsHelper;
using security_interstitials::SecurityInterstitialCommand;
using security_interstitials::UnsafeResource;

namespace {
// Name of the metric recorded when a malware interstitial is shown or closed.
const char kMalwareDecisionMetric[] = "interstitial.malware.decision";

// Creates an UnsafeResource for `web_state` using `url`.
UnsafeResource CreateResource(web::WebState* web_state, const GURL& url) {
  UnsafeResource resource;
  resource.url = url;
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_MALWARE;
  resource.weak_web_state = web_state->GetWeakPtr();
  resource.threat_source = safe_browsing::ThreatSource::LOCAL_PVER4;
  return resource;
}
}  // namespace

// Test fixture for SafeBrowsingBlockingPage.
class SafeBrowsingBlockingPageTest : public PlatformTest {
 public:
  SafeBrowsingBlockingPageTest()
      : profile_(TestProfileIOS::Builder().Build()),
        url_("http://www.chromium.test"),
        resource_(CreateResource(&web_state_, url_)) {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetBrowserState(profile_.get());
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    web_state_.SetBrowserState(profile_.get());
    page_ = SafeBrowsingBlockingPage::Create(resource_);
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
    SafeBrowsingUrlAllowList::FromWebState(&web_state_)
        ->AddPendingUnsafeNavigationDecision(url_, resource_.threat_type);
  }

  void SendCommand(SecurityInterstitialCommand command) {
    page_->HandleCommand(command);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<ProfileIOS> profile_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
  GURL url_;
  UnsafeResource resource_;
  std::unique_ptr<IOSSecurityInterstitialPage> page_;
  base::HistogramTester histogram_tester_;
};

// Tests that the blocking page generates HTML.
TEST_F(SafeBrowsingBlockingPageTest, GenerateHTML) {
  EXPECT_GT(page_->GetHtmlContents().size(), 0U);
}

// Tests that the blocking page handles the proceed command by updating the
// allow list and reloading the page.
TEST_F(SafeBrowsingBlockingPageTest, HandleProceedCommand) {
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  ASSERT_FALSE(allow_list->AreUnsafeNavigationsAllowed(url_));
  ASSERT_FALSE(navigation_manager_->ReloadWasCalled());

  // Send the proceed command.
  SendCommand(security_interstitials::CMD_PROCEED);

  std::set<SBThreatType> allowed_threats;
  EXPECT_TRUE(allow_list->AreUnsafeNavigationsAllowed(url_, &allowed_threats));
  EXPECT_TRUE(base::Contains(allowed_threats, resource_.threat_type));
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());

  // Verify that metrics are recorded correctly.
  histogram_tester_.ExpectTotalCount(kMalwareDecisionMetric, 2);
  histogram_tester_.ExpectBucketCount(kMalwareDecisionMetric,
                                      MetricsHelper::PROCEED, 1);
  histogram_tester_.ExpectBucketCount(kMalwareDecisionMetric,
                                      MetricsHelper::SHOW, 1);
}

// Tests that the blocking page handles the don't proceed command by navigating
// back.
TEST_F(SafeBrowsingBlockingPageTest, HandleDontProceedCommand) {
  // Insert a safe navigation so that the page can navigate back to safety, then
  // add a navigation for the committed interstitial page.
  GURL kSafeUrl("http://www.safe.test");
  navigation_manager_->AddItem(kSafeUrl, ui::PAGE_TRANSITION_TYPED);
  navigation_manager_->AddItem(url_, ui::PAGE_TRANSITION_LINK);
  ASSERT_EQ(1, navigation_manager_->GetLastCommittedItemIndex());
  ASSERT_TRUE(navigation_manager_->CanGoBack());

  // Send the don't proceed command.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);

  // Verify that the NavigationManager has navigated back.
  EXPECT_EQ(0, navigation_manager_->GetLastCommittedItemIndex());
  EXPECT_FALSE(navigation_manager_->CanGoBack());

  // Verify that metrics are recorded correctly.
  histogram_tester_.ExpectTotalCount(kMalwareDecisionMetric, 2);
  histogram_tester_.ExpectBucketCount(kMalwareDecisionMetric,
                                      MetricsHelper::DONT_PROCEED, 1);
  histogram_tester_.ExpectBucketCount(kMalwareDecisionMetric,
                                      MetricsHelper::SHOW, 1);
}

// Tests that the blocking page handles the don't proceed command by closing the
// WebState if there is no safe NavigationItem to navigate back to.
TEST_F(SafeBrowsingBlockingPageTest, HandleDontProceedCommandWithoutSafeItem) {
  // Send the don't proceed command.
  SendCommand(security_interstitials::CMD_DONT_PROCEED);

  // Wait for the WebState to be closed.  The close command run asynchronously
  // on the UI thread, so the runloop needs to be spun before it is handled.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kSpinDelaySeconds, ^{
    return web_state_.IsClosed();
  }));
}

// Tests that the blocking page removes pending allow list decisions if
// destroyed.
TEST_F(SafeBrowsingBlockingPageTest, RemovePendingDecisionsUponDestruction) {
  SafeBrowsingUrlAllowList* allow_list =
      SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  std::set<safe_browsing::SBThreatType> pending_threats;
  ASSERT_TRUE(
      allow_list->IsUnsafeNavigationDecisionPending(url_, &pending_threats));
  ASSERT_EQ(1U, pending_threats.size());
  ASSERT_TRUE(base::Contains(pending_threats, resource_.threat_type));

  page_ = nullptr;

  EXPECT_FALSE(allow_list->IsUnsafeNavigationDecisionPending(url_));
}
