// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_NTP_MODEL_FEATURES_H_

#import "base/feature_list.h"

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

// Name of the param that indicates which variation of the kSetUpListInFirstRun
// is enabled. The Set Up List items shown depend on the variation.
extern const char kSetUpListInFirstRunParam[];

// Returns which variation of the kSetUpListInFirstRun feature is enabled.
// Returns 0 if the feature is disabled.
FirstRunVariationType GetSetUpListInFirstRunVariation();

}  // namespace set_up_list

#endif  // IOS_CHROME_BROWSER_NTP_MODEL_FEATURES_H_
