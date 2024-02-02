// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/public/omnibox_position_util.h"

#import "base/stl_util.h"
#import "base/time/time.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"

namespace {
/// The time delta for a user to be considered as a new user.
const base::TimeDelta kNewUserTimeDelta = base::Days(60);

}  // namespace

namespace omnibox {

bool IsNewUser() {
  return IsFirstRunRecent(kNewUserTimeDelta);
}

bool IsSafariSwitcher(
    const segmentation_platform::ClassificationResult& result) {
  CHECK(result.status == segmentation_platform::PredictionStatus::kSucceeded);
  if (result.ordered_labels.empty()) {
    DUMP_WILL_BE_CHECK(!result.ordered_labels.empty());
    return false;
  }

  if (!IsNewUser()) {
    return false;
  }

  // Is considered a Safari switcher, a user that hasn't used Chrome on Android
  // or iOS recently. The number of days is defined by the segmentation team.
  std::vector<std::string> excludedLabels = {
      segmentation_platform::DeviceSwitcherModel::kAndroidPhoneLabel,
      segmentation_platform::DeviceSwitcherModel::kAndroidTabletLabel,
      segmentation_platform::DeviceSwitcherModel::kIosPhoneChromeLabel,
      segmentation_platform::DeviceSwitcherModel::kIosTabletLabel};
  std::sort(excludedLabels.begin(), excludedLabels.end());

  // `result` contains the list of recently used devices.
  auto sortedLabels = std::vector<std::string>(result.ordered_labels);
  std::sort(sortedLabels.begin(), sortedLabels.end());

  std::vector<std::string> intersection =
      base::STLSetIntersection<std::vector<std::string>>(sortedLabels,
                                                         excludedLabels);
  // Verify that the user hasn't used an `excluded` device recently.
  return intersection.empty();
}

}  // namespace omnibox
