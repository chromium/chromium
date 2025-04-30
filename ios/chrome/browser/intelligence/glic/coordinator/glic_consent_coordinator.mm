// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/glic/coordinator/glic_consent_coordinator.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/intelligence/glic/coordinator/glic_consent_mediator.h"
#import "ios/chrome/browser/intelligence/glic/ui/glic_consent_view_controller.h"

@interface GlicConsentCoordinator () <UISheetPresentationControllerDelegate>

@end

@implementation GlicConsentCoordinator {
  GlicConsentMediator* _mediator;
  GlicConsentViewController* _viewController;
}

#pragma mark - ChromeCoordinator

// Starts the coordinator.
- (void)start {
  _mediator = [[GlicConsentMediator alloc] init];
  _viewController = [[GlicConsentViewController alloc] init];

  _viewController.sheetPresentationController.delegate = self;

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

// Handles the dismiss the UI.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self stop];
}

@end
