// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/backend_promo/model/backend_promo_profile_agent.h"

#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/backend_promo/model/backend_promo_service.h"
#import "ios/chrome/browser/backend_promo/model/backend_promo_service_factory.h"

@implementation BackendPromoProfileAgent

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kFinal) {
    return;
  }

  // Initialize the BackendPromoService.
  BackendPromoServiceFactory::GetForProfile(self.profileState.profile);

  [profileState removeObserver:self];
  [profileState removeAgent:self];
}

@end
