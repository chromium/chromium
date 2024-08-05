// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_test_utils.h"

#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/shopping_user_model.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/segment_selection_result.h"

namespace segmentation_platform {

namespace test {

MockDeviceSwitcherResultDispatcher::MockDeviceSwitcherResultDispatcher(
    SegmentationPlatformService* segmentation_service,
    syncer::DeviceInfoTracker* device_info_tracker,
    PrefService* prefs,
    FieldTrialRegister* field_trial_register)
    : DeviceSwitcherResultDispatcher(segmentation_service,
                                     device_info_tracker,
                                     prefs,
                                     field_trial_register) {}
MockDeviceSwitcherResultDispatcher::~MockDeviceSwitcherResultDispatcher() =
    default;

void SetOrderedLabelsForTesting(
    DefaultBrowserUserSegment segment,
    std::vector<std::string>* device_switcher_labels,
    std::vector<std::string>* shopper_labels) {
  switch (segment) {
    case DefaultBrowserUserSegment::kDesktopUser:
      CHECK(device_switcher_labels);
      *device_switcher_labels = {DeviceSwitcherModel::kOtherLabel,
                                 DeviceSwitcherModel::kDesktopLabel,
                                 DeviceSwitcherModel::kAndroidPhoneLabel};
      return;
    case DefaultBrowserUserSegment::kAndroidSwitcher:
      CHECK(device_switcher_labels);
      *device_switcher_labels = {DeviceSwitcherModel::kOtherLabel,
                                 DeviceSwitcherModel::kAndroidPhoneLabel};
      return;
    case DefaultBrowserUserSegment::kShopper:
      CHECK(shopper_labels);
      *shopper_labels = {kShoppingUserUmaName};
      return;
    case DefaultBrowserUserSegment::kDefault:
      return;
  }
    NOTREACHED_NORETURN();
}

}  // namespace test

}  // namespace segmentation_platform
