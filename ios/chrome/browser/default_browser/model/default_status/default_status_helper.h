// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback.h"

class PrefService;

namespace base {
class Time;
}  // namespace base

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

enum class DefaultStatusAPIResult;
enum class DefaultStatusRetention;

// Do not call these functions; they are exposed for unit testing only.
namespace internal {

// Returns true if the current client is too recent (based on time since
// completing the first run experience) to be assigned to a cohort.
bool IsNewClient();

// Returns true if the current client is already part of a cohort.
bool HasCohortAssigned(PrefService* local_state);

// Returns a random cohort number between 1 and the maximum number of cohorts.
int GenerateRandomCohort();

// Returns a base::Time representing the first day of the next reporting window
// for the given cohort number. By default, if the current date is already
// within the specified cohort's reporting window, then the first day of the
// current month is returned. If `after_current_month` is set to true, the
// computation to find the next reporting window starts at the beginning of next
// month. All time computations are in UTC.
base::Time GetCohortNextStartDate(int cohort_number,
                                  bool after_current_month = false);

// If eligible (correct system API availability, minimum client age met, no
// cohort already assigned), randomly assigns the client to a reporting cohort
// and sets the default status API call's next retry date to that cohort's next
// reporting window.
void AssignNewCohortIfNeeded(PrefService* local_state);

// Converts the system's default status enum value to the one used by this
// helper.

DefaultStatusAPIResult SystemToLocalEnum(
    UIApplicationCategoryDefaultStatus system_enum) API_AVAILABLE(ios(18.4));

// Returns the retention type based on the previous and current default status
// result.
DefaultStatusRetention DetermineRetentionStatus(DefaultStatusAPIResult previous,
                                                DefaultStatusAPIResult current);

// Initiates the system API call if this client is outside the cooldown period,
// and logs all relevant metrics based on the result.
void QueryDefaultStatusIfReadyAndLogResults(PrefService* local_state);

// Use in tests only. The specified callback will be invoked instead of making a
// real call to the default status API.

void OverrideSystemCallForTesting(
    base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)> cb)
    API_AVAILABLE(ios(18.4));

}  // namespace internal

// Queries the system API to determine whether this is the default browser, and
// logs the result. This is NOT meant to be used by feature code, as the system
// call is rate limited. Instead, use the helpers in default browser utils to
// check whether this is the likely the default browser.
void TriggerDefaultStatusCheck();

}  // namespace default_status

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_STATUS_DEFAULT_STATUS_HELPER_H_
