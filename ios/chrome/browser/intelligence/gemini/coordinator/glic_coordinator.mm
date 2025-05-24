// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_coordinator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_mediator.h"
#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/gemini/ui/glic_navigation_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/glic_commands.h"

@interface GLICCoordinator () <UISheetPresentationControllerDelegate,
                               GLICMediatorDelegate>

@end

@implementation GLICCoordinator {
  // Mediator for handling all logic related to GLIC.
  GLICMediator* _mediator;

  // Navigation view controller owning the promo and the consent UI.
  GLICNavigationController* _navigationController;

  // Handler for sending GLIC commands.
  id<GlicCommands> _handler;
}

#pragma mark - ChromeCoordinator

- (void)start {
  PrefService* pref_service = self.profile->GetPrefs();
  CHECK(pref_service);

  _handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), GlicCommands);

  _mediator =
      [[GLICMediator alloc] initWithPrefService:pref_service
                                        browser:self.browser
                             baseViewController:self.baseViewController];
  _mediator.delegate = self;

  [_mediator presentGlicFlow];

  [super start];
}

- (void)stop {
  [super stop];
}

#pragma mark - GLICConsentMediatorDelegate

- (void)presentGlicFRE {
  _navigationController = [[GLICNavigationController alloc] init];
  _navigationController.sheetPresentationController.delegate = self;
  _navigationController.mutator = _mediator;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - UISheetPresentationControllerDelegate

// Handles the dismissal of the UI.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // TODO(crbug.com/419064727): Add metric for dismissing coordinator.
  [_handler dismissGlicFlow];
}

#pragma mark - GLICConsentMediatorDelegate

// Dismisses the UI by stopping the coordinator.
- (void)dismissGLICConsentUI {
  [_handler dismissGlicFlow];
}

#pragma mark - Private

// Decides whether GLIC consent should be shown.
- (BOOL)shouldShowGLICConsent {
  PrefService* prefService = self.profile->GetPrefs();
  CHECK(prefService);
  return !prefService->GetBoolean(prefs::kIOSGLICConsent);
}

@end
