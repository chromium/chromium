// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"

#import <memory>

#import "base/command_line.h"
#import "base/files/scoped_temp_dir.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/default_clock.h"
#import "components/policy/policy_constants.h"
#import "components/reading_list/core/reading_list_entry.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper_browser_presentation_provider.h"
#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper_delegate.h"
#import "ios/chrome/browser/app_launcher/model/fake_app_launcher_abuse_detector.h"
#import "ios/chrome/browser/policy/model/enterprise_policy_test_helper.h"
#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_service.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {
// An fake AppLauncherTabHelperDelegate for tests.
class FakeAppLauncherTabHelperDelegate : public AppLauncherTabHelperDelegate {
 public:
  GURL GetLastLaunchedAppUrl() { return last_launched_app_url_; }
  size_t GetAppLaunchCount() { return app_launch_count_; }
  size_t GetAlertShownCount() { return alert_shown_count_; }
  void SetShouldAcceptPrompt(bool should_accept_prompt) {
    should_accept_prompt_ = should_accept_prompt;
  }
  void SetAppLaunchShouldFail(bool app_launch_should_fail) {
    app_launch_should_fail_ = app_launch_should_fail;
  }
  bool ShouldAcceptPrompt() { return should_accept_prompt_; }
  void SetShouldCompleteAppLaunchImmediately(
      bool should_complete_app_launch_immediately) {
    should_complete_app_launch_immediately_ =
        should_complete_app_launch_immediately;
  }
  bool ShouldCompleteAppLaunchImmediately() {
    return should_complete_app_launch_immediately_;
  }
  void CompleteAppLaunch() {
    if (app_launch_completion_) {
      std::move(app_launch_completion_).Run();
    }
  }
  void CompleteBackToApp() {
    if (back_to_app_completion_) {
      std::move(back_to_app_completion_).Run();
    }
  }
  bool IsAppLaunchCompletionPending() const { return !!app_launch_completion_; }

  // AppLauncherTabHelperDelegate:
  void LaunchAppForTabHelper(
      AppLauncherTabHelper* tab_helper,
      const GURL& url,
      base::OnceCallback<void(bool)> completion,
      base::OnceClosure back_to_app_completion) override {
    const GURL app_url = url;
    void (^block_completion)(bool) =
        base::CallbackToBlock(std::move(completion));
    app_launch_completion_ = base::BindOnce(^{
      last_launched_app_url_ = app_url;
      if (!app_launch_should_fail_) {
        ++app_launch_count_;
      }
      block_completion(!app_launch_should_fail_);
    });

    back_to_app_completion_ = std::move(back_to_app_completion);

    if (should_complete_app_launch_immediately_) {
      CompleteAppLaunch();
      CompleteBackToApp();
    }
  }
  void ShowAppLaunchAlert(AppLauncherTabHelper* tab_helper,
                          AppLauncherAlertCause cause,
                          base::OnceCallback<void(bool)> completion) override {
    ++alert_shown_count_;
    std::move(completion).Run(should_accept_prompt_);
  }

 private:
  // URL of the last launched application.
  GURL last_launched_app_url_;
  // Number of times an app was launched.
  size_t app_launch_count_ = 0;
  // Number of times the repeated launches alert has been shown.
  size_t alert_shown_count_ = 0;
  // Simulates the user tapping the accept button when prompted via
  // `-appLauncherTabHelper:showAlertOfRepeatedLaunchesWithCompletionHandler`.
  bool should_accept_prompt_ = false;
  // Simulates the app launch failed.
  bool app_launch_should_fail_ = false;
  // Completion to be called once the app has been launched.
  base::OnceClosure app_launch_completion_;
  // Completion to be called after the scene is activated again.
  base::OnceClosure back_to_app_completion_;
  // Whether to call `complete_app_launch()` immediately at the end of
  // `LaunchAppForTabHelper()`.
  bool should_complete_app_launch_immediately_ = true;
};
// A fake NavigationManager to be used by the WebState object for the
// AppLauncher.
class FakeNavigationManager : public web::FakeNavigationManager {
 public:
  FakeNavigationManager() = default;

