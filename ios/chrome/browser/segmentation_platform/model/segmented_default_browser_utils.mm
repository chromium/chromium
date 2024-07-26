// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"

#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/public/result.h"

namespace segmentation_platform {

const base::TimeDelta kDeviceSwitcherWaitTimeout = base::Seconds(1);

DefaultBrowserUserSegment GetDefaultBrowserUserSegment(
    const ClassificationResult& device_switcher_result) {
  if (device_switcher_result.status != PredictionStatus::kSucceeded) {
    return DefaultBrowserUserSegment::kDefault;
  }

  if (std::find(device_switcher_result.ordered_labels.begin(),
                device_switcher_result.ordered_labels.end(),
                DeviceSwitcherModel::kDesktopLabel) !=
      device_switcher_result.ordered_labels.end()) {
    return DefaultBrowserUserSegment::kDesktopUser;
  }
  if (std::find(device_switcher_result.ordered_labels.begin(),
                device_switcher_result.ordered_labels.end(),
                DeviceSwitcherModel::kAndroidPhoneLabel) !=
      device_switcher_result.ordered_labels.end()) {
    return DefaultBrowserUserSegment::kAndroidSwitcher;
  }
  return DefaultBrowserUserSegment::kDefault;
}

}  // namespace segmentation_platform
