// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_consent_coordinator.h"

#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_consent_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_consent_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_navigation_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface BWGConsentCoordinator () <UISheetPresentationControllerDelegate,
                                     BWGConsentMediatorDelegate>

@end

@implementation BWGConsentCoordinator {
  // Mediator of BWG consent handling logic for accepting the consent.
  BWGConsentMediator* _mediator;
  // Navigation view controller owning the promo and the consent UI.
  BWGNavigationController* _navigationController;
}

#pragma mark - ChromeCoordinator

// Starts the coordinator.
- (void)start {
  PrefService* pref_service = self.profile->GetPrefs();
  CHECK(pref_service);

  _mediator = [[BWGConsentMediator alloc] initWithPrefService:pref_service];
  _mediator.delegate = self;

  _navigationController = [[BWGNavigationController alloc] init];
  _navigationController.sheetPresentationController.delegate = self;
  _navigationController.mutator = _mediator;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
  [super start];
}

// Stops the coordinator.
- (void)stop {
  if (_navigationController.presentingViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }

  _mediator = nil;
  _navigationController = nil;
  [super stop];
}

#pragma mark - UISheetPresentationControllerDelegate

// Handles the dismissal of the UI.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self stop];
}

#pragma mark - BWGConsentMediatorDelegate

// Dismisses the UI by stopping the coordinator.
- (void)dismissBWGConsentUI {
  [self stop];
}

@end
