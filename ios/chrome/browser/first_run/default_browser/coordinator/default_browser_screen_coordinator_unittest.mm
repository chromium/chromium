// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/default_browser/coordinator/default_browser_screen_coordinator.h"

#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/metrics/field_trial.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_command_line.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/public/features.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_android_switcher_screen_view_controller.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_animated_screen_view_controller.h"
#import "ios/chrome/browser/first_run/default_browser/ui/default_browser_screen_view_controller.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
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

std::unique_ptr<KeyedService> BuildSegmentationPlatformServiceWithLabels(
    std::optional<std::vector<std::string>> labels,
    ProfileIOS* profile) {
  if (labels.has_value()) {
    base::ListValue labels_list;
    for (const std::string& label : *labels) {
      labels_list.Append(label);
    }
    profile->GetPrefs()->SetDict(
        "segmentation_platform.device_switcher_util",
        base::DictValue().Set(
            "result", base::DictValue().Set("labels", std::move(labels_list))));
  }
  return segmentation_platform::SegmentationPlatformServiceFactory::
      GetDefaultFactory()
          .Run(profile);
}

// Unit tests for `DefaultBrowserScreenCoordinator`.
class DefaultBrowserScreenCoordinatorTest : public PlatformTest {
 protected:
  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    delegate_ = nil;
    navigation_controller_ = nil;
    browser_.reset();
    profile_ = nullptr;
    ios::provider::test::ResetSwitcherInfoState();
    PlatformTest::TearDown();
  }

  // Signs in with a fake identity.
  void SignIn() {
    if (!profile_) {
      ConfigureSegmentationLabels();
    }
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForProfile(profile_)->SignIn(
        identity, signin_metrics::AccessPoint::kStartPage);
  }

  // Returns the parameter string corresponding to `variation`.
  const char* VariationParamValue(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType
          variation) {
    switch (variation) {
      case first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kDisabled:
        return first_run::kAndroidSwitcherDefaultBrowserPromoInFREParamDisabled;
      case first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSegmentationSignal:
        return first_run::
            kAndroidSwitcherDefaultBrowserPromoInFREParamSegmentationSignal;
      case first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSwitcherInfoSignal:
        return first_run::
            kAndroidSwitcherDefaultBrowserPromoInFREParamSwitcherInfoSignal;
    }
  }

  // Enables `variation` of `kAndroidSwitcherDefaultBrowserPromoInFRE` and
  // disables the animated promo.
  void EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType
          variation) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{first_run::kAndroidSwitcherDefaultBrowserPromoInFRE,
          {{first_run::kAndroidSwitcherDefaultBrowserPromoInFREParam,
            VariationParamValue(variation)}}}},
        /*disabled_features=*/{first_run::kAnimatedDefaultBrowserPromoInFRE});
  }

  // Initializes `profile_` and `coordinator_` with cached segmentation
  // `labels`.
  void ConfigureSegmentationLabels(
      std::optional<std::vector<std::string>> labels = std::nullopt) {
    [coordinator_ stop];
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        segmentation_platform::SegmentationPlatformServiceFactory::
            GetInstance(),
        base::BindOnce(&BuildSegmentationPlatformServiceWithLabels,
                       std::move(labels)));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_);
    navigation_controller_ = [[UINavigationController alloc] init];
    delegate_ = [[DefaultBrowserFirstRunScreenTestDelegate alloc] init];
    coordinator_ = [[DefaultBrowserScreenCoordinator alloc]
        initWithBaseNavigationController:navigation_controller_
                                 browser:browser_.get()
                                delegate:delegate_];
  }

  // Verifies the recorded segmentation result and latency histograms.
  void ExpectSignalMetrics(
      first_run::DefaultBrowserPromoSegmentationResult expected_result,
      bool is_success) {
    histogram_tester_.ExpectUniqueSample(
        first_run::kDefaultBrowserPromoSegmentationResultHistogram,
        expected_result, 1);
    histogram_tester_.ExpectTotalCount(
        first_run::kDefaultBrowserPromoSegmentationLatencySuccessHistogram,
        is_success ? 1 : 0);
    histogram_tester_.ExpectTotalCount(
        first_run::kDefaultBrowserPromoSegmentationLatencyFailureHistogram,
        is_success ? 0 : 1);
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
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
  histogram_tester_.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram, 0);
}

// Tests that the standard static Default Browser promo is presented without
// activating the Finch trial when the Android Switcher flag is enabled in a
// study but the user is signed out.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWhenSignedOut) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSegmentationSignal);
  ConfigureSegmentationLabels(std::vector<std::string>{
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel});

  [coordinator_ start];

  UIViewController* presented_view_controller =
      navigation_controller_.topViewController;
  EXPECT_TRUE([presented_view_controller
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
  histogram_tester_.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram, 0);
  base::FieldTrial* trial = base::FeatureList::GetFieldTrial(
      first_run::kAndroidSwitcherDefaultBrowserPromoInFRE);
  ASSERT_TRUE(trial);
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));
}

