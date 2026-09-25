// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_promo_profile_agent.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation LevelUpPromoProfileAgent

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFinal) {
    return;
  }
  // Registers the promo only if the user has not opted in.
  if (profileState.profile->GetPrefs()->GetBoolean(prefs::kLevelUpOptIn)) {
    return;
  }

  PromosManagerFactory::GetForProfile(profileState.profile)
      ->RegisterPromoForSingleDisplay(promos_manager::Promo::LevelUp);

  [profileState removeObserver:self];
  [profileState removeAgent:self];
}

@end
