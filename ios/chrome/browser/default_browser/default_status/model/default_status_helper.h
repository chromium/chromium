// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_STATUS_MODEL_DEFAULT_STATUS_HELPER_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_STATUS_MODEL_DEFAULT_STATUS_HELPER_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "base/functional/callback.h"

class PrefRegistrySimple;
class PrefService;

// This helper contains the necessary logic to integrate with the default status
// system API. At a high level, the integration works as follows:
//    - To spread the reporting out throughout the year, clients are randomly
//      assigned to a monthly cohort. The cohort assignment is persisted to
//      prefs.
//    - There are currently 4 cohorts, which correspond to the following months:
//        - Cohort 1: January, May, September
//        - Cohort 2: February, June, October
//        - Cohort 3: March, July, November
//        - Cohort 4: April, August, December
//    - At startup, if the current date is at or after the assigned cohort's
//      reporting window, a call to the default status API is triggered and
//      relevant metrics and prefs are recorded.
namespace default_status {

// Registers the prefs associated with the default status helper in `registry`.
void RegisterDefaultStatusPrefs(PrefRegistrySimple* registry);

// Queries the system API to determine whether Chrome is the default browser,
// and logs the result. This is NOT meant to be used by feature code, as the
// system call is rate limited. Instead, use the helpers in default browser
// utils to check whether Chrome is likely the default browser.
void TriggerDefaultStatusCheck();

// Executes the default status check using `local_state`, invoking `system_call`
// instead of the system API when the check is ready, and optionally forcing
// initial cohort assignment to `cohort_override` when set.
void TriggerDefaultStatusCheckForTesting(
    PrefService* local_state,
    base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)>
        system_call,
    std::optional<int> cohort_override) API_AVAILABLE(ios(18.4));

}  // namespace default_status

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_STATUS_MODEL_DEFAULT_STATUS_HELPER_H_
