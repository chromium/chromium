// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider+protected.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
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
#import "ios/chrome/test/providers/switcher_info/test_switcher_info.h"
#import "ios/public/provider/chrome/browser/switcher_info/switcher_info_api.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

@interface FirstRunCoordinator (Testing) <FirstRunScreenDelegate>
@property(nonatomic, strong, readonly) ChromeCoordinator* childCoordinator;
@end

namespace {

class FirstRunCoordinatorTest : public PlatformTest {
 public:
  FirstRunCoordinatorTest() = default;

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    browser_.reset();
    profile_ = nullptr;
    scene_state_ = nil;
    profile_state_ = nil;
    base_view_controller_ = nil;
    ios::provider::test::ResetSwitcherInfoState();
    PlatformTest::TearDown();
  }

  void InitProfileAndBrowser() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    scene_state_ = [[SceneState alloc] init];
    scene_state_.profileState = profile_state_;
    browser_ = std::make_unique<TestBrowser>(profile_, scene_state_);
    base_view_controller_ = [[UIViewController alloc] init];
  }

  void AddIdentityOnDevice() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity([FakeSystemIdentity fakeIdentity1]);
  }

  void SignInIdentity() {
    if (!profile_) {
      InitProfileAndBrowser();
    }
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager())
        ->AddIdentity(identity);
    AuthenticationServiceFactory::GetForProfile(profile_)->SignIn(
        identity, signin_metrics::AccessPoint::kStartPage);
  }

  void StartCoordinatorWithScreens(NSArray* screens) {
    if (!profile_) {
      InitProfileAndBrowser();
    }
    ScreenProvider* screen_provider =
        [[ScreenProvider alloc] initWithScreens:screens];
    coordinator_ = [[FirstRunCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                    screenProvider:screen_provider];
    [coordinator_ start];
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  ProfileState* profile_state_ = nil;
  SceneState* scene_state_ = nil;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_ = nil;
  FirstRunCoordinator* coordinator_ = nil;
};

// Tests that when the signed-in user completes the FRE and the switcher info
// API returns kSwitcher, kAndroidSwitcher and success latency are recorded on
// UMA.
TEST_F(FirstRunCoordinatorTest,
       RecordsAndroidSwitcherSwitcherInfoResultAndLatencyOnCompletion) {
  feature_list_.InitAndEnableFeature(
      first_run::kQuerySwitcherInfoSignalInFirstRun);
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  base::HistogramTester histogram_tester;

  StartCoordinatorWithScreens(@[ @(kSignIn), @(kStepsCompleted) ]);
  SignInIdentity();
  task_environment_.FastForwardBy(base::Seconds(1));

  histogram_tester.ExpectUniqueSample(
      first_run::kDefaultBrowserPromoSwitcherInfoResultHistogram,
      first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencySuccessHistogram, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencyFailureHistogram, 0);
}

// Tests that when the signed-in user completes the FRE and the switcher info
// API returns kNotSwitcher, kNotAndroidSwitcher and success latency are
// recorded on UMA.
TEST_F(FirstRunCoordinatorTest,
       RecordsNotAndroidSwitcherSwitcherInfoResultAndLatencyOnCompletion) {
  feature_list_.InitAndEnableFeature(
      first_run::kQuerySwitcherInfoSignalInFirstRun);
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kNotSwitcher,
      .is_chrome_user = false});
  base::HistogramTester histogram_tester;

  StartCoordinatorWithScreens(@[ @(kSignIn), @(kStepsCompleted) ]);
  SignInIdentity();
  task_environment_.FastForwardBy(base::Seconds(1));

  histogram_tester.ExpectUniqueSample(
      first_run::kDefaultBrowserPromoSwitcherInfoResultHistogram,
      first_run::DefaultBrowserPromoSegmentationResult::kNotAndroidSwitcher, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencySuccessHistogram, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencyFailureHistogram, 0);
}

