// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_promo_manager.h"

#import "base/test/scoped_feature_list.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/test/mock_tracker.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_browser/model/utils_test_support.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

std::unique_ptr<KeyedService> BuildFeatureEngagementMockTracker(
    web::BrowserState* browser_state) {
  return std::make_unique<feature_engagement::test::MockTracker>();
}

class DefaultBrowserPromoManagerTest : public PlatformTest {
 public:
  DefaultBrowserPromoManagerTest() : PlatformTest() {}

 protected:
  void SetUp() override {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state_.get());
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(AuthenticationServiceFactory::GetDefaultFactory()));
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        base::BindRepeating(&BuildFeatureEngagementMockTracker));
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    view_controller_ = [[UIViewController alloc] init];
    default_browser_promo_manager_ = [[DefaultBrowserPromoManager alloc]
        initWithBaseViewController:view_controller_
                           browser:browser_.get()];
  }

  void TearDown() override {
    [default_browser_promo_manager_ stop];
    browser_state_.reset();
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    TestingApplicationContext::GetGlobal()->SetLastShutdownClean(false);
    local_state_.reset();
    ClearDefaultBrowserPromoData();
    default_browser_promo_manager_ = nil;
  }

  void SignIn() {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  std::unique_ptr<TestChromeBrowserState> browser_state_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<Browser> browser_;
  UIViewController* view_controller_;
  DefaultBrowserPromoManager* default_browser_promo_manager_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the DefaultPromoTypeMadeForIOS tailored promo is shown when it was
// detected that the user is likely interested in the promo.
TEST_F(DefaultBrowserPromoManagerTest, ShowTailoredPromoMadeForIOS) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  id mock = [OCMockObject mockForClass:[DefaultBrowserPromoManager class]];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeMadeForIOS);
  [[mock expect] showPromoForTesting:DefaultPromoTypeMadeForIOS];
  [default_browser_promo_manager_ start];
  EXPECT_OCMOCK_VERIFY(mock);
}

// Tests that the DefaultPromoTypeStaySafe tailored promo is shown when it was
// detected that the user is likely interested in the promo.
TEST_F(DefaultBrowserPromoManagerTest, ShowTailoredPromoStaySafe) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  id mock = [OCMockObject mockForClass:[DefaultBrowserPromoManager class]];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  [[mock expect] showPromoForTesting:DefaultPromoTypeStaySafe];
  [default_browser_promo_manager_ start];
  EXPECT_OCMOCK_VERIFY(mock);
}

// Tests that the DefaultPromoTypeAllTabs tailored promo is shown when it was
// detected that the user is likely interested in the promo.
TEST_F(DefaultBrowserPromoManagerTest, ShowTailoredPromoAllTabs) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  id mock = [OCMockObject mockForClass:[DefaultBrowserPromoManager class]];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  SignIn();
  [[mock expect] showPromoForTesting:DefaultPromoTypeAllTabs];
  [default_browser_promo_manager_ start];
  EXPECT_OCMOCK_VERIFY(mock);
}

// Tests that the DefaultPromoTypeGeneral promo is shown when it was detected
// that the user is likely interested in the promo.
TEST_F(DefaultBrowserPromoManagerTest, showDefaultBrowserFullscreenPromo) {
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  id mock = [OCMockObject mockForClass:[DefaultBrowserPromoManager class]];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  SignIn();
  [[mock expect] showPromoForTesting:DefaultPromoTypeGeneral];
  [default_browser_promo_manager_ start];
  EXPECT_OCMOCK_VERIFY(mock);
}

// Tests that the DefaultPromoTypeVideo promo is shown when it was detected
// that the user is likely interested in the promo.
TEST_F(DefaultBrowserPromoManagerTest, showDefaultBrowserVideoPromo) {
  std::map<std::string, std::string> parameters;
  base::test::ScopedFeatureList feature_list;
  parameters[kDefaultBrowserVideoPromoVariant] =
      kVideoConditionsFullscreenPromo;
  feature_list.InitAndEnableFeatureWithParameters(kDefaultBrowserVideoPromo,
                                                  parameters);
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  feature_engagement::test::MockTracker* mock_tracker =
      static_cast<feature_engagement::test::MockTracker*>(
          feature_engagement::TrackerFactory::GetForBrowserState(
              browser_state_.get()));
  id mock = [OCMockObject mockForClass:[DefaultBrowserPromoManager class]];
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeVideo);
  EXPECT_CALL(
      *mock_tracker,
      WouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserVideoPromoTriggerFeature)))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(
      *mock_tracker,
      ShouldTriggerHelpUI(testing::Ref(
          feature_engagement::kIPHiOSDefaultBrowserVideoPromoTriggerFeature)))
      .WillOnce(testing::Return(true));
  [[mock expect] showPromoForTesting:DefaultPromoTypeVideo];
  [default_browser_promo_manager_ start];
  EXPECT_OCMOCK_VERIFY(mock);
}

// Tests that the DefaultPromoTypeGeneral promo is shown if the trigger criteria
// experiment is enabled.
TEST_F(DefaultBrowserPromoManagerTest,
       showDefaultBrowserFullscreenPromo_TriggerExpEnabled) {
  feature_list_.InitWithFeatures({kDefaultBrowserTriggerCriteriaExperiment},
                                 {});
  TestingApplicationContext::GetGlobal()->SetLastShutdownClean(true);
  id mock = [OCMockObject mockForClass:[DefaultBrowserPromoManager class]];

  // Even if the conditions are right for tailored promo, it should display
  // default browser promo if the trigger criteria experiment is on.
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeAllTabs);
  SignIn();

  [[mock expect] showPromoForTesting:DefaultPromoTypeGeneral];
  [default_browser_promo_manager_ start];
  EXPECT_OCMOCK_VERIFY(mock);
}
}  // namespace
