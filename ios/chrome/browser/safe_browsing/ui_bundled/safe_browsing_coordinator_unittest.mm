// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/ui_bundled/safe_browsing_coordinator.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <string>
#import <string_view>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/test_future.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/infobars/core/infobar.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/safe_browsing/core/browser/suspicious_site_warning_allowlist.h"
#import "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/safe_browsing/model/suspicious_site_tab_helper.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/chrome_tailored_security_service.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/tailored_security/tailored_security_tab_helper.h"
#import "ios/chrome/browser/safe_browsing/ui_bundled/suspicious_site_warning_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

namespace safe_browsing {

namespace {

constexpr std::string_view kSuspiciousUrl =
    "https://suspicious.chromium.test/page";
constexpr std::string_view kSuspiciousHost = "suspicious.chromium.test";
constexpr std::string_view kWarningOutcomeHistogram =
    "SafeBrowsing.SuspiciousSiteWarning.WarningOutcome";
constexpr std::string_view kUserInteractionHistogram =
    "SafeBrowsing.SuspiciousSiteWarning.UserInteraction";

class MockTailoredSecurityService : public ChromeTailoredSecurityService {
 public:
  MockTailoredSecurityService(ProfileIOS& profile,
                              signin::IdentityManager* identity_manager)
      : ChromeTailoredSecurityService(&profile,
                                      identity_manager,
                                      /*sync_service=*/nullptr) {}
};

std::unique_ptr<KeyedService> BuildMockTailoredSecurityService(
    ProfileIOS& profile) {
  return std::make_unique<MockTailoredSecurityService>(
      profile, IdentityManagerFactory::GetForProfile(profile));
}

// Starts and finishes a mock navigation successfully.
void PerformFakeNavigation(const GURL& url, web::FakeWebState* web_state) {
  web::FakeNavigationContext context;
  context.SetUrl(url);
  context.SetHasCommitted(true);
  context.SetIsSameDocument(false);
  web_state->OnNavigationStarted(&context);
  web_state->OnNavigationFinished(&context);
}

}  // namespace

class SafeBrowsingCoordinatorTest : public PlatformTest {
 protected:
  SafeBrowsingCoordinatorTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        TailoredSecurityServiceFactory::GetInstance(),
        base::BindOnce(&BuildMockTailoredSecurityService));
    test_profile_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindOnce(&CreateTestSyncService));
    profile_ = std::move(test_profile_builder).Build();

    client_ = std::make_unique<FakeSafeBrowsingClient>(profile_->GetPrefs());

    browser_ = std::make_unique<TestBrowser>(profile_.get());

    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_handler_
                     forProtocol:@protocol(SettingsCommands)];

    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state->SetNavigationManager(std::move(navigation_manager));
    web_state_ = web_state.get();

    InfoBarManagerImpl::CreateForWebState(web_state_);

    // Create SafeBrowsingQueryManager.
    SafeBrowsingQueryManager::CreateForWebState(web_state_, client_.get());

    // Create SafeBrowsingTabHelper and SuspiciousSiteTabHelper.
    SafeBrowsingTabHelper::CreateForWebState(web_state_, client_.get());
    SafeBrowsingUrlAllowList::CreateForWebState(web_state_);
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(web_state_);
    SuspiciousSiteTabHelper::CreateForWebState(
        web_state_,
        ios::HostContentSettingsMapFactory::GetForProfile(profile_.get()));

    browser_->GetWebStateList()->InsertWebState(std::move(web_state));
    browser_->GetWebStateList()->ActivateWebStateAt(0);

    base_view_controller_ = [[UIViewController alloc] init];
    [scoped_key_window_.Get() setRootViewController:base_view_controller_];

    coordinator_ = [[SafeBrowsingCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];
    [coordinator_ start];
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    navigation_manager_ = nullptr;
    web_state_ = nullptr;
    browser_.reset();
    client_.reset();
    PlatformTest::TearDown();
  }

  // Triggers a main-frame suspicious site warning for `url`, completes Safe
  // Browsing checks, and commits the navigation on `web_state_`.
  void TriggerAndCommitSuspiciousNavigation(const GURL& url) {
    web_state_->WasShown();

    web::FakeNavigationContext start_context;
    start_context.SetUrl(url);
    start_context.SetIsSameDocument(false);
    web_state_->OnNavigationStarted(&start_context);

    const web::WebStatePolicyDecider::RequestInfo request_info(
        ui::PageTransition::PAGE_TRANSITION_TYPED,
        /*target_frame_is_main=*/true,
        /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/true, /*user_tapped_recently=*/true);
    base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
        request_decision;
    web_state_->ShouldAllowRequest(
        [NSURLRequest requestWithURL:net::NSURLWithGURL(url)], request_info,
        request_decision.GetCallback());
    ASSERT_TRUE(request_decision.Wait());

    security_interstitials::UnsafeResource resource;
    resource.url = url;
    resource.threat_type =
        safe_browsing::SBThreatType::SB_THREAT_TYPE_WARNABLE_SUSPICIOUS_SITE;
    resource.weak_web_state = web_state_->GetWeakPtr();
    SuspiciousSiteTabHelper::FromWebState(web_state_)
        ->OnSuspiciousSiteWarningTriggered(resource);

    client_->run_sync_callbacks();
    client_->run_async_callbacks();

    NSURLResponse* response =
        [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(url)
                                  MIMEType:@"text/html"
                     expectedContentLength:0
                          textEncodingName:nil];
    web::WebStatePolicyDecider::ResponseInfo response_info(
        /*for_main_frame=*/true);
    base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
        response_decision;
    web_state_->ShouldAllowResponse(response, response_info,
                                    response_decision.GetCallback());
    ASSERT_TRUE(response_decision.Wait());

    navigation_manager_->AddItem(url, ui::PAGE_TRANSITION_TYPED);
    navigation_manager_->SetLastCommittedItemIndex(
        navigation_manager_->GetItemCount() - 1);
    web_state_->SetCurrentURL(url);

    web::FakeNavigationContext finish_context;
    finish_context.SetUrl(url);
    finish_context.SetHasCommitted(true);
    finish_context.SetIsSameDocument(false);
    web_state_->OnNavigationFinished(&finish_context);
  }

  // Waits until `base_view_controller_` finishes presenting its modal.
  bool WaitForWarningPresentationFinished() {
    return base::test::RunUntil(^bool {
      UIViewController* presented =
          base_view_controller_.presentedViewController;
      return presented != nil && !presented.isBeingPresented;
    });
  }

  // Waits until `base_view_controller_` has no presented view controller.
  bool WaitForWarningDismissed() {
    return base::test::RunUntil(^bool {
      return base_view_controller_.presentedViewController == nil;
    });
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<web::FakeWebState> web_state_ = nullptr;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
  std::unique_ptr<FakeSafeBrowsingClient> client_;
  ScopedKeyWindow scoped_key_window_;
  UIViewController* base_view_controller_ = nil;
  SafeBrowsingCoordinator* coordinator_ = nil;
  id mock_settings_handler_ = nil;
};