  FakeNavigationManager(const FakeNavigationManager&) = delete;
  FakeNavigationManager& operator=(const FakeNavigationManager&) = delete;

  // web::NavigationManager implementation.
  void DiscardNonCommittedItems() override {}
};

}  // namespace

// A fake AppLauncherTabHelperBrowserPresentationProvider to be used by the
// AppLaunchTabHelper with customizable presentingUI property.
@interface FakeAppLauncherTabHelperBrowserPresentationProvider
    : NSObject <AppLauncherTabHelperBrowserPresentationProvider>

@property(nonatomic, assign) BOOL presentingUI;

@end

@implementation FakeAppLauncherTabHelperBrowserPresentationProvider

- (BOOL)isBrowserPresentingUI {
  return _presentingUI;
}

@end

// Test fixture for AppLauncherTabHelper class.
class AppLauncherTabHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ReadingListModelFactory::GetInstance(),
        base::BindRepeating(&BuildReadingListModelWithFakeStorage,
                            std::vector<scoped_refptr<ReadingListEntry>>()));
    profile_ = std::move(builder).Build();
    abuse_detector_ = [[FakeAppLauncherAbuseDetector alloc] init];
    AppLauncherTabHelper::CreateForWebState(&web_state_, abuse_detector_,
                                            /*incognito*/ incognito_);
    // Allow is the default policy for this test.
    abuse_detector_.policy = ExternalAppLaunchPolicyAllow;
    auto navigation_manager = std::make_unique<FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    web_state_.SetCurrentURL(GURL("https://chromium.org"));
    web_state_.SetBrowserState(profile_.get());
    web_state_.WasShown();
    browser_presentation_provider_ =
        [[FakeAppLauncherTabHelperBrowserPresentationProvider alloc] init];
    tab_helper_ = AppLauncherTabHelper::FromWebState(&web_state_);
    tab_helper_->SetDelegate(&delegate_);
    tab_helper_->SetBrowserPresentationProvider(browser_presentation_provider_);
  }

  [[nodiscard]] bool TestShouldAllowRequest(
      NSString* url_string,
      bool target_frame_is_main,
      bool target_frame_is_cross_origin,
      bool target_window_is_cross_origin,
      bool is_user_initiated,
      bool user_tapped_recently,
      ui::PageTransition transition_type =
          ui::PageTransition::PAGE_TRANSITION_LINK) {
    NSURL* url = [NSURL URLWithString:url_string];
    const web::WebStatePolicyDecider::RequestInfo request_info(
        transition_type, target_frame_is_main, target_frame_is_cross_origin,
        target_window_is_cross_origin, is_user_initiated, user_tapped_recently);
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    tab_helper_->ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                    request_info, std::move(callback));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called);
    return policy_decision.ShouldAllowNavigation();
  }

  // Returns true if the `expected_read_status` matches the read status for any
  // non empty source URL based on the transition type and the app policy.
  bool TestReadingListUpdate(bool is_app_blocked,
                             bool is_link_transition,
                             bool expected_read_status) {
    web_state_.SetCurrentURL(GURL("https://chromium.org"));
    GURL pending_url("http://google.com");
    navigation_manager_->AddItem(pending_url, ui::PAGE_TRANSITION_LINK);
    web::NavigationItem* item = navigation_manager_->GetItemAtIndex(0);
    navigation_manager_->SetPendingItem(item);
    item->SetOriginalRequestURL(pending_url);

    ReadingListModel* model =
        ReadingListModelFactory::GetForProfile(profile_.get());
    EXPECT_TRUE(model->DeleteAllEntries(FROM_HERE));
    model->AddOrReplaceEntry(pending_url, "unread",
                             reading_list::ADDED_VIA_CURRENT_APP,
                             /*estimated_read_time=*/base::TimeDelta());
    abuse_detector_.policy = is_app_blocked ? ExternalAppLaunchPolicyBlock
                                            : ExternalAppLaunchPolicyAllow;
    ui::PageTransition transition_type =
        is_link_transition ? ui::PageTransition::PAGE_TRANSITION_LINK
                           : ui::PageTransition::PAGE_TRANSITION_TYPED;

    NSURL* url = [NSURL URLWithString:@"valid://1234"];
    const web::WebStatePolicyDecider::RequestInfo request_info(
        transition_type,
        /*target_frame_is_main=*/true, /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/true, /*user_tapped_recently=*/true);
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    tab_helper_->ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                    request_info, std::move(callback));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called);
    EXPECT_TRUE(policy_decision.ShouldCancelNavigation());

    scoped_refptr<const ReadingListEntry> entry =
        model->GetEntryByURL(pending_url);
    return entry->IsRead() == expected_read_status;
  }

  base::test::TaskEnvironment task_environment;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  bool incognito_ = false;
  raw_ptr<FakeNavigationManager> navigation_manager_ = nullptr;
  FakeAppLauncherAbuseDetector* abuse_detector_ = nil;
  FakeAppLauncherTabHelperDelegate delegate_;
  FakeAppLauncherTabHelperBrowserPresentationProvider*
      browser_presentation_provider_;
  raw_ptr<AppLauncherTabHelper> tab_helper_ = nullptr;
};

