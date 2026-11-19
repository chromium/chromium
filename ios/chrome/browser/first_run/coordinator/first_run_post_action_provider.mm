// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_post_action_provider.h"

#import <utility>
#import <vector>

#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/safari_data_import/model/features.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider+protected.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation FirstRunPostActionProvider {
  BOOL _guidedTourStarted;
}

- (instancetype)initWithProfilePrefs:(PrefService*)profilePrefs
                          localState:(PrefService*)localState {
  // For now, skip all post-FRE screens if the interactive promo is enabled, as
  // the interactive promo makes the FRE longer.
  if (IsBestOfAppLensInteractivePromoEnabled()) {
    return [super initWithScreens:{ScreenType::kStepsCompleted}];
  }

  BOOL promoTriggered =
      profilePrefs &&
      profilePrefs->GetBoolean(prefs::kAppStoreGeminiPromoTriggered);
  if (promoTriggered) {
    profilePrefs->SetBoolean(prefs::kAppStoreGeminiPromoTriggered, false);
    return [super initWithScreens:{ScreenType::kStepsCompleted}];
  }

  if (localState &&
      !localState->GetList(prefs::kFirstRunDeferredScreens).empty() &&
      first_run::IsFREAblationEnabled() &&
      first_run::GetFREAblationVariationType() ==
          first_run::FREAblationVariationType::kSkipAllExceptSignInAndChoice) {
    return [super initWithScreens:{ScreenType::kStepsCompleted}];
  }

  std::vector<ScreenType> screens;
  screens.push_back(ScreenType::kSyncedSetUp);

  if (IsBestOfAppGuidedTourEnabled()) {
    screens.push_back(ScreenType::kGuidedTour);
  }
  if (ShouldShowSafariDataImportEntryPoint(profilePrefs)) {
    screens.push_back(ScreenType::kSafariImport);
  }
  screens.push_back(ScreenType::kStepsCompleted);
  return [super initWithScreens:std::move(screens)];
}

- (void)setGuidedTourStarted:(BOOL)started {
  _guidedTourStarted = started;
}

- (ScreenType)nextScreenType {
  // Update internal index to move to next screen.
  ScreenType next = [super nextScreenType];

  // If guided tour has started, that step is very long, so showing more steps
  // after it is overwhelming.
  while (_guidedTourStarted && next != ScreenType::kStepsCompleted) {
    next = [super nextScreenType];
  }

  return next;
}

@end