// Tests that when the segmentation signal variation is enabled and the user is
// classified as an Android switcher, the Android Switcher promo is presented
// and `kAndroidSwitcher` with success latency is recorded.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsAndroidSwitcherPromoWhenSegmentedAsAndroidSwitcher) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSegmentationSignal);
  ConfigureSegmentationLabels(std::vector<std::string>{
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel});
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
  ExpectSignalMetrics(
      first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher,
      /*is_success=*/true);
}

// Tests that when the user has both Android phone and iOS phone Chrome labels,
// the standard promo is presented and `kNotAndroidSwitcher` is recorded.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWhenIosPhoneLabelPresent) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSegmentationSignal);
  ConfigureSegmentationLabels(std::vector<std::string>{
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel,
      segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel});
  SignIn();

  [coordinator_ start];

  EXPECT_TRUE([navigation_controller_.topViewController
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
  ExpectSignalMetrics(
      first_run::DefaultBrowserPromoSegmentationResult::kNotAndroidSwitcher,
      /*is_success=*/true);
}

// Tests that when the segmentation request times out after 1 second, the
// standard promo is presented and `kNotReady` with failure latency is recorded.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWhenSegmentationTimesOut) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{first_run::kAndroidSwitcherDefaultBrowserPromoInFRE,
        {{first_run::kAndroidSwitcherDefaultBrowserPromoInFREParam,
          first_run::
              kAndroidSwitcherDefaultBrowserPromoInFREParamSegmentationSignal}}}},
      /*disabled_features=*/{
          first_run::kAnimatedDefaultBrowserPromoInFRE,
          segmentation_platform::features::kSegmentationPlatformDeviceSwitcher,
          segmentation_platform::features::kSegmentationPlatformUmaFromSqlDb});
  ConfigureSegmentationLabels();
  SignIn();

  [coordinator_ start];
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_TRUE([navigation_controller_.topViewController
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
  ExpectSignalMetrics(
      first_run::DefaultBrowserPromoSegmentationResult::kNotReady,
      /*is_success=*/false);
}

// Tests that when `kNotSyncedLabel` is present in the segmentation result, the
// standard promo is presented and `kNotReady` is recorded.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWhenNotSyncedLabelPresent) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSegmentationSignal);
  ConfigureSegmentationLabels(std::vector<std::string>{
      segmentation_platform::DeviceSwitcherModel::kNotSyncedLabel});
  SignIn();

  [coordinator_ start];

  EXPECT_TRUE([navigation_controller_.topViewController
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
  ExpectSignalMetrics(
      first_run::DefaultBrowserPromoSegmentationResult::kNotReady,
      /*is_success=*/false);
}

// Tests that when the segmentation result has empty labels, the standard promo
// is presented and `kFailed` is recorded.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWhenSegmentationLabelsEmpty) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSegmentationSignal);
  ConfigureSegmentationLabels(std::vector<std::string>{});
  SignIn();

  [coordinator_ start];

  EXPECT_TRUE([navigation_controller_.topViewController
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
  ExpectSignalMetrics(first_run::DefaultBrowserPromoSegmentationResult::kFailed,
                      /*is_success=*/false);
}

// Tests that when the forced device switcher flag is set to `AndroidPhone`, the
// Android Switcher promo is presented without recording segmentation metrics.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsAndroidSwitcherPromoWhenForcedDeviceSwitcherFlagSet) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSegmentationSignal);
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      "force-device-switcher-experience",
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel);
  SignIn();

  [coordinator_ start];

  EXPECT_TRUE([navigation_controller_.topViewController
      isMemberOfClass:[DefaultBrowserAndroidSwitcherScreenViewController
                          class]]);
  histogram_tester_.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram, 0);
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

// Tests that stopping the coordinator while a segmentation signal request is in
// flight does not present a screen or record metrics when the timeout fires.
TEST_F(
    DefaultBrowserScreenCoordinatorTest,
    StopWhileSegmentationRequestInFlightDoesNotPresentScreenOrRecordMetrics) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{first_run::kAndroidSwitcherDefaultBrowserPromoInFRE,
        {{first_run::kAndroidSwitcherDefaultBrowserPromoInFREParam,
          first_run::
              kAndroidSwitcherDefaultBrowserPromoInFREParamSegmentationSignal}}}},
      /*disabled_features=*/{
          first_run::kAnimatedDefaultBrowserPromoInFRE,
          segmentation_platform::features::kSegmentationPlatformDeviceSwitcher,
          segmentation_platform::features::kSegmentationPlatformUmaFromSqlDb});
  ConfigureSegmentationLabels();
  SignIn();

  [coordinator_ start];
  [coordinator_ stop];
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_EQ(navigation_controller_.topViewController, nil);
  histogram_tester_.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram, 0);
}