// Tests that a valid URL launches app.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_AbuseDetectorPolicyAllowedForValidUrl \
  AbuseDetectorPolicyAllowedForValidUrl
#else
#define MAYBE_AbuseDetectorPolicyAllowedForValidUrl \
  DISABLED_AbuseDetectorPolicyAllowedForValidUrl
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_AbuseDetectorPolicyAllowedForValidUrl) {
  abuse_detector_.policy = ExternalAppLaunchPolicyAllow;
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
  EXPECT_EQ(GURL("valid://1234"), delegate_.GetLastLaunchedAppUrl());
}

// Tests that AppLauncherTabHelper waits for any pending app launch completion
// and scene activation before calling policy decision callbacks for
// subsequent navigation requests.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ShouldAllowRequestWhileAppLaunchPending \
  ShouldAllowRequestWhileAppLaunchPending
#else
#define MAYBE_ShouldAllowRequestWhileAppLaunchPending \
  DISABLED_ShouldAllowRequestWhileAppLaunchPending
#endif
TEST_F(AppLauncherTabHelperTest,
       MAYBE_ShouldAllowRequestWhileAppLaunchPending) {
  delegate_.SetShouldCompleteAppLaunchImmediately(false);

  // Should not allow navigation request with App-URL.
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  // App launch should not be completed yet.
  EXPECT_TRUE(delegate_.IsAppLaunchCompletionPending());
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  // Start another non-App URL request while app launch is still pending.
  NSURL* url =
      [NSURL URLWithString:@"http://itunes.apple.com/us/app/appname/id123"];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
      /*target_frame_is_main=*/true, /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true);
  __block bool callback_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Cancel();
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
        callback_called = true;
      });
  tab_helper_->ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                  request_info, std::move(callback));
  base::RunLoop().RunUntilIdle();

  // Policy decision should wait for app launch completion.
  EXPECT_FALSE(callback_called);
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  // After app launch completion, policy decision should not be received with
  // navigation allowed before CompleteBackToApp.
  delegate_.CompleteAppLaunch();

  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
  EXPECT_EQ(GURL("valid://1234"), delegate_.GetLastLaunchedAppUrl());

  base::RunLoop().RunUntilIdle();
  // Application should be launched, but navigation should still be pending.
  EXPECT_FALSE(callback_called);

  delegate_.CompleteBackToApp();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());
}

// Tests that AppLauncherTabHelper waits for any pending app launch completion
// but not scene activation before calling policy decision callbacks for
// subsequent navigation requests if kill switch is on.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ShouldAllowRequestWhileAppLaunchPendingKS \
  ShouldAllowRequestWhileAppLaunchPendingKS
#else
#define MAYBE_ShouldAllowRequestWhileAppLaunchPendingKS \
  DISABLED_ShouldAllowRequestWhileAppLaunchPendingKS
