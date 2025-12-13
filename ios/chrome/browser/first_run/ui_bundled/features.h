// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FEATURES_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FEATURES_H_

#import "base/feature_list.h"

class ProfileIOS;

namespace first_run {

// Defines the different experiment arms for the Animated Default Browser Promo
// in the FRE experiment.
enum class AnimatedDefaultBrowserPromoInFREExperimentType {
  // The experiment arm that displays the title, subtitle, animation, and the
  // action buttons.
  kAnimationWithActionButtons = 0,

  // The experiment arm that displays the title, subtitle, animation, and the
  // action buttons - including the show me how button.
  kAnimationWithShowMeHow = 1,

  // The experiment arm that displays the title, subtitle, animation,
  // instruction view, and the action buttons.
  kAnimationWithInstructions = 2,
};

// Enum to represent variations of feature kBestFeaturesScreenInFirstRun.
enum class BestFeaturesScreenVariationType {
  kDisabled,
  // Show general screen to all users, after the Default Browser promo.
  kGeneralScreenAfterDBPromo,
  // Show general screen to all users, before the Default Browser promo.
  kGeneralScreenBeforeDBPromo,
  // Show a modified version of the general screen to all users, after the
  // Default Browser promo. The "incognito tabs" item is replaced with
  // "passwords" item.
  kGeneralScreenWithPasswordItemAfterDBPromo,
  // For "Shopping" classified users, show the Shopping-specific screen. For all
  // other users, show screen in kGeneralScreenWithPasswordItem arm. Appears
  // after Default Browser promo.
  kShoppingUsersWithFallbackAfterDBPromo,
  // For signed-in users, show the "signed-in" specific screen. For all other
  // users, don't show screen. Appears after Default Browser promo.
  kSignedInUsersOnlyAfterDBPromo,
  // Show the address bar promo instead of the Best Features screen.
  kAddressBarPromoInsteadOfBestFeaturesScreen,
};

// Enum to represent arms of feature kUpdatedFirstRunSequence.
enum class UpdatedFRESequenceVariationType {
  kDisabled,
  kDBPromoFirst,
  kRemoveSignInSync,
  kDBPromoFirstAndRemoveSignInSync,
};

// Flag to enable the FRE Default Browser Experiment.
BASE_DECLARE_FEATURE(kAnimatedDefaultBrowserPromoInFRE);

// Feature flag to enable the presentation of the "Best Features" screen in the
// FRE sequence.
BASE_DECLARE_FEATURE(kBestFeaturesScreenInFirstRun);

// Flag to enable manual metrics log uploads in the FRE screens.
BASE_DECLARE_FEATURE(kManualLogUploadsInTheFRE);

// Flag to skip the Default Browser Promo from the FRE in the EU/EEA.
BASE_DECLARE_FEATURE(kSkipDefaultBrowserPromoInFirstRun);

// Feature to enable updates to the sequence of the first run screens.
BASE_DECLARE_FEATURE(kUpdatedFirstRunSequence);

// Name of the parameter that controls the experiment type for the Animated
// Default Browser Promo in the FRE experiment, which determines the layout of
// the promo.
extern const char kAnimatedDefaultBrowserPromoInFREExperimentType[];

// Name of the parameter that indicates which variation of the
//  kBestFeaturesScreenInFirstRun feature will be displayed.
extern const char kBestFeaturesScreenInFirstRunParam[];

// Name of the param that indicates which variation of the
// kUpdatedFirstRunSequence is enabled.
extern const char kUpdatedFirstRunSequenceParam[];

// Returns which variation of the kBestFeaturesScreenInFirstRun feature is
// enabled or `kDisabled` if the feature is disabled.
BestFeaturesScreenVariationType GetBestFeaturesScreenVariationType();

// Returns which variation of the kUpdatedFirstRunSequence feature is enabled or
// `kDisabled` if the feature is disabled. This feature is disabled for profiles
// that can be eligible to search engine choice screens.
UpdatedFRESequenceVariationType GetUpdatedFRESequenceVariation(
    ProfileIOS* profile);

// Whether the Default Browser Experiment in the FRE is enabled. This feature is
// disabled when kUpdatedFirstRunSequence is enabled.
bool IsAnimatedDefaultBrowserPromoInFREEnabled();

// Returns the experimental arm for the Animated DBP in FRE experiment, which
// determines the layout of the Animated DBP.
AnimatedDefaultBrowserPromoInFREExperimentType
AnimatedDefaultBrowserPromoInFREExperimentTypeEnabled();

}  // namespace first_run

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_FEATURES_H_