TEST_F(SafeBrowsingCoordinatorTest,
       SuppressInfobarWhenHandlingSyncNotification) {
  // Navigation is needed for InfoBarManager to have an active entry.
  PerformFakeNavigation(GURL("https://google.com"), web_state_);
  web_state_->WasShown();

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);

  MockTailoredSecurityService* service =
      static_cast<MockTailoredSecurityService*>(
          TailoredSecurityServiceFactory::GetForProfile(profile_.get()));

  // Simulate handling sync notification.
  {
    TailoredSecurityService::ScopedSyncNotificationGuard guard(*service);
    // Trigger a preference change that would normally show an infobar.
    profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
    EXPECT_EQ(infobar_manager->infobars().size(), 0u);
  }

  // After sync is done, changes should trigger infobar.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  EXPECT_EQ(infobar_manager->infobars().size(), 1u);
}

TEST_F(SafeBrowsingCoordinatorTest,
       ShowInfobarWhenNotHandlingSyncNotification) {
  // Navigation is needed for InfoBarManager to have an active entry.
  PerformFakeNavigation(GURL("https://google.com"), web_state_);
  web_state_->WasShown();

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);

  // Trigger a preference change without handling sync notification.
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_EQ(infobar_manager->infobars().size(), 1u);
}

TEST_F(SafeBrowsingCoordinatorTest,
       ShowOnlyOneInfobarWhenEnabledViaTailoredSecurity) {
  // Navigation is needed for InfoBarManager to have an active entry.
  PerformFakeNavigation(GURL("https://google.com"), web_state_);
  web_state_->WasShown();

  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state_);
  EXPECT_EQ(infobar_manager->infobars().size(), 0u);

  MockTailoredSecurityService* service =
      static_cast<MockTailoredSecurityService*>(
          TailoredSecurityServiceFactory::GetForProfile(profile_.get()));

  TailoredSecurityTabHelper::CreateForWebState(web_state_, service);
  TailoredSecurityTabHelper* tab_helper =
      TailoredSecurityTabHelper::FromWebState(web_state_);

  // Trigger the Tailored Security sync notification.
  // This will internaly set the preference AND show its own infobar.
  tab_helper->OnSyncNotificationMessageRequest(/*is_enabled=*/true);

  // Verify that only one infobar is shown (the one from
  // TailoredSecurityTabHelper). SafeBrowsingCoordinator should have suppressed
  // its own.
  ASSERT_EQ(infobar_manager->infobars().size(), 1u);
  InfoBarIOS* infobar =
      static_cast<InfoBarIOS*>(infobar_manager->infobars()[0]);
  EXPECT_EQ(infobar->infobar_type(),
            InfobarType::kInfobarTypeTailoredSecurityService);
}

