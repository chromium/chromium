// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_FEATURES_H_

#import "base/feature_list.h"
#import "base/time/time.h"

namespace set_up_list {

// Enum to represent arms of feature kSetUpListInFirstRun.
enum class FirstRunVariationType {
  kDisabled,
  kDockingAndAddressBar,
  kDocking,
  kAddressBar,
};

// Feature to enable the Set Up List in the First Run.
BASE_DECLARE_FEATURE(kSetUpListInFirstRun);

// Feature to adjust the Set Up List duration.
BASE_DECLARE_FEATURE(kSetUpListShortenedDuration);

// Feature to remove the sign-in item in the Set Up List.
BASE_DECLARE_FEATURE(kSetUpListWithoutSignInItem);

// Name of the param that indicates which variation of the kSetUpListInFirstRun
// is enabled. The Set Up List items shown depend on the variation.
extern const char kSetUpListInFirstRunParam[];

// Name of the param that indicates the duration of the Set Up List in days.
extern const char kSetUpListDurationParam[];

// Returns which variation of the kSetUpListInFirstRun feature is enabled.
// Returns 0 if the feature is disabled.
FirstRunVariationType GetSetUpListInFirstRunVariation();

// Returns the duration for the SetUpList based off the state of the
// kSetUpListShortenedDuration feature. Returns the duration past the First Run,
// so if the function returns 1 day, that means the Set Up List will appear one
// day past the First Run.
base::TimeDelta SetUpListDurationPastFirstRun();

}  // namespace set_up_list

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_FEATURES_H_
