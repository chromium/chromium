// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_mediator.h"

#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#import "ios/chrome/browser/first_run/ui_bundled/default_browser/default_browser_screen_consumer.h"
#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_test_utils.h"
#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

// Test class for DefaultBrowserScreenMediator.
class DefaultBrowserScreenMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    segmentation_platform::DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
        prefs_->registry());
    device_info_tracker_ = std::make_unique<syncer::FakeDeviceInfoTracker>();
    device_switcher_result_dispatcher_ = std::make_unique<
        segmentation_platform::test::FakeDeviceSwitcherResultDispatcher>(
        &segmentation_platform_service_, device_info_tracker_.get(),
        prefs_.get(), &field_trial_register_);
    consumer_mock_ =
        OCMStrictProtocolMock(@protocol(DefaultBrowserScreenConsumer));
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)consumer_mock_);
    PlatformTest::TearDown();
    mediator_to_test_ = nil;
    device_switcher_result_dispatcher_.reset();
    prefs_.reset();
    device_info_tracker_.reset();
  }

  void SetUpMediatorTest(segmentation_platform::DefaultBrowserUserSegment label,
                         segmentation_platform::PredictionStatus status) {
    device_switcher_result_dispatcher_->SetSegmentLabel(label);
    device_switcher_result_dispatcher_->SetPredictionStatus(status);
    mediator_to_test_ = [[DefaultBrowserScreenMediator alloc]
           initWithSegmentationService:&segmentation_platform_service_
        deviceSwitcherResultDispatcher:device_switcher_result_dispatcher_
                                           .get()];
  }

 protected:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<syncer::FakeDeviceInfoTracker> device_info_tracker_;
  testing::NiceMock<segmentation_platform::MockSegmentationPlatformService>
      segmentation_platform_service_;
  testing::NiceMock<segmentation_platform::MockFieldTrialRegister>
      field_trial_register_;
  std::unique_ptr<
      segmentation_platform::test::FakeDeviceSwitcherResultDispatcher>
      device_switcher_result_dispatcher_;
  DefaultBrowserScreenMediator* mediator_to_test_;
  id<DefaultBrowserScreenConsumer> consumer_mock_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

#pragma mark - Unit Tests

// Tests that consumer is correctly informed if a user's retrieved segment is
// Desktop User.
TEST_F(DefaultBrowserScreenMediatorTest, UserIsDesktopUser) {
  SetUpMediatorTest(
      segmentation_platform::DefaultBrowserUserSegment::kDesktopUser,
      segmentation_platform::PredictionStatus::kSucceeded);
  OCMExpect([consumer_mock_
      setPromoTitle:
          l10n_util::GetNSString(
              UseIPadTailoredStringForDefaultBrowserPromo()
                  ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPAD
                  : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPHONE)]);
  OCMExpect([consumer_mock_
      setPromoSubtitle:
          l10n_util::GetNSString(
              UseIPadTailoredStringForDefaultBrowserPromo()
                  ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DESKTOP_USER_SUBTITLE_IPAD
                  : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DESKTOP_USER_SUBTITLE_IPHONE)]);
  mediator_to_test_.consumer = consumer_mock_;
}

// Tests that consumer is correctly informed if a user's retrieved segment is
// Android switcher.
TEST_F(DefaultBrowserScreenMediatorTest, UserIsAndroidSwitcher) {
  SetUpMediatorTest(
      segmentation_platform::DefaultBrowserUserSegment::kAndroidSwitcher,
      segmentation_platform::PredictionStatus::kSucceeded);
  OCMExpect([consumer_mock_
      setPromoTitle:
          l10n_util::GetNSString(
              IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPHONE)]);
  OCMExpect([consumer_mock_
      setPromoSubtitle:
          l10n_util::GetNSString(
              UseIPadTailoredStringForDefaultBrowserPromo()
                  ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_ANDROID_SWITCHER_SUBTITLE_IPAD
                  : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_ANDROID_SWITCHER_SUBTITLE_IPHONE)]);
  mediator_to_test_.consumer = consumer_mock_;
}

// Tests that consumer is correctly informed if a user's retrieved segment is
// not a targeted segment.
TEST_F(DefaultBrowserScreenMediatorTest, UserIsNotInTargetedSegment) {
  SetUpMediatorTest(segmentation_platform::DefaultBrowserUserSegment::kDefault,
                    segmentation_platform::PredictionStatus::kSucceeded);
  OCMExpect([consumer_mock_
      setPromoTitle:
          l10n_util::GetNSString(
              UseIPadTailoredStringForDefaultBrowserPromo()
                  ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
                  : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE)]);
  OCMExpect([consumer_mock_
      setPromoSubtitle:
          l10n_util::GetNSString(
              UseIPadTailoredStringForDefaultBrowserPromo()
                  ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
                  : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE)]);
  mediator_to_test_.consumer = consumer_mock_;
}

// Tests that consumer is correctly informed if user classification is not ready
// yet.
TEST_F(DefaultBrowserScreenMediatorTest, ClassificationNotReady) {
  SetUpMediatorTest(
      segmentation_platform::DefaultBrowserUserSegment::kDesktopUser,
      segmentation_platform::PredictionStatus::kNotReady);
  OCMExpect([consumer_mock_
      setPromoTitle:
          l10n_util::GetNSString(
              UseIPadTailoredStringForDefaultBrowserPromo()
                  ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
                  : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE)]);
  OCMExpect([consumer_mock_
      setPromoSubtitle:
          l10n_util::GetNSString(
              UseIPadTailoredStringForDefaultBrowserPromo()
                  ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
                  : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE)]);
  mediator_to_test_.consumer = consumer_mock_;
}

// Tests that consumer is correctly informed if user classification retrieval
// fails.
TEST_F(DefaultBrowserScreenMediatorTest, ClassificationFailed) {
  SetUpMediatorTest(
      segmentation_platform::DefaultBrowserUserSegment::kDesktopUser,
      segmentation_platform::PredictionStatus::kFailed);
  OCMExpect([consumer_mock_
      setPromoTitle:
          l10n_util::GetNSString(
              UseIPadTailoredStringForDefaultBrowserPromo()
                  ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
                  : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE)]);
  OCMExpect([consumer_mock_
      setPromoSubtitle:
          l10n_util::GetNSString(
              UseIPadTailoredStringForDefaultBrowserPromo()
                  ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
                  : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE)]);
  mediator_to_test_.consumer = consumer_mock_;
}