#endif
TEST_F(AppLauncherTabHelperTest,
       MAYBE_ShouldAllowRequestWhileAppLaunchPendingKS) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kInactiveNavigationAfterAppLaunchKillSwitch);

  delegate_.SetShouldCompleteAppLaunchImmediately(false);

  // Should not allow navigation request with App-URL.
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  // App launch should not be completed yet.
  EXPECT_TRUE(delegate_.IsAppLaunchCompletionPending());
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  // Start another non-App URL request while app launch is still pending.
  NSURL* url =
      [NSURL URLWithString:@"http://itunes.apple.com/us/app/appname/id123"];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
      /*target_frame_is_main=*/true, /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true);
  __block bool callback_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Cancel();
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
        callback_called = true;
      });
  tab_helper_->ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                  request_info, std::move(callback));
  base::RunLoop().RunUntilIdle();

  // Policy decision should wait for app launch completion.
  EXPECT_FALSE(callback_called);
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  // After app launch completion, policy decision should be received with
  // navigation allowed.
  delegate_.CompleteAppLaunch();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
  EXPECT_EQ(GURL("valid://1234"), delegate_.GetLastLaunchedAppUrl());
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());
}

// Tests that AppLauncherTabHelper waits for any pending app launch completion
// before calling policy decision callbacks for subsequent navigation requests
// when app launching failed.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ShouldAllowRequestWhileFailingAppLaunchPending \
  ShouldAllowRequestWhileFailingAppLaunchPending
#else
#define MAYBE_ShouldAllowRequestWhileFailingAppLaunchPending \
  DISABLED_ShouldAllowRequestWhileFailingAppLaunchPending
#endif
TEST_F(AppLauncherTabHelperTest,
       MAYBE_ShouldAllowRequestWhileFailingAppLaunchPending) {
  delegate_.SetShouldCompleteAppLaunchImmediately(false);
  delegate_.SetAppLaunchShouldFail(true);

  // Should not allow navigation request with App-URL.
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  // App launch should not be completed yet.
  EXPECT_TRUE(delegate_.IsAppLaunchCompletionPending());
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  // Start another non-App URL request while app launch is still pending.
  NSURL* url =
      [NSURL URLWithString:@"http://itunes.apple.com/us/app/appname/id123"];
  const web::WebStatePolicyDecider::RequestInfo request_info(
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
      /*target_frame_is_main=*/true, /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true);
  __block bool callback_called = false;
  __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Cancel();
  auto callback =
      base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
        callback_called = true;
      });
  tab_helper_->ShouldAllowRequest([NSURLRequest requestWithURL:url],
                                  request_info, std::move(callback));
  base::RunLoop().RunUntilIdle();

  // Policy decision should wait for app launch completion.
  EXPECT_FALSE(callback_called);
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
  EXPECT_TRUE(delegate_.IsAppLaunchCompletionPending());

  // After app launch completion, policy decision should be received with
  // navigation allowed.
  delegate_.CompleteAppLaunch();
  base::RunLoop().RunUntilIdle();

  // Application should not be launched, and navigation should trigger be
  // pending.
  EXPECT_FALSE(delegate_.IsAppLaunchCompletionPending());
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());
}

// Tests that a valid URL does not launch app when launch policy is to block.
TEST_F(AppLauncherTabHelperTest, AbuseDetectorPolicyBlockedForValidUrl) {
  abuse_detector_.policy = ExternalAppLaunchPolicyBlock;
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAlertShownCount());
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
}

// Tests that no alert is shown on app launch failure on valid scheme.
TEST_F(AppLauncherTabHelperTest, AppLaunchingFails) {
  delegate_.SetAppLaunchShouldFail(true);
  delegate_.SetShouldAcceptPrompt(true);

  // Should not allow navigation request with App-URL.
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  // App launch should not be completed yet.
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
  EXPECT_EQ(0U, delegate_.GetAlertShownCount());
}

// Tests that an extra alert is shown on app launch failure without user
// gesture.
// TODO(crbug.com/40287450): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_AppLaunchingFailsWithoutUserGesture \
  AppLaunchingFailsWithoutUserGesture