// Tests that when the signed-in user completes the FRE and the switcher info
// API returns kUnknown, kNotReady and failure latency are recorded on UMA.
TEST_F(FirstRunCoordinatorTest,
       RecordsUnknownSwitcherInfoResultAndLatencyOnCompletion) {
  feature_list_.InitAndEnableFeature(
      first_run::kQuerySwitcherInfoSignalInFirstRun);
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kUnknown,
      .is_chrome_user = false});
  base::HistogramTester histogram_tester;

  StartCoordinatorWithScreens(@[ @(kSignIn), @(kStepsCompleted) ]);
  SignInIdentity();
  task_environment_.FastForwardBy(base::Seconds(1));

  histogram_tester.ExpectUniqueSample(
      first_run::kDefaultBrowserPromoSwitcherInfoResultHistogram,
      first_run::DefaultBrowserPromoSegmentationResult::kNotReady, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencySuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencyFailureHistogram, 1);
}

// Tests that when the signed-in user completes the FRE and the switcher info
// API fails (std::nullopt), kFailed and failure latency are recorded on UMA.
TEST_F(FirstRunCoordinatorTest,
       RecordsFailedSwitcherInfoResultAndLatencyOnCompletion) {
  feature_list_.InitAndEnableFeature(
      first_run::kQuerySwitcherInfoSignalInFirstRun);
  ios::provider::test::SetSwitcherInfoResult(std::nullopt);
  base::HistogramTester histogram_tester;

  StartCoordinatorWithScreens(@[ @(kSignIn), @(kStepsCompleted) ]);
  SignInIdentity();
  task_environment_.FastForwardBy(base::Seconds(1));

  histogram_tester.ExpectUniqueSample(
      first_run::kDefaultBrowserPromoSwitcherInfoResultHistogram,
      first_run::DefaultBrowserPromoSegmentationResult::kFailed, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencySuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencyFailureHistogram, 1);
}

// Tests that when an iOS device identity exists but the user completes the FRE
// without signing in, the switcher info API is not queried and no metrics are
// recorded.
TEST_F(FirstRunCoordinatorTest,
       DoesNotRecordSwitcherInfoMetricsWhenSignInSkipped) {
  feature_list_.InitAndEnableFeature(
      first_run::kQuerySwitcherInfoSignalInFirstRun);
  AddIdentityOnDevice();
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  base::HistogramTester histogram_tester;

  StartCoordinatorWithScreens(@[ @(kSignIn), @(kStepsCompleted) ]);
  [coordinator_
      firstRunScreenCoordinatorWantsToBeStopped:coordinator_.childCoordinator];
  task_environment_.FastForwardBy(base::Seconds(1));

  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoResultHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencySuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencyFailureHistogram, 0);
}

// Tests that when kQuerySwitcherInfoSignalInFirstRun is disabled, the switcher
// info API is not queried on FRE completion and no metrics are recorded.
TEST_F(FirstRunCoordinatorTest,
       DoesNotRecordSwitcherInfoMetricsWhenFeatureDisabled) {
  feature_list_.InitAndDisableFeature(
      first_run::kQuerySwitcherInfoSignalInFirstRun);
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  base::HistogramTester histogram_tester;

  StartCoordinatorWithScreens(@[ @(kSignIn), @(kStepsCompleted) ]);
  SignInIdentity();
  task_environment_.FastForwardBy(base::Seconds(1));

  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoResultHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencySuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencyFailureHistogram, 0);
}

// Tests that finishing the sign-in screen before kStepsCompleted does not
// prematurely record switcher info metrics until all screens complete.
TEST_F(FirstRunCoordinatorTest,
       DoesNotRecordSwitcherInfoMetricsBeforeStepsCompleted) {
  feature_list_.InitAndEnableFeature(
      first_run::kQuerySwitcherInfoSignalInFirstRun);
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  base::HistogramTester histogram_tester;

  StartCoordinatorWithScreens(
      @[ @(kSignIn), @(kDefaultBrowserPromo), @(kStepsCompleted) ]);
  SignInIdentity();
  task_environment_.FastForwardBy(base::Seconds(1));

  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoResultHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencySuccessHistogram, 0);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencyFailureHistogram, 0);

  [coordinator_
      firstRunScreenCoordinatorWantsToBeStopped:coordinator_.childCoordinator];
  task_environment_.FastForwardBy(base::Seconds(1));

  histogram_tester.ExpectUniqueSample(
      first_run::kDefaultBrowserPromoSwitcherInfoResultHistogram,
      first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher, 1);
  histogram_tester.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSwitcherInfoLatencySuccessHistogram, 1);
}

}  // namespace
