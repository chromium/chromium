// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_TEST_UTILS_H_

#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/testing/mock_field_trial_register.h"
#import "components/sync_device_info/fake_device_info_tracker.h"
#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"
#import "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

namespace test {

// Fake DeviceSwitcherResultDispatcher with overridden getter for
// ClassificationResult.
class FakeDeviceSwitcherResultDispatcher
    : public segmentation_platform::DeviceSwitcherResultDispatcher {
 public:
  FakeDeviceSwitcherResultDispatcher(
      segmentation_platform::SegmentationPlatformService* segmentation_service,
      syncer::DeviceInfoTracker* device_info_tracker,
      PrefService* prefs,
      segmentation_platform::FieldTrialRegister* field_trial_register);

  ~FakeDeviceSwitcherResultDispatcher() override;

  void WaitForClassificationResult(
      base::TimeDelta timeout,
      segmentation_platform::ClassificationResultCallback callback) override;

  void SetSegmentLabel(segmentation_platform::DefaultBrowserUserSegment label);

  void SetPredictionStatus(segmentation_platform::PredictionStatus status);

 protected:
  segmentation_platform::ClassificationResult classification_result_;
  segmentation_platform::DefaultBrowserUserSegment segment_label_;
};

}  // namespace test

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_TEST_UTILS_H_
