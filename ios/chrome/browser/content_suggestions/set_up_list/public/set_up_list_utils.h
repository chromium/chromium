// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SET_UP_LIST_PUBLIC_SET_UP_LIST_UTILS_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SET_UP_LIST_PUBLIC_SET_UP_LIST_UTILS_H_

#import "base/time/time.h"

class PrefService;

namespace set_up_list_utils {

// Returns `true` if the SetUpList should be active based on how long it has
// been since the user finished the FRE, and whether it has been disabled via
// a local state pref. The check excludes the disabled pref if
// `include_disable_pref` is false.
// The `local_prefs` indicate whether this device is eligible to show the set up
// list, while the `user_prefs` indicates the module's selected visibility.
// TODO(crbug.com/350990359): Update comment when Home Customization launches.
bool IsSetUpListActive(PrefService* local_prefs,
                       PrefService* user_prefs = nullptr,
                       bool include_disable_pref = true);

// true if the Set Up List should be shown in a compacted layout in the Magic
// Stack.
bool ShouldShowCompactedSetUpListModule();

// Duration of the Set Up List past the first run.
// So if the function returns 1 day, that means the Set Up List will appear one
// day past the First Run.
base::TimeDelta SetUpListDurationPastFirstRun();

}  // namespace set_up_list_utils

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_SET_UP_LIST_PUBLIC_SET_UP_LIST_UTILS_H_
