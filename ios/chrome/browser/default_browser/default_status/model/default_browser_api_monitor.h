// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_STATUS_MODEL_DEFAULT_BROWSER_API_MONITOR_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_STATUS_MODEL_DEFAULT_BROWSER_API_MONITOR_H_

#import <UIKit/UIKit.h>

#import <optional>

#import "base/functional/callback.h"
#import "base/memory/raw_ref.h"

class PrefRegistrySimple;
class PrefService;

// Runs the periodic check that queries the default browser system API and
// reports the result to metrics. At a high level, the check works as follows:
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
class DefaultBrowserAPIMonitor {
 public:
  // `local_state` must outlive this object.
  explicit DefaultBrowserAPIMonitor(PrefService& local_state);
  DefaultBrowserAPIMonitor(const DefaultBrowserAPIMonitor&) = delete;
  DefaultBrowserAPIMonitor& operator=(const DefaultBrowserAPIMonitor&) = delete;
  ~DefaultBrowserAPIMonitor();

  // Registers the Local State prefs used by the monitor in `registry`.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Queries the system API to determine whether Chrome is the default browser
  // when the periodic check is due, and logs the result.
  void RunCohortCheckIfDue() API_AVAILABLE(ios(18.4));

  // Executes the default status check, invoking `system_call` instead of the
  // system API when the check is ready, and optionally forcing initial cohort
  // assignment to `cohort_override` when set.
  void RunCohortCheckIfDueForTesting(
      base::OnceCallback<UIApplicationCategoryDefaultStatus(NSError**)>
          system_call,
      std::optional<int> cohort_override) API_AVAILABLE(ios(18.4));

 private:
  const raw_ref<PrefService> local_state_;
};

// Queries the system API to determine whether Chrome is the default browser,
// and logs the result. This is NOT meant to be used by feature code, as the
// system call is rate limited. Instead, use the helpers in default browser
// utils to check whether Chrome is likely the default browser.
void TriggerDefaultStatusCheck();

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_DEFAULT_STATUS_MODEL_DEFAULT_BROWSER_API_MONITOR_H_
