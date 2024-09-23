// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_default_browser_promo_mediator.h"

#import "base/test/gmock_callback_support.h"
#import "base/test/scoped_feature_list.h"
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
#import "ui/base/l10n/l10n_util.h"

using base::test::RunOnceCallback;
using base::test::ScopedFeatureList;
using l10n_util::GetNSString;
using syncer::FakeDeviceInfoTracker;
using testing::_;
using testing::NiceMock;
using testing::Return;

namespace segmentation_platform {

namespace test {

// Test class for SetUpListDefaultBrowserPromoMediator.
class SetUpListDefaultBrowserPromoMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    scoped_feature_list_.InitAndEnableFeature(kSegmentedDefaultBrowserPromo);
    DeviceSwitcherResultDispatcher::RegisterProfilePrefs(prefs_->registry());
    device_info_tracker_ = std::make_unique<FakeDeviceInfoTracker>();
    consumer_mock_ =
        OCMStrictProtocolMock(@protocol(DefaultBrowserScreenConsumer));
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)consumer_mock_);
    PlatformTest::TearDown();
    mediator_to_test_.consumer = nil;
    [mediator_to_test_ disconnect];
    mediator_to_test_ = nil;
  }

  void SetUpMediatorTest(DefaultBrowserUserSegment label,
                         PredictionStatus status) {
    ClassificationResult device_switcher_result(status);
    ClassificationResult shopper_result(status);
    device_switcher_result.ordered_labels =
        GetDeviceSwitcherOrderedLabelsForTesting(label);
    shopper_result.ordered_labels = GetShopperOrderedLabelsForTesting(label);

    NiceMock<MockDeviceSwitcherResultDispatcher>
        device_switcher_result_dispatcher(&segmentation_platform_service_,
                                          device_info_tracker_.get(),
                                          prefs_.get(), &field_trial_register_);

    EXPECT_CALL(device_switcher_result_dispatcher,
                GetCachedClassificationResult())
        .WillOnce(Return(device_switcher_result));

    EXPECT_CALL(segmentation_platform_service_,
                GetClassificationResult(_, _, _, _))
        .WillOnce(RunOnceCallback<3>(shopper_result));

    mediator_to_test_ = [[SetUpListDefaultBrowserPromoMediator alloc]
           initWithSegmentationService:&segmentation_platform_service_
        deviceSwitcherResultDispatcher:&device_switcher_result_dispatcher];
    [mediator_to_test_ retrieveUserSegmentWithCompletion:^{
    }];
  }

  void ExpectTextForSegment(DefaultBrowserUserSegment label) {
    OCMExpect([consumer_mock_
        setPromoTitle:GetNSString(GetFirstRunDefaultBrowserScreenTitleStringID(
                          label))]);
    OCMExpect([consumer_mock_
        setPromoSubtitle:GetNSString(
                             GetFirstRunDefaultBrowserScreenSubtitleStringID(
                                 label))]);
    mediator_to_test_.consumer = consumer_mock_;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<FakeDeviceInfoTracker> device_info_tracker_;
  NiceMock<MockSegmentationPlatformService> segmentation_platform_service_;
  NiceMock<MockFieldTrialRegister> field_trial_register_;
  SetUpListDefaultBrowserPromoMediator* mediator_to_test_;
  id<DefaultBrowserScreenConsumer> consumer_mock_;
};

#pragma mark - Unit Tests

// Tests that consumer is correctly informed if a user's retrieved segment is
// Desktop User.
TEST_F(SetUpListDefaultBrowserPromoMediatorTest, UserIsDesktopUser) {
  SetUpMediatorTest(DefaultBrowserUserSegment::kDesktopUser,
                    PredictionStatus::kSucceeded);
  ExpectTextForSegment(DefaultBrowserUserSegment::kDesktopUser);
}

// Tests that consumer is correctly informed if a user's retrieved segment is
// Android switcher.
TEST_F(SetUpListDefaultBrowserPromoMediatorTest, UserIsAndroidSwitcher) {
  SetUpMediatorTest(DefaultBrowserUserSegment::kAndroidSwitcher,
                    PredictionStatus::kSucceeded);
  ExpectTextForSegment(DefaultBrowserUserSegment::kAndroidSwitcher);
}

// Tests that consumer is correctly informed if a user's retrieved segment is
// Shopper.
TEST_F(SetUpListDefaultBrowserPromoMediatorTest, UserIsShopper) {
  SetUpMediatorTest(DefaultBrowserUserSegment::kShopper,
                    PredictionStatus::kSucceeded);
  ExpectTextForSegment(DefaultBrowserUserSegment::kShopper);
}

// Tests that consumer is correctly informed if a user's retrieved segment is
// not a targeted segment.
TEST_F(SetUpListDefaultBrowserPromoMediatorTest, UserIsNotInTargetedSegment) {
  SetUpMediatorTest(DefaultBrowserUserSegment::kDefault,
                    PredictionStatus::kSucceeded);
  ExpectTextForSegment(DefaultBrowserUserSegment::kDefault);
}

// Tests that consumer is correctly informed if user classification is not ready
// yet.
TEST_F(SetUpListDefaultBrowserPromoMediatorTest, ClassificationNotReady) {
  SetUpMediatorTest(DefaultBrowserUserSegment::kDesktopUser,
                    PredictionStatus::kNotReady);
  ExpectTextForSegment(DefaultBrowserUserSegment::kDefault);
}

// Tests that consumer is correctly informed if user classification retrieval
// fails.
TEST_F(SetUpListDefaultBrowserPromoMediatorTest, ClassificationFailed) {
  SetUpMediatorTest(DefaultBrowserUserSegment::kDesktopUser,
                    PredictionStatus::kFailed);
  ExpectTextForSegment(DefaultBrowserUserSegment::kDefault);
}

}  // namespace test

}  // namespace segmentation_platform
