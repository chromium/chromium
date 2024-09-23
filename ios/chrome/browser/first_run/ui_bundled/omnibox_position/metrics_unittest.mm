// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/omnibox_position/metrics.h"

#import <memory>
#import <string_view>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "components/prefs/testing_pref_service.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/field_trial_register.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#import "components/sync_device_info/fake_device_info_tracker.h"
#import "ios/chrome/browser/shared/model/utils/first_run_test_util.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

/// Mock FieldTrialRegister for `DeviceSwitcherResultDispatcher`.
class MockFieldTrialRegister
    : public segmentation_platform::FieldTrialRegister {
 public:
  MOCK_METHOD(void,
              RegisterFieldTrial,
              (std::string_view trial_name, std::string_view group_name));

  MOCK_METHOD(void,
              RegisterSubsegmentFieldTrialIfNeeded,
              (std::string_view trial_name,
               segmentation_platform::proto::SegmentId segment_id,
               int subsegment_rank));
};

/// Fake `FakeDeviceSwitcherResultDispatcher` with public
/// `classification_result`.
class FakeDeviceSwitcherResultDispatcher
    : public segmentation_platform::DeviceSwitcherResultDispatcher {
 public:
  FakeDeviceSwitcherResultDispatcher(
      segmentation_platform::SegmentationPlatformService* segmentation_service,
      syncer::DeviceInfoTracker* device_info_tracker,
      PrefService* prefs,
      segmentation_platform::FieldTrialRegister* field_trial_register)
      : segmentation_platform::DeviceSwitcherResultDispatcher(
            segmentation_service,
            device_info_tracker,
            prefs,
            field_trial_register),
        classification_result(
            segmentation_platform::PredictionStatus::kNotReady) {}
  ~FakeDeviceSwitcherResultDispatcher() override = default;

  segmentation_platform::ClassificationResult GetCachedClassificationResult()
      override {
    return classification_result;
  }

  segmentation_platform::ClassificationResult classification_result;
};

class OmniboxPositionMetricsTest : public PlatformTest {
 public:
  OmniboxPositionMetricsTest() = default;
  ~OmniboxPositionMetricsTest() override = default;

  void SetUp() override {
    PlatformTest::SetUp();
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    segmentation_platform::DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
        prefs_->registry());
    device_info_tracker_ = std::make_unique<syncer::FakeDeviceInfoTracker>();
    device_switcher_result_dispatcher_ =
        std::make_unique<FakeDeviceSwitcherResultDispatcher>(
            &segmentation_platform_service_, device_info_tracker_.get(),
            prefs_.get(), &field_trial_register_);
  }

  void TearDown() override {
    PlatformTest::TearDown();
    device_switcher_result_dispatcher_.reset();
    prefs_.reset();
    device_info_tracker_.reset();
  }

 protected:
  // base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<syncer::FakeDeviceInfoTracker> device_info_tracker_;
  testing::NiceMock<segmentation_platform::MockSegmentationPlatformService>
      segmentation_platform_service_;
  testing::NiceMock<MockFieldTrialRegister> field_trial_register_;

  std::unique_ptr<FakeDeviceSwitcherResultDispatcher>
      device_switcher_result_dispatcher_;
};

// Tests selected position logging on startup without device switcher result.
TEST_F(OmniboxPositionMetricsTest,
       SelectedPositionStartupNoDeviceSwitcherResult) {
  base::HistogramTester histogram_tester;

  // Top default.
  RecordSelectedPosition(ToolbarType::kPrimary, /*is_default=*/YES, nullptr);
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup", /* Top default */ 0, 1);

  // Bottom default.
  RecordSelectedPosition(ToolbarType::kSecondary, /*is_default=*/YES, nullptr);
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup", /* Bottom default */ 1, 1);

  // Top not default.
  RecordSelectedPosition(ToolbarType::kPrimary, /*is_default=*/NO, nullptr);
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup", /* Top not default */ 2, 1);

  // Bottom not default.
  RecordSelectedPosition(ToolbarType::kSecondary, /*is_default=*/NO, nullptr);
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup", /* Bottom not default */ 3,
      1);

  // These histograms should not be logged when there is no device switcher
  // result.
  histogram_tester.ExpectTotalCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.NotNew", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.Unavailable", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.IsSwitcher", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.NotSwitcher", 0);
}

// Tests selected position logging on first run without device switcher result.
TEST_F(OmniboxPositionMetricsTest, SelectedPositionFRENoDeviceSwitcherResult) {
  base::HistogramTester histogram_tester;

  // Top default.
  RecordSelectedPosition(ToolbarType::kPrimary, /*is_default=*/YES, nullptr);
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup",
      /* Top default */ 0, 1);
  // Bottom default.
  RecordSelectedPosition(ToolbarType::kSecondary, /*is_default=*/YES, nullptr);
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup",
      /* Bottom default */ 1, 1);
  // Top not default.
  RecordSelectedPosition(ToolbarType::kPrimary, /*is_default=*/NO, nullptr);
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup",
      /* Top not default */ 2, 1);
  // Bottom not default.
  RecordSelectedPosition(ToolbarType::kSecondary, /*is_default=*/NO, nullptr);
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup",
      /* Bottom not default */ 3, 1);

  // These histograms should not be logged when there is no device switcher
  // result.
  histogram_tester.ExpectTotalCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.NotNew", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.Unavailable", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.IsSwitcher", 0);
  histogram_tester.ExpectTotalCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.NotSwitcher", 0);
}

// Tests selected position logging with device switcher results.
TEST_F(OmniboxPositionMetricsTest, SelectedPositionDeviceSwitcherResult) {
  base::HistogramTester histogram_tester;
  segmentation_platform::ClassificationResult& classification_result =
      device_switcher_result_dispatcher_->classification_result;
  (void)classification_result;

  // Reset to new user.
  ResetFirstRunSentinel();

  // New user without classification result.
  RecordSelectedPosition(ToolbarType::kPrimary, /*is_default=*/YES,
                         device_switcher_result_dispatcher_.get());
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.Unavailable",
      /* Top default */ 0, 1);

  // Set status to Succeeded for the following tests.
  classification_result.status =
      segmentation_platform::PredictionStatus::kSucceeded;

  // Safari switcher.
  classification_result.ordered_labels = std::vector<std::string>(
      {segmentation_platform::DeviceSwitcherModel::kDesktopLabel,
       segmentation_platform::DeviceSwitcherModel::kSyncedAndFirstDeviceLabel,
       segmentation_platform::DeviceSwitcherModel::kNotSyncedLabel});
  RecordSelectedPosition(ToolbarType::kPrimary, /*is_default=*/YES,
                         device_switcher_result_dispatcher_.get());
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.IsSwitcher",
      /* Top default */ 0, 1);

  // Not Safari switcher.
  classification_result.ordered_labels = std::vector<std::string>(
      {segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel,
       segmentation_platform::DeviceSwitcherModel::kIosTabletLabel});
  RecordSelectedPosition(ToolbarType::kPrimary, /*is_default=*/YES,
                         device_switcher_result_dispatcher_.get());
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.NotSwitcher",
      /* Top default */ 0, 1);

  // Not a new user.
  // Force first run recency to a large value.
  ForceFirstRunRecency(100);
  RecordSelectedPosition(ToolbarType::kPrimary, /*is_default=*/YES,
                         device_switcher_result_dispatcher_.get());
  histogram_tester.ExpectBucketCount(
      "IOS.Omnibox.Promo.SelectedPosition.Startup.NotNew",
      /* Top default */ 0, 1);

  ResetFirstRunSentinel();
}

}  // namespace
