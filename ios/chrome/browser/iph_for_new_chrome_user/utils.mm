// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/iph_for_new_chrome_user/utils.h"

#import "base/containers/contains.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#import "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#import "components/segmentation_platform/public/result.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

base::TimeDelta new_user_first_run_recency = base::Days(60);

bool IsUserNewChromeUser(
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher) {
  if (!dispatcher) {
    return false;
  }

  // Use the first_run age to determine the user is new on this device.
  bool first_run = FirstRun::IsChromeFirstRun() ||
                   experimental_flags::AlwaysDisplayFirstRun();
  if (!first_run) {
    absl::optional<base::File::Info> info = FirstRun::GetSentinelInfo();
    if (info.has_value()) {
      base::Time first_run_time = info.value().creation_time;
      bool is_first_run_recent =
          base::Time::Now() - first_run_time < new_user_first_run_recency;
      if (!is_first_run_recent) {
        return false;
      }
    }
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

bool IsUserEligible(
    segmentation_platform::DeviceSwitcherResultDispatcher* dispatcher) {
  // Evaluate the experiment `kIPHForSafariSwitcher` only after
  // `IsUserNewChromeUser` returns true, to avoid putting ineligible users in
  // the experiment.
  return IsUserNewChromeUser(dispatcher) &&
         base::FeatureList::IsEnabled(kIPHForSafariSwitcher);
}

}  // namespace iph_for_new_chrome_user
