// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_TEST_UTILS_H_

#import "components/segmentation_platform/public/result.h"
#import "components/segmentation_platform/public/testing/mock_device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/testing/mock_field_trial_register.h"
#import "components/sync_device_info/fake_device_info_tracker.h"
#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"
#import "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

namespace test {

// Helper function for testing that returns classification result labels for a
// device switcher segment `ClassificationResult` according to a user
// segmentation classification.
std::vector<std::string> GetDeviceSwitcherOrderedLabelsForTesting(
    DefaultBrowserUserSegment segment);

// Helper function for testing that returns classification result labels for a
// shopper segment `ClassificationResult` according to a user segmentation
// classification.
std::vector<std::string> GetShopperOrderedLabelsForTesting(
    DefaultBrowserUserSegment segment);

}  // namespace test

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_SEGMENTED_DEFAULT_BROWSER_TEST_UTILS_H_
