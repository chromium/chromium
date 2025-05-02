// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/glic/coordinator/glic_consent_coordinator.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/glic/coordinator/glic_consent_mediator.h"
#import "ios/chrome/browser/intelligence/glic/coordinator/glic_consent_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/glic/ui/glic_consent_view_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface GLICConsentCoordinator () <UISheetPresentationControllerDelegate,
                                      GLICConsentMediatorDelegate>

@end

@implementation GLICConsentCoordinator {
  // Mediator of GLIC consent handling logic for accepting the consent.
  GLICConsentMediator* _mediator;

  // View controller of GLIC consent presenting the UI.
  GLICConsentViewController* _viewController;
}

#pragma mark - ChromeCoordinator

// Starts the coordinator.
- (void)start {
  PrefService* pref_service = self.profile->GetPrefs();
  CHECK(pref_service);

  _mediator = [[GLICConsentMediator alloc] initWithPrefService:pref_service];
  _mediator.delegate = self;

  _viewController = [[GLICConsentViewController alloc] init];
  _viewController.sheetPresentationController.delegate = self;
  _viewController.mutator = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
  [super start];
}

// Stops the coordinator.
- (void)stop {
  if (_viewController.presentingViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  }

  _mediator = nil;
  _viewController = nil;
  [super stop];
}

#pragma mark - UISheetPresentationControllerDelegate

// Handles the dismissal of the UI.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self stop];
}

#pragma mark - GLICConsentMediatorDelegate

// Dismisses the UI by stopping the coordinator.
- (void)dismissGLICConsentUI {
  [self stop];
}

@end
