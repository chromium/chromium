// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_FEATURES_H_

#import "base/feature_list.h"
#import "base/time/time.h"

namespace set_up_list {

// Feature to adjust the Set Up List duration.
BASE_DECLARE_FEATURE(kSetUpListShortenedDuration);

// Feature to remove the sign-in item in the Set Up List.
BASE_DECLARE_FEATURE(kSetUpListWithoutSignInItem);

// Name of the param that indicates the duration of the Set Up List in days.
extern const char kSetUpListDurationParam[];

// Returns the duration for the SetUpList based off the state of the
// kSetUpListShortenedDuration feature. Returns the duration past the First Run,
// so if the function returns 1 day, that means the Set Up List will appear one
// day past the First Run.
base::TimeDelta SetUpListDurationPastFirstRun();

}  // namespace set_up_list

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_FEATURES_H_