// Tests that `SafeBrowsingCoordinator` presents a
// `SuspiciousSiteWarningViewController` when triggered by
// `SuspiciousSiteTabHelper`, and dismisses it cleanly even when hidden during
// the presentation animation without recording `kDismissedBySystem`.
TEST_F(SafeBrowsingCoordinatorTest,
       ShowsAndDismissesSuspiciousSiteWarningAlert) {
  base::HistogramTester histogram_tester;
  TriggerAndCommitSuspiciousNavigation(GURL(kSuspiciousUrl));

  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[SuspiciousSiteWarningViewController class]]);

  // Hiding the tab immediately (while presentation is in flight) should still
  // dismiss the alert cleanly once the transition completes without recording
  // `kDismissedBySystem`.
  web_state_->WasHidden();
  EXPECT_TRUE(WaitForWarningDismissed());
  histogram_tester.ExpectTotalCount(kWarningOutcomeHistogram, 0);
}

// Tests that tapping the "Back to safety" button on the suspicious site warning
// alert navigates away, dismisses the alert, and records the expected metrics.
TEST_F(SafeBrowsingCoordinatorTest,
       SuspiciousSiteWarningBackToSafetyActionNavigatesBack) {
  base::HistogramTester histogram_tester;
  TriggerAndCommitSuspiciousNavigation(GURL(kSuspiciousUrl));

  ASSERT_TRUE(WaitForWarningPresentationFinished());
  SuspiciousSiteWarningViewController* warning_vc =
      base::apple::ObjCCastStrict<SuspiciousSiteWarningViewController>(
          base_view_controller_.presentedViewController);
  NSArray<UIButton*>* buttons =
      chrome_test_util::FindViewsByClass<UIButton>(warning_vc.view);
  ASSERT_EQ(2u, buttons.count);
  [buttons[0] sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_TRUE(WaitForWarningDismissed());
  EXPECT_TRUE(navigation_manager_->LoadURLWithParamsWasCalled());
  EXPECT_EQ(GURL(kChromeUINewTabURL),
            navigation_manager_->GetLastLoadURLWithParams()->url);
  histogram_tester.ExpectBucketCount(
      kUserInteractionHistogram,
      SuspiciousSiteWarningUserInteraction::kBackToSafetyButton, 1);
  histogram_tester.ExpectUniqueSample(
      kWarningOutcomeHistogram, SuspiciousSiteWarningOutcome::kAdhered, 1);
}

// Tests that tapping the "Mark as safe" button on the suspicious site warning
// alert dismisses the alert, allowlists the host, and records the expected
// metrics.
TEST_F(SafeBrowsingCoordinatorTest,
       SuspiciousSiteWarningMarkAsSafeActionAllowlistsHost) {
  base::HistogramTester histogram_tester;
  TriggerAndCommitSuspiciousNavigation(GURL(kSuspiciousUrl));

  ASSERT_TRUE(WaitForWarningPresentationFinished());
  SuspiciousSiteWarningViewController* warning_vc =
      base::apple::ObjCCastStrict<SuspiciousSiteWarningViewController>(
          base_view_controller_.presentedViewController);
  NSArray<UIButton*>* buttons =
      chrome_test_util::FindViewsByClass<UIButton>(warning_vc.view);
  ASSERT_EQ(2u, buttons.count);
  [buttons[1] sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_TRUE(WaitForWarningDismissed());
  SuspiciousSiteWarningAllowlist allowlist(
      ios::HostContentSettingsMapFactory::GetForProfile(profile_.get()));
  EXPECT_TRUE(allowlist.IsSiteAllowedForHost(std::string(kSuspiciousHost)));
  histogram_tester.ExpectBucketCount(
      kUserInteractionHistogram,
      SuspiciousSiteWarningUserInteraction::kMarkAsSafe, 1);
  histogram_tester.ExpectUniqueSample(
      kWarningOutcomeHistogram, SuspiciousSiteWarningOutcome::kBypassed, 1);
}

// Tests that stopping `SafeBrowsingCoordinator` while the suspicious site
// warning alert is visible dismisses it and records `kDismissedBySystem`.
TEST_F(SafeBrowsingCoordinatorTest,
       StoppingCoordinatorWhileAlertVisibleLogsDismissedBySystem) {
  base::HistogramTester histogram_tester;
  TriggerAndCommitSuspiciousNavigation(GURL(kSuspiciousUrl));

  ASSERT_TRUE(WaitForWarningPresentationFinished());
  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[SuspiciousSiteWarningViewController class]]);

  [coordinator_ stop];

  EXPECT_TRUE(WaitForWarningDismissed());
  histogram_tester.ExpectUniqueSample(
      kUserInteractionHistogram, SuspiciousSiteWarningUserInteraction::kShown,
      1);
  histogram_tester.ExpectUniqueSample(
      kWarningOutcomeHistogram,
      SuspiciousSiteWarningOutcome::kDismissedBySystem, 1);
}