#else
#define MAYBE_AppLaunchingFailsWithoutUserGesture \
  DISABLED_AppLaunchingFailsWithoutUserGesture
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_AppLaunchingFailsWithoutUserGesture) {
  delegate_.SetAppLaunchShouldFail(true);
  delegate_.SetShouldAcceptPrompt(true);

  // Should not allow navigation request with App-URL.
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/false,
                                      /*user_tapped_recently=*/false));
  // App launch should not be completed yet.
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
  // Two alerts (no user gesture + app launch failed).
  EXPECT_EQ(2U, delegate_.GetAlertShownCount());
  // Should not allow navigation request with App-URL.
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/false,
                                      /*user_tapped_recently=*/true));
  // App launch should not be completed yet.
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
  // Two more alerts (no user gesture + app launch failed).
  EXPECT_EQ(4U, delegate_.GetAlertShownCount());
}

// Tests that a valid URL shows an alert and launches app when launch policy is
// to prompt and user accepts.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ValidUrlPromptUserAccepts ValidUrlPromptUserAccepts
#else
#define MAYBE_ValidUrlPromptUserAccepts DISABLED_ValidUrlPromptUserAccepts
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_ValidUrlPromptUserAccepts) {
  abuse_detector_.policy = ExternalAppLaunchPolicyPrompt;
  delegate_.SetShouldAcceptPrompt(true);
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));

  EXPECT_EQ(1U, delegate_.GetAlertShownCount());
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
  EXPECT_EQ(GURL("valid://1234"), delegate_.GetLastLaunchedAppUrl());
}

// Tests that a valid URL does not launch app when launch policy is to prompt
// and user rejects.
TEST_F(AppLauncherTabHelperTest, ValidUrlPromptUserRejects) {
  abuse_detector_.policy = ExternalAppLaunchPolicyPrompt;
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
}

