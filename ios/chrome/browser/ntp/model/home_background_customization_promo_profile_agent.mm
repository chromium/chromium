// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/model/home_background_customization_promo_profile_agent.h"

#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"

@implementation HomeBackgroundCustomizationPromoProfileAgent

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFinal) {
    return;
  }

  PromosManagerFactory::GetForProfile(self.profileState.profile)
      ->RegisterPromoForContinuousDisplay(
          promos_manager::Promo::HomeBackgroundCustomization);

  [profileState removeObserver:self];
  [profileState removeAgent:self];
}

@end