// Tests that closing the active tab while the suspicious site warning alert is
// visible dismisses the alert and records `kCloseTab` and `kAdhered`.
TEST_F(SafeBrowsingCoordinatorTest,
       ClosingActiveTabWhileAlertVisibleDismissesAlertAndLogsCloseTab) {
  base::HistogramTester histogram_tester;
  TriggerAndCommitSuspiciousNavigation(GURL(kSuspiciousUrl));

  ASSERT_TRUE(WaitForWarningPresentationFinished());
  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[SuspiciousSiteWarningViewController class]]);

  navigation_manager_ = nullptr;
  web_state_ = nullptr;
  browser_->GetWebStateList()->CloseWebStateAt(
      0, WebStateList::ClosingReason::kDefault);

  EXPECT_TRUE(WaitForWarningDismissed());
  histogram_tester.ExpectBucketCount(
      kUserInteractionHistogram,
      SuspiciousSiteWarningUserInteraction::kCloseTab, 1);
  histogram_tester.ExpectUniqueSample(
      kWarningOutcomeHistogram, SuspiciousSiteWarningOutcome::kAdhered, 1);
}

// Tests that external dismissal of the warning view controller records
// `kDismissedBySystem`.
TEST_F(SafeBrowsingCoordinatorTest,
       ExternalModalDismissalLogsDismissedBySystem) {
  base::HistogramTester histogram_tester;
  TriggerAndCommitSuspiciousNavigation(GURL(kSuspiciousUrl));

  ASSERT_TRUE(WaitForWarningPresentationFinished());
  ASSERT_TRUE([base_view_controller_.presentedViewController
      isKindOfClass:[SuspiciousSiteWarningViewController class]]);

  [base_view_controller_ dismissViewControllerAnimated:NO completion:nil];
  EXPECT_TRUE(WaitForWarningDismissed());

  histogram_tester.ExpectUniqueSample(
      kUserInteractionHistogram, SuspiciousSiteWarningUserInteraction::kShown,
      1);
  histogram_tester.ExpectUniqueSample(
      kWarningOutcomeHistogram,
      SuspiciousSiteWarningOutcome::kDismissedBySystem, 1);
}

// Tests that the warning alert is not presented and `kShown` is not recorded
// when `base_view_controller_` is already presenting another modal.
TEST_F(SafeBrowsingCoordinatorTest,
       DoesNotPresentWarningWhenAnotherModalIsPresented) {
  UIViewController* existing_modal = [[UIViewController alloc] init];
  [base_view_controller_ presentViewController:existing_modal
                                      animated:NO
                                    completion:nil];
  ASSERT_EQ(existing_modal, base_view_controller_.presentedViewController);

  base::HistogramTester histogram_tester;
  TriggerAndCommitSuspiciousNavigation(GURL(kSuspiciousUrl));

  EXPECT_EQ(existing_modal, base_view_controller_.presentedViewController);
  histogram_tester.ExpectTotalCount(kUserInteractionHistogram, 0);
  histogram_tester.ExpectTotalCount(kWarningOutcomeHistogram, 0);

  [base_view_controller_ dismissViewControllerAnimated:NO completion:nil];
}

}  // namespace safe_browsing