// Tests that a valid URL triggers a prompt if transition is not link.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ValidUrlNotLinkTransition ValidUrlNotLinkTransition
#else
#define MAYBE_ValidUrlNotLinkTransition DISABLED_ValidUrlNotLinkTransition
#endif
// TODO(crbug.com/40287450): The test fails on device.
TEST_F(AppLauncherTabHelperTest, MAYBE_ValidUrlNotLinkTransition) {
  delegate_.SetShouldAcceptPrompt(true);
  EXPECT_FALSE(TestShouldAllowRequest(
      @"valid://1234",
      /*target_frame_is_main=*/true,
      /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true,
      /*transition_type=*/ui::PageTransition::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(1U, delegate_.GetAlertShownCount());
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
}

// Tests that iTunes Urls are blocked with a prompt.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_iTunesURL iTunesURL
#else
#define MAYBE_iTunesURL DISABLED_iTunesURL
#endif
// TODO(crbug.com/40287450): The test fails on device.
TEST_F(AppLauncherTabHelperTest, MAYBE_iTunesURL) {
  NSString* url_string = @"itms-apps://itunes.apple.com/us/app/appname/id123";
  delegate_.SetShouldAcceptPrompt(true);
  EXPECT_FALSE(TestShouldAllowRequest(/*url_string=*/url_string,
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(1U, delegate_.GetAlertShownCount());
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
}

// Tests that ShouldAllowRequest only launches apps for App Urls in main frame,
// or iframe when there was a recent user interaction.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ShouldAllowRequestWithAppUrl ShouldAllowRequestWithAppUrl
#else
#define MAYBE_ShouldAllowRequestWithAppUrl DISABLED_ShouldAllowRequestWithAppUrl
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_ShouldAllowRequestWithAppUrl) {
  NSString* url_string = @"valid://1234";
  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/false,
                                      /*user_tapped_recently=*/false));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());

  EXPECT_FALSE(TestShouldAllowRequest(url_string,
                                      /*target_frame_is_main=*/false,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/false,
                                      /*user_tapped_recently=*/false));
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());

  EXPECT_FALSE(TestShouldAllowRequest(url_string,
                                      /*target_frame_is_main=*/false,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(2U, delegate_.GetAppLaunchCount());
}

// Tests that ShouldAllowRequest always allows requests and does not launch
// apps for non App Urls.
TEST_F(AppLauncherTabHelperTest, ShouldAllowRequestWithNonAppUrl) {
  EXPECT_TRUE(TestShouldAllowRequest(
      @"http://itunes.apple.com/us/app/appname/id123",
      /*target_frame_is_main=*/true, /*target_frame_is_cross_origin=*/false,
      /*target_window_is_cross_origin=*/false,
      /*is_user_initiated=*/true, /*user_tapped_recently=*/true));
  EXPECT_TRUE(TestShouldAllowRequest(@"file://a/b/c",
                                     /*target_frame_is_main=*/true,
                                     /*target_frame_is_cross_origin=*/false,
                                     /*target_window_is_cross_origin=*/false,
                                     /*is_user_initiated=*/true,
                                     /*user_tapped_recently=*/true));
  EXPECT_TRUE(TestShouldAllowRequest(@"about://test",
                                     /*target_frame_is_main=*/false,
                                     /*target_frame_is_cross_origin=*/false,
                                     /*target_window_is_cross_origin=*/false,
                                     /*is_user_initiated=*/true,
                                     /*user_tapped_recently=*/true));
  EXPECT_TRUE(TestShouldAllowRequest(@"data://test",
                                     /*target_frame_is_main=*/false,
                                     /*target_frame_is_cross_origin=*/false,
                                     /*target_window_is_cross_origin=*/false,
                                     /*is_user_initiated=*/true,
                                     /*user_tapped_recently=*/true));
  EXPECT_TRUE(TestShouldAllowRequest(@"blob://test",
                                     /*target_frame_is_main=*/false,
                                     /*target_frame_is_cross_origin=*/false,
                                     /*target_window_is_cross_origin=*/false,
                                     /*is_user_initiated=*/true,
                                     /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
}

// Tests that invalid Urls are completely blocked.
TEST_F(AppLauncherTabHelperTest, InvalidUrls) {
  EXPECT_FALSE(TestShouldAllowRequest(/*url_string=*/@"",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_FALSE(TestShouldAllowRequest(@"invalid",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
}

// Tests that if web_state is not shown or if there is a UI on top of it, no
// request is triggered.
// TODO(crbug.com/40287450): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_WebStateNotShown WebStateNotShown
#else
#define MAYBE_WebStateNotShown DISABLED_WebStateNotShown
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_WebStateNotShown) {
  // Base case.
  NSString* url_string = @"valid://1234";
  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());

  // WebState hidden.
  web_state_.WasHidden();
  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());

  // Browser not visible.
  web_state_.WasShown();
  browser_presentation_provider_.presentingUI = YES;
  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());

  // Base case to check.
  browser_presentation_provider_.presentingUI = NO;
  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(2U, delegate_.GetAppLaunchCount());
}

// Tests that when the last committed URL is invalid, the URL is only opened
// when the last committed item is nil.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ValidUrlInvalidCommittedURL ValidUrlInvalidCommittedURL
#else
#define MAYBE_ValidUrlInvalidCommittedURL DISABLED_ValidUrlInvalidCommittedURL
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_ValidUrlInvalidCommittedURL) {
  NSString* url_string = @"valid://1234";
  web_state_.SetCurrentURL(GURL());

  std::unique_ptr<web::NavigationItem> item = web::NavigationItem::Create();
  item->SetURL(GURL());

  navigation_manager_->SetLastCommittedItem(item.get());
  EXPECT_FALSE(TestShouldAllowRequest(url_string,
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  navigation_manager_->SetLastCommittedItem(nullptr);
  EXPECT_FALSE(TestShouldAllowRequest(url_string,
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
}

// Tests that URLs with schemes that might be a security risk are blocked.
TEST_F(AppLauncherTabHelperTest, InsecureUrls) {
  EXPECT_FALSE(TestShouldAllowRequest(@"app-settings://",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
}

// Tests that tel: URLs are blocked when the target frame is cross-origin
// with respect to the source origin.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_TelUrls TelUrls
#else
#define MAYBE_TelUrls DISABLED_TelUrls
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_TelUrls) {
  EXPECT_FALSE(TestShouldAllowRequest(@"tel:+12345551212",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/true,
                                      /*target_window_is_cross_origin=*/true,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  EXPECT_FALSE(TestShouldAllowRequest(@"tel:+12345551212",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/true,
                                      /*target_window_is_cross_origin=*/true,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  EXPECT_FALSE(TestShouldAllowRequest(@"tel:+12345551212",
                                      /*target_frame_is_main=*/false,
                                      /*target_frame_is_cross_origin=*/true,
                                      /*target_window_is_cross_origin=*/true,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  EXPECT_FALSE(TestShouldAllowRequest(@"tel:+12345551212",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());

  EXPECT_FALSE(TestShouldAllowRequest(@"tel:+12345551212",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/false,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(2U, delegate_.GetAppLaunchCount());
}

// Tests that URLs with Chrome Bundle schemes are blocked on main frames and
// iframes.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ChromeBundleUrlScheme ChromeBundleUrlScheme
#else
#define MAYBE_ChromeBundleUrlScheme DISABLED_ChromeBundleUrlScheme
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_ChromeBundleUrlScheme) {
  // Get the test bundle URL Scheme.
  NSString* scheme = [[ChromeAppConstants sharedInstance] bundleURLScheme];
  NSString* url = [NSString stringWithFormat:@"%@://www.google.com", scheme];

  // Verify that the URL is blocked on iframes.
  EXPECT_FALSE(TestShouldAllowRequest(url,
                                      /*target_frame_is_main=*/false,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  EXPECT_FALSE(TestShouldAllowRequest(url,
                                      /*target_frame_is_main=*/false,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  // Verify that the URL is blocked on main frames.
  EXPECT_FALSE(TestShouldAllowRequest(url,
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
}

// Tests that ShouldAllowRequest updates the reading list correctly for non-link
// transitions regardless of the app launching success when AppLauncherRefresh
// flag is enabled.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_UpdatingTheReadingList UpdatingTheReadingList
#else
#define MAYBE_UpdatingTheReadingList DISABLED_UpdatingTheReadingList
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_UpdatingTheReadingList) {
  delegate_.SetShouldAcceptPrompt(false);
  // Update reading list if the transition is not a link transition.
  EXPECT_TRUE(TestReadingListUpdate(/*is_app_blocked=*/true,
                                    /*is_link_transition*/ false,
                                    /*expected_read_status*/ true));
  EXPECT_EQ(1U, delegate_.GetAlertShownCount());
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  EXPECT_TRUE(TestReadingListUpdate(/*is_app_blocked=*/false,
                                    /*is_link_transition*/ false,
                                    /*expected_read_status*/ true));
  EXPECT_EQ(2U, delegate_.GetAlertShownCount());
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  // Don't update reading list if the transition is a link transition.
  EXPECT_TRUE(TestReadingListUpdate(/*is_app_blocked=*/true,
                                    /*is_link_transition*/ true,
                                    /*expected_read_status*/ false));
  EXPECT_EQ(2U, delegate_.GetAlertShownCount());
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());

  EXPECT_TRUE(TestReadingListUpdate(/*is_app_blocked=*/false,
                                    /*is_link_transition*/ true,
                                    /*expected_read_status*/ false));
  EXPECT_EQ(2U, delegate_.GetAlertShownCount());
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
}

// Tests that launching a SMS URL via a JavaScript redirect in the main frame
// is allowed. Covers the scenario for crbug.com/1058388
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_LaunchSmsApp_JavaScriptRedirect LaunchSmsApp_JavaScriptRedirect
#else
#define MAYBE_LaunchSmsApp_JavaScriptRedirect \
  DISABLED_LaunchSmsApp_JavaScriptRedirect
#endif
TEST_F(AppLauncherTabHelperTest, MAYBE_LaunchSmsApp_JavaScriptRedirect) {
  NSString* sms_url_string = @"sms:?&body=Hello%20World";
  ui::PageTransition page_transition = ui::PageTransitionFromInt(
      ui::PageTransition::PAGE_TRANSITION_LINK |
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT);
  EXPECT_FALSE(
      TestShouldAllowRequest(sms_url_string, /*target_frame_is_main=*/true,
                             /*target_frame_is_cross_origin=*/false,
                             /*target_window_is_cross_origin=*/false,
                             /*is_user_initiated=*/true,
                             /*user_tapped_recently=*/true, page_transition));
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
}

// Test fixture for testing the app launcher interaction with enterprise policy
// URLBlocklist.
class BlockedUrlPolicyAppLauncherTabHelperTest
    : public AppLauncherTabHelperTest {
 protected:
  void SetUp() override {
    AppLauncherTabHelperTest::SetUp();

    ASSERT_TRUE(state_directory_.CreateUniqueTempDir());
    enterprise_policy_helper_ = std::make_unique<EnterprisePolicyTestHelper>(
        state_directory_.GetPath());
    ASSERT_TRUE(enterprise_policy_helper_->GetProfile());

    web_state_.SetBrowserState(enterprise_policy_helper_->GetProfile());

    policy::PolicyMap policy_map;
    base::Value::List value;
    value.Append("itms-apps://*");
    policy_map.Set(policy::key::kURLBlocklist, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   base::Value(std::move(value)), nullptr);
    enterprise_policy_helper_->GetPolicyProvider()->UpdateChromePolicy(
        policy_map);

    policy_blocklist_service_ = static_cast<PolicyBlocklistService*>(
        PolicyBlocklistServiceFactory::GetForProfile(
            enterprise_policy_helper_->GetProfile()));
  }

  // Temporary directory to hold preference files.
  base::ScopedTempDir state_directory_;

  // Enterprise policy boilerplate configuration.
  std::unique_ptr<EnterprisePolicyTestHelper> enterprise_policy_helper_;
  raw_ptr<PolicyBlocklistService> policy_blocklist_service_;
};

// Tests that URLs to blocked domains do not open native apps.
TEST_F(BlockedUrlPolicyAppLauncherTabHelperTest, BlockedUrl) {
  NSString* url_string = @"itms-apps://itunes.apple.com/us/app/appname/id123";
  EXPECT_FALSE(TestShouldAllowRequest(url_string, /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
}

// Tests that URLs to non-blocked domains are able to open native apps when
// policy is blocking other domains.
// TODO(crbug.com/40166678): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_AllowedUrl AllowedUrl
#else
#define MAYBE_AllowedUrl DISABLED_AllowedUrl
#endif
TEST_F(BlockedUrlPolicyAppLauncherTabHelperTest, MAYBE_AllowedUrl) {
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
  EXPECT_EQ(GURL("valid://1234"), delegate_.GetLastLaunchedAppUrl());
}

// Test fixture for incognito AppLauncherTabHelper class.
class IncognitoAppLauncherTabHelperTest : public AppLauncherTabHelperTest {
 protected:
  void SetUp() override {
    incognito_ = true;
    AppLauncherTabHelperTest::SetUp();
  }
};

// Tests that opening an external App from incognito tab always triggers a
// prompt.
// TODO(crbug.com/40287450): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_ValidUrlPromptUserAccepts ValidUrlPromptUserAccepts
#else
#define MAYBE_ValidUrlPromptUserAccepts DISABLED_ValidUrlPromptUserAccepts
#endif
TEST_F(IncognitoAppLauncherTabHelperTest, MAYBE_ValidUrlPromptUserAccepts) {
  delegate_.SetShouldAcceptPrompt(true);
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(1U, delegate_.GetAlertShownCount());
  EXPECT_EQ(1U, delegate_.GetAppLaunchCount());
  EXPECT_EQ(GURL("valid://1234"), delegate_.GetLastLaunchedAppUrl());
}

// Tests that a second prompt is triggered when failing to open an external app
// from incognito.
// TODO(crbug.com/40287450): The test fails on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_AppLaunchFails AppLaunchFails
#else
#define MAYBE_AppLaunchFails DISABLED_AppLaunchFails
#endif
TEST_F(IncognitoAppLauncherTabHelperTest, MAYBE_AppLaunchFails) {
  delegate_.SetShouldAcceptPrompt(true);
  delegate_.SetAppLaunchShouldFail(true);
  EXPECT_FALSE(TestShouldAllowRequest(@"valid://1234",
                                      /*target_frame_is_main=*/true,
                                      /*target_frame_is_cross_origin=*/false,
                                      /*target_window_is_cross_origin=*/false,
                                      /*is_user_initiated=*/true,
                                      /*user_tapped_recently=*/true));
  EXPECT_EQ(2U, delegate_.GetAlertShownCount());
  EXPECT_EQ(0U, delegate_.GetAppLaunchCount());
}
