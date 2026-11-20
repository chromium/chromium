// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_FEATURES_H_

#import "base/feature_list.h"

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
  kDisabled = 0,
  // Show general screen to all users, after the Default Browser promo.
  kGeneralScreenAfterDBPromo = 1,
  // Show general screen to all users, before the Default Browser promo.
  kGeneralScreenBeforeDBPromo = 2,
  // Show a modified version of the general screen to all users, after the
  // Default Browser promo. The "incognito tabs" item is replaced with
  // "passwords" item.
  kGeneralScreenWithPasswordItemAfterDBPromo = 3,
  // kShoppingUsersWithFallbackAfterDBPromo = 4,  // Obsolete.
  // For signed-in users, show the "signed-in" specific screen. For all other
  // users, don't show screen. Appears after Default Browser promo.
  kSignedInUsersOnlyAfterDBPromo = 5,
  // Show the address bar promo instead of the Best Features screen.
  kAddressBarPromoInsteadOfBestFeaturesScreen = 6,
  // Show the general screen to all users as a part of the Best of App FRE.
  kBestOfApp = 7,
};

// Enum to represent variations of feature
// `kAndroidSwitcherDefaultBrowserPromoInFRE`.
enum class AndroidSwitcherDefaultBrowserPromoInFREVariationType {
  kDisabled = 0,
  // Trigger the Android Switcher promo and record metrics using the
  // Segmentation Platform `DeviceSwitcherModel` signal.
  kSegmentationSignal = 1,
  // Trigger the Android Switcher promo and record metrics using the
  // `SwitcherInfo` provider API signal.
  kSwitcherInfoSignal = 2,
};

// Enum to represent UI copy arms of feature
// `kAndroidSwitcherDefaultBrowserPromoInFRE`.
enum class AndroidSwitcherDefaultBrowserPromoInFREArm {
  // Highlights Google ecosystem integration.
  kGoogleEcosystem = 0,
  // Combines Google ecosystem utility with Chrome branding.
  kHybrid = 1,
};

// Enum to represent variations of feature `kFREAblation`.
enum class FREAblationVariationType {
  // Skips the History Sync screen in the FRE.
  kSkipHistorySync = 0,
  // Skips all FRE screens except for the Sign-in screen and the Search Engine
  // Choice screen (if applicable).
  kSkipAllExceptSignInAndChoice = 1,
};

// Flag to enable the FRE Default Browser Experiment.
BASE_DECLARE_FEATURE(kAnimatedDefaultBrowserPromoInFRE);

// Feature flag to enable the presentation of the "Best Features" screen in the
// FRE sequence.
BASE_DECLARE_FEATURE(kBestFeaturesScreenInFirstRun);

// Feature flag (killswitch) to enable presenting post FRE IPH promos in
// FirstRunProfileAgent instead of FirstRunCoordinator.
BASE_DECLARE_FEATURE(kPostFREIphInProfileAgent);

// Feature flag to enable the Android Switcher Default Browser Promo in the FRE.
BASE_DECLARE_FEATURE(kAndroidSwitcherDefaultBrowserPromoInFRE);

// Feature flag (killswitch) to record `.SwitcherInfo.*` Default Browser
// metrics.
BASE_DECLARE_FEATURE(kRecordSwitcherInfoDefaultBrowserMetrics);

// Feature flag to enable the FRE ablation experiment.
BASE_DECLARE_FEATURE(kFREAblation);

// Name of the parameter that controls the experiment type for the Animated
// Default Browser Promo in the FRE experiment, which determines the layout of
// the promo.
extern const char kAnimatedDefaultBrowserPromoInFREExperimentType[];

// Name of the parameter that indicates which variation of the
//  kBestFeaturesScreenInFirstRun feature will be displayed.
extern const char kBestFeaturesScreenInFirstRunParam[];

// Name of the parameter that indicates which variation of the
// `kAndroidSwitcherDefaultBrowserPromoInFRE` feature is enabled.
extern const char kAndroidSwitcherDefaultBrowserPromoInFREParam[];

// Name of the parameter that indicates which variation of the `kFREAblation`
// feature is enabled.
extern const char kFREAblationParam[];

// Parameter values for `kAndroidSwitcherDefaultBrowserPromoInFREParam`.
extern const char kAndroidSwitcherDefaultBrowserPromoInFREParamDisabled[];
extern const char
    kAndroidSwitcherDefaultBrowserPromoInFREParamSegmentationSignal[];
extern const char
    kAndroidSwitcherDefaultBrowserPromoInFREParamSwitcherInfoSignal[];

// Name of the parameter that indicates which UI arm of the
// `kAndroidSwitcherDefaultBrowserPromoInFRE` feature is enabled.
extern const char kAndroidSwitcherDefaultBrowserPromoInFREArmParam[];

// Parameter values for `kAndroidSwitcherDefaultBrowserPromoInFREArmParam`.
extern const char kAndroidSwitcherDefaultBrowserPromoInFREArmGoogleEcosystem[];
extern const char kAndroidSwitcherDefaultBrowserPromoInFREArmHybrid[];

// Parameter values for `kFREAblationParam`.
extern const char kFREAblationParamSkipHistorySync[];
extern const char kFREAblationParamSkipAllExceptSignInAndChoice[];

// Returns which variation of the kBestFeaturesScreenInFirstRun feature is
// enabled or `kDisabled` if the feature is disabled.
BestFeaturesScreenVariationType GetBestFeaturesScreenVariationType();

// Whether the Default Browser Experiment in the FRE is enabled.
bool IsAnimatedDefaultBrowserPromoInFREEnabled();

// Returns the experimental arm for the Animated DBP in FRE experiment, which
// determines the layout of the Animated DBP.
AnimatedDefaultBrowserPromoInFREExperimentType
AnimatedDefaultBrowserPromoInFREExperimentTypeEnabled();

// Returns whether post FRE IPH promos should be presented in
// FirstRunProfileAgent instead of FirstRunCoordinator (killswitch).
bool IsPostFREIphInProfileAgentEnabled();

// Returns which variation of the `kAndroidSwitcherDefaultBrowserPromoInFRE`
// feature is enabled or `kDisabled` if the feature is disabled.
AndroidSwitcherDefaultBrowserPromoInFREVariationType
GetAndroidSwitcherDefaultBrowserPromoInFREVariationType();

// Returns which UI arm of the `kAndroidSwitcherDefaultBrowserPromoInFRE`
// feature is enabled.
AndroidSwitcherDefaultBrowserPromoInFREArm
GetAndroidSwitcherDefaultBrowserPromoInFREArm();

// Returns whether the `kFREAblation` feature is enabled.
bool IsFREAblationEnabled();

// Returns which variation of the `kFREAblation` feature is enabled. Must only
// be called when `IsFREAblationEnabled()` returns true.
FREAblationVariationType GetFREAblationVariationType();

}  // namespace first_run

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_FEATURES_H_
