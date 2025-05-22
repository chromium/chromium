// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/set_up_list/utils.h"

#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/ntp/model/features.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace set_up_list_utils {

bool IsSetUpListActive(PrefService* local_prefs,
                       PrefService* user_prefs,
                       bool include_disable_pref) {
  if (!user_prefs->GetBoolean(
          prefs::kHomeCustomizationMagicStackSetUpListEnabled)) {
    return false;
  }
  // Check if we are within the duration of the Set Up List, relevant to the
  // FRE.
  if (IsFirstRun()) {
    // If this is the first time the app has been opened, First Run will not
    // have been completed yet. In this case, we will wait until the next run.
    return false;
  }
  if (!IsFirstRunRecent(set_up_list::SetUpListDurationPastFirstRun())) {
    // It is past the max duration of the Set Up List, but if user has
    // interacted in the last day the time will be extended.
    base::Time last_interaction =
        set_up_list_prefs::GetLastInteraction(local_prefs);
    if (base::Time::Now() > last_interaction + base::Days(1)) {
      return false;
    }
  }

  return true;
}

bool ShouldShowCompactedSetUpListModule() {
  return !IsFirstRun();
}

}  // namespace set_up_list_utils
