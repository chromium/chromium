// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/default_browser/coordinator/default_browser_screen_coordinator.h"

#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_android_switcher_screen_view_controller.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_animated_screen_view_controller.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_screen_view_controller.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Test implementation of `FirstRunScreenDelegate`.
@interface DefaultBrowserFirstRunScreenTestDelegate
    : NSObject <FirstRunScreenDelegate>
@end

@implementation DefaultBrowserFirstRunScreenTestDelegate

- (void)firstRunScreenCoordinatorWantsToBeStopped:
    (ChromeCoordinator*)coordinator {
}

@end

namespace {

// Unit tests for `DefaultBrowserScreenCoordinator`.
class DefaultBrowserScreenCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_);
    navigation_controller_ = [[UINavigationController alloc] init];
    delegate_ = [[DefaultBrowserFirstRunScreenTestDelegate alloc] init];

    coordinator_ = [[DefaultBrowserScreenCoordinator alloc]
        initWithBaseNavigationController:navigation_controller_
                                 browser:browser_.get()
                                delegate:delegate_];
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    delegate_ = nil;
    navigation_controller_ = nil;
    browser_.reset();
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  // Signs in with a fake identity.
  void SignIn() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForProfile(profile_)->SignIn(
        identity, signin_metrics::AccessPoint::kStartPage);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  std::unique_ptr<TestBrowser> browser_;
  UINavigationController* navigation_controller_;
  DefaultBrowserFirstRunScreenTestDelegate* delegate_;
  DefaultBrowserScreenCoordinator* coordinator_;
};

// Tests that the standard static Default Browser promo is presented when the
// Android Switcher flag is disabled, even if the user is signed in.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWhenFlagDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          first_run::kAnimatedDefaultBrowserPromoInFRE,
          first_run::kAndroidSwitcherDefaultBrowserPromoInFRE});
  SignIn();

  [coordinator_ start];

  UIViewController* presented_view_controller =
      navigation_controller_.topViewController;
  EXPECT_TRUE([presented_view_controller
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
}

// Tests that the standard static Default Browser promo is presented when the
// Android Switcher flag is enabled but the user is signed out.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWhenSignedOut) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{first_run::
                                kAndroidSwitcherDefaultBrowserPromoInFRE},
      /*disabled_features=*/{first_run::kAnimatedDefaultBrowserPromoInFRE});

  [coordinator_ start];

  UIViewController* presented_view_controller =
      navigation_controller_.topViewController;
  EXPECT_TRUE([presented_view_controller
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
}

// Tests that the Android Switcher Default Browser promo is presented when the
// flag is enabled and the user is signed in.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsAndroidSwitcherPromoWhenFlagEnabledAndSignedIn) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{first_run::kAndroidSwitcherDefaultBrowserPromoInFRE,
        {{first_run::kAndroidSwitcherDefaultBrowserPromoInFREParam,
          first_run::
              kAndroidSwitcherDefaultBrowserPromoInFREParamSegmentationSignal}}}},
      /*disabled_features=*/{first_run::kAnimatedDefaultBrowserPromoInFRE});
  SignIn();

  [coordinator_ start];

  UIViewController* presented_view_controller =
      navigation_controller_.topViewController;
  ASSERT_TRUE([presented_view_controller
      isMemberOfClass:[DefaultBrowserAndroidSwitcherScreenViewController
                          class]]);
  [presented_view_controller loadViewIfNeeded];
  EXPECT_NSEQ(
      presented_view_controller.view.accessibilityIdentifier,
      first_run::
          kFirstRunDefaultBrowserAndroidSwitcherScreenAccessibilityIdentifier);
}

// Tests that the animated Default Browser promo takes precedence when both the
// animated and Android Switcher promo flags are enabled.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsAnimatedPromoWhenBothFlagsEnabled) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{first_run::kAnimatedDefaultBrowserPromoInFRE,
                            first_run::
                                kAndroidSwitcherDefaultBrowserPromoInFRE},
      /*disabled_features=*/{first_run::kUpdatedFirstRunSequence});
  SignIn();

  [coordinator_ start];

  UIViewController* presented_view_controller =
      navigation_controller_.topViewController;
  EXPECT_TRUE([presented_view_controller
      isMemberOfClass:[DefaultBrowserAnimatedScreenViewController class]]);
}

}  // namespace
