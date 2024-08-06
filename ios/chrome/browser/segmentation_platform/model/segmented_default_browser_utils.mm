// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"

#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/shopping_user_model.h"
#import "components/segmentation_platform/public/constants.h"

namespace segmentation_platform {

const base::TimeDelta kDeviceSwitcherWaitTimeout = base::Seconds(1);

DefaultBrowserUserSegment GetDefaultBrowserUserSegment(
    const ClassificationResult* device_switcher_result,
    const ClassificationResult* shopper_result) {
  if (device_switcher_result &&
      device_switcher_result->status == PredictionStatus::kSucceeded) {
    if (std::find(device_switcher_result->ordered_labels.begin(),
                  device_switcher_result->ordered_labels.end(),
                  DeviceSwitcherModel::kDesktopLabel) !=
        device_switcher_result->ordered_labels.end()) {
      return DefaultBrowserUserSegment::kDesktopUser;
    }
    if (std::find(device_switcher_result->ordered_labels.begin(),
                  device_switcher_result->ordered_labels.end(),
                  DeviceSwitcherModel::kAndroidPhoneLabel) !=
        device_switcher_result->ordered_labels.end()) {
      return DefaultBrowserUserSegment::kAndroidSwitcher;
    }
  }

  if (shopper_result &&
      shopper_result->status == PredictionStatus::kSucceeded) {
    // A shopper segment classification result is binary, `ordered_labels`
    // should only have one label.
    if (std::find(shopper_result->ordered_labels.begin(),
                  shopper_result->ordered_labels.end(), kShoppingUserUmaName) !=
        shopper_result->ordered_labels.end()) {
      return DefaultBrowserUserSegment::kShopper;
    }
  }
  return DefaultBrowserUserSegment::kDefault;
}

}  // namespace segmentation_platform
