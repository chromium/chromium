// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/iph_for_new_chrome_user/model/utils.h"

#import "base/containers/contains.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/features.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

namespace {

constexpr base::TimeDelta kNewUserFirstRunRecency = base::Days(60);

bool IsUserSafariSwitcher(
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher) {
  if (!dispatcher) {
    return false;
  }

  bool is_new_chrome_user_forced =
      experimental_flags::GetSegmentForForcedDeviceSwitcherExperience() ==
      segmentation_platform::DeviceSwitcherModel::kSyncedAndFirstDeviceLabel;
  if (is_new_chrome_user_forced) {
    return true;
  }
  segmentation_platform::ClassificationResult result =
      dispatcher->GetCachedClassificationResult();
  // Use the device switcher classification result to determine the user is new
  // to Chrome across all devices and platforms.
  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    return false;
  }
  return base::Contains(
      result.ordered_labels,
      segmentation_platform::DeviceSwitcherModel::kSyncedAndFirstDeviceLabel);
}
}  // namespace
namespace iph_for_new_chrome_user {

struct ClassificationResult;

bool IsUserNewSafariSwitcher(
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher) {
  // Evaluate the experiment `kIPHForSafariSwitcher` only after
  // `IsUserSafariSwitcher` returns true, to avoid putting ineligible users in
  // the experiment.
  return IsFirstRunRecent(kNewUserFirstRunRecency) &&
         IsUserSafariSwitcher(dispatcher) &&
         base::FeatureList::IsEnabled(kIPHForSafariSwitcher);
}

}  // namespace iph_for_new_chrome_user
