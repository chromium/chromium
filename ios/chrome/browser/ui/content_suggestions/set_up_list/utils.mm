// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"

#import "base/time/time.h"
#import "ios/chrome/browser/first_run/first_run.h"
#import "ios/chrome/browser/ntp/set_up_list_prefs.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"

namespace set_up_list_utils {

bool IsSetUpListActive(PrefService* local_state) {
  if (set_up_list_prefs::IsSetUpListDisabled(local_state)) {
    return false;
  }
  if (FirstRun::IsChromeFirstRun()) {
    return false;
  }
  // Check if we are within 14 days of FRE
  absl::optional<base::Time> first_run_time = GetFirstRunTime();
  if (!first_run_time) {
    // If this is the first time the app has been opened, First Run will not
    // have been completed yet. In this case, we will wait until the next run.
    return false;
  }
  base::Time now = base::Time::Now();
  base::Time expiry_time = first_run_time.value() + base::Days(14);
  if (now > expiry_time) {
    // It has been 14+ days since FRE, but if user has interacted in the last
    // day the time will be extended.
    base::Time last_interaction =
        set_up_list_prefs::GetLastInteraction(local_state);
    if (now > last_interaction + base::Days(1)) {
      return false;
    }
  }

  return true;
}

bool ShouldShowCompactedSetUpListModule() {
  absl::optional<base::Time> firstRunTime = GetFirstRunTime();
  base::Time expiry_time =
      firstRunTime.value() + base::Days(TimeUntilShowingCompactedSetUpList());
  if (base::Time::Now() > expiry_time) {
    return true;
  }
  return false;
}

}  // namespace set_up_list_utils
