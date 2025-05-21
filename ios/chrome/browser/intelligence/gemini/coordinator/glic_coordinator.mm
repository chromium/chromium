// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_coordinator.h"

#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_mediator.h"
#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/gemini/ui/glic_navigation_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface GLICCoordinator () <UISheetPresentationControllerDelegate,
                               GLICMediatorDelegate>

@end

@implementation GLICCoordinator {
  // Mediator for handling all logic related to GLIC.
  GLICMediator* _mediator;
  // Navigation view controller owning the promo and the consent UI.
  GLICNavigationController* _navigationController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  PrefService* pref_service = self.profile->GetPrefs();
  CHECK(pref_service);

  _mediator = [[GLICMediator alloc] initWithPrefService:pref_service];
  _mediator.delegate = self;

  _navigationController = [[GLICNavigationController alloc] init];
  _navigationController.sheetPresentationController.delegate = self;
  _navigationController.mutator = _mediator;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
  [super start];
}

- (void)stop {
  [super stop];
}

#pragma mark - GLICConsentMediatorDelegate

// Dismisses the UI by stopping the coordinator.
- (void)dismissGLICConsentUI {
  [self stop];
}

@end
