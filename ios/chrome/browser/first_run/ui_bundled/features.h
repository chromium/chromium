// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FEATURES_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FEATURES_H_

#import "base/feature_list.h"

class ProfileIOS;

namespace first_run {

// Enum to represent arms of feature kUpdatedFirstRunSequence.
enum class UpdatedFRESequenceVariationType {
  kDisabled,
  kDBPromoFirst,
  kRemoveSignInSync,
  kDBPromoFirstAndRemoveSignInSync,
};

// Feature to enable updates to the sequence of the first run screens.
BASE_DECLARE_FEATURE(kUpdatedFirstRunSequence);

// Name of the param that indicates which variation of the
// kUpdatedFirstRunSequence is enabled.
extern const char kUpdatedFirstRunSequenceParam[];

// Returns which variation of the kUpdatedFirstRunSequence feature is enabled or
// `kDisabled` if the feature is disabled. This feature is disabled for EEA
// countries.
UpdatedFRESequenceVariationType GetUpdatedFRESequenceVariation(
    ProfileIOS* profile);

// Flag to enable the FRE Default Browser Experiment.
BASE_DECLARE_FEATURE(kAnimatedDefaultBrowserPromoInFRE);

// Whether the Default Browser Experiment in the FRE is enabled. This feature is
// disabled when kUpdatedFirstRunSequence is enabled.
bool IsAnimatedDefaultBrowserPromoInFREEnabled();

}  // namespace first_run

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FEATURES_H_
