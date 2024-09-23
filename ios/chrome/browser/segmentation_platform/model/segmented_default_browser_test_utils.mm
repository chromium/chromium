// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_test_utils.h"

#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/shopping_user_model.h"
#import "components/segmentation_platform/public/constants.h"

namespace segmentation_platform {

namespace test {

std::vector<std::string> GetDeviceSwitcherOrderedLabelsForTesting(
    DefaultBrowserUserSegment segment) {
  switch (segment) {
    case DefaultBrowserUserSegment::kDesktopUser:
      return {DeviceSwitcherModel::kOtherLabel,
              DeviceSwitcherModel::kDesktopLabel,
              DeviceSwitcherModel::kAndroidPhoneLabel};
    case DefaultBrowserUserSegment::kAndroidSwitcher:
      return {DeviceSwitcherModel::kOtherLabel,
              DeviceSwitcherModel::kAndroidPhoneLabel};
    case DefaultBrowserUserSegment::kShopper:
    case DefaultBrowserUserSegment::kDefault:
      return {DeviceSwitcherModel::kOtherLabel};
  }
}

std::vector<std::string> GetShopperOrderedLabelsForTesting(
    DefaultBrowserUserSegment segment) {
  switch (segment) {
    case DefaultBrowserUserSegment::kShopper:
      return {kShoppingUserUmaName};
    case DefaultBrowserUserSegment::kDesktopUser:
    case DefaultBrowserUserSegment::kAndroidSwitcher:
    case DefaultBrowserUserSegment::kDefault:
      return {kLegacyNegativeLabel};
  }
}

}  // namespace test

}  // namespace segmentation_platform