// Tests that when the switcher info signal variation is enabled in a study but
// the user is signed out, the standard promo is presented without activating
// the Finch trial.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWithoutActivatingTrialWhenSignedOutInSwitcherInfo) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSwitcherInfoSignal);
  ConfigureSegmentationLabels();

  [coordinator_ start];

  EXPECT_TRUE([navigation_controller_.topViewController
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
  histogram_tester_.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram, 0);
  base::FieldTrial* trial = base::FeatureList::GetFieldTrial(
      first_run::kAndroidSwitcherDefaultBrowserPromoInFRE);
  ASSERT_TRUE(trial);
  EXPECT_FALSE(base::FieldTrialList::IsTrialActive(trial->trial_name()));
}

// Tests that when the switcher info signal variation is enabled and the API
// returns `kSwitcher`, the Android Switcher promo is presented and
// `kAndroidSwitcher` with success latency is recorded.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsAndroidSwitcherPromoWhenSwitcherInfoReturnsSwitcher) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSwitcherInfoSignal);
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  SignIn();

  [coordinator_ start];
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return navigation_controller_.topViewController != nil; }));

  EXPECT_TRUE([navigation_controller_.topViewController
      isMemberOfClass:[DefaultBrowserAndroidSwitcherScreenViewController
                          class]]);
  ExpectSignalMetrics(
      first_run::DefaultBrowserPromoSegmentationResult::kAndroidSwitcher,
      /*is_success=*/true);
}

// Tests that when the switcher info signal variation is enabled and the API
// returns `kNotSwitcher`, the standard promo is presented and
// `kNotAndroidSwitcher` with success latency is recorded.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWhenSwitcherInfoReturnsNotSwitcher) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSwitcherInfoSignal);
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kNotSwitcher,
      .is_chrome_user = false});
  SignIn();

  [coordinator_ start];
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return navigation_controller_.topViewController != nil; }));

  EXPECT_TRUE([navigation_controller_.topViewController
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
  ExpectSignalMetrics(
      first_run::DefaultBrowserPromoSegmentationResult::kNotAndroidSwitcher,
      /*is_success=*/true);
}

// Tests that when the switcher info signal variation is enabled and the API
// returns `kUnknown`, the standard promo is presented and `kNotReady` with
// failure latency is recorded.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWhenSwitcherInfoReturnsUnknown) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSwitcherInfoSignal);
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kUnknown,
      .is_chrome_user = false});
  SignIn();

  [coordinator_ start];
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return navigation_controller_.topViewController != nil; }));

  EXPECT_TRUE([navigation_controller_.topViewController
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
  ExpectSignalMetrics(
      first_run::DefaultBrowserPromoSegmentationResult::kNotReady,
      /*is_success=*/false);
}

// Tests that when the switcher info signal variation is enabled and the API
// fails (`std::nullopt`), the standard promo is presented and `kFailed` with
// failure latency is recorded.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       PresentsStandardPromoWhenSwitcherInfoFails) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSwitcherInfoSignal);
  ios::provider::test::SetSwitcherInfoResult(std::nullopt);
  SignIn();

  [coordinator_ start];
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return navigation_controller_.topViewController != nil; }));

  EXPECT_TRUE([navigation_controller_.topViewController
      isMemberOfClass:[DefaultBrowserScreenViewController class]]);
  ExpectSignalMetrics(first_run::DefaultBrowserPromoSegmentationResult::kFailed,
                      /*is_success=*/false);
}

// Tests that stopping the coordinator while a switcher info request is in
// flight does not present a screen or record metrics when the request later
// completes.
TEST_F(DefaultBrowserScreenCoordinatorTest,
       StopWhileSignalRequestInFlightDoesNotPresentScreenOrRecordMetrics) {
  EnableVariation(
      first_run::AndroidSwitcherDefaultBrowserPromoInFREVariationType::
          kSwitcherInfoSignal);
  ios::provider::test::SetSwitcherInfoResult(ios::provider::SwitcherInfoResult{
      .switcher_status = ios::provider::SwitcherStatus::kSwitcher,
      .is_chrome_user = true});
  SignIn();

  [coordinator_ start];
  [coordinator_ stop];
  task_environment_.FastForwardBy(base::Seconds(1));

  EXPECT_EQ(navigation_controller_.topViewController, nil);
  histogram_tester_.ExpectTotalCount(
      first_run::kDefaultBrowserPromoSegmentationResultHistogram, 0);
}

}  // namespace
