// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/coordinator/first_run_post_action_provider.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/safari_data_import/model/features.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider+protected.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation FirstRunPostActionProvider {
  BOOL _guidedTourStarted;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  NSMutableArray<NSNumber*>* screens = [NSMutableArray array];
  // For now, skip all post-FRE screens if the interactive promo is enabled, as
  // the interactive promo makes the FRE longer.
  if (IsBestOfAppLensInteractivePromoEnabled()) {
    [screens addObject:@(kStepsCompleted)];
    return [super initWithScreens:screens];
  }

  BOOL promoTriggered =
      IsAppStoreInAppEventsEnabled() && prefService &&
      prefService->GetBoolean(prefs::kAppStoreGeminiPromoTriggered);
  if (promoTriggered) {
    prefService->SetBoolean(prefs::kAppStoreGeminiPromoTriggered, false);
    [screens addObject:@(kStepsCompleted)];
    return [super initWithScreens:screens];
  }

  if (IsSyncedSetUpEnabled()) {
    [screens addObject:@(kSyncedSetUp)];
  }
  if (IsBestOfAppGuidedTourEnabled()) {
    [screens addObject:@(kGuidedTour)];
  }
  if (ShouldShowSafariDataImportEntryPoint(prefService)) {
    [screens addObject:@(kSafariImport)];
  }
  [screens addObject:@(kStepsCompleted)];
  return [super initWithScreens:screens];
}

- (void)setGuidedTourStarted:(BOOL)started {
  _guidedTourStarted = started;
}

- (ScreenType)nextScreenType {
  // Update internal index to move to next screen.
  ScreenType next = [super nextScreenType];

  // If guided tour has started, that step is very long, so showing more steps
  // after it is overwhelming.
  while (_guidedTourStarted && next != kStepsCompleted) {
    next = [super nextScreenType];
  }

  return next;
}

@end
