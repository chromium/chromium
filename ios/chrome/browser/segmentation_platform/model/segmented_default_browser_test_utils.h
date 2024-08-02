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

// TODO(crbug.com/1418395): Rename this file to
// mock_device_switcher_result_dispatcher.h
class MockDeviceSwitcherResultDispatcher
    : public DeviceSwitcherResultDispatcher {
 public:
  MockDeviceSwitcherResultDispatcher(
      SegmentationPlatformService* segmentation_service,
      syncer::DeviceInfoTracker* device_info_tracker,
      PrefService* prefs,
      FieldTrialRegister* field_trial_register);

  ~MockDeviceSwitcherResultDispatcher() override;

  MOCK_METHOD(void,
              WaitForClassificationResult,
              (base::TimeDelta, ClassificationResultCallback));

  MOCK_METHOD(ClassificationResult, GetCachedClassificationResult, ());
};

// Helper function for testing that sets classification result labels according
// to a user segmentation classification. The labels are set in the parameters
// `device_switcher_labels` and `shopper_labels`.
void SetOrderedLabelsForTesting(
    DefaultBrowserUserSegment segment,
    std::vector<std::string>* device_switcher_labels,
    std::vector<std::string>* shopper_labels);

}  // namespace test

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_TEST_UTILS_H_
