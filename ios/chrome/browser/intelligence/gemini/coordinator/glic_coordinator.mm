// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_coordinator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_mediator.h"
#import "ios/chrome/browser/intelligence/gemini/coordinator/glic_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/gemini/model/glic_service.h"
#import "ios/chrome/browser/intelligence/gemini/model/glic_service_factory.h"
#import "ios/chrome/browser/intelligence/gemini/ui/glic_navigation_controller.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
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

  _mediator = [[GLICMediator alloc] initWithPrefService:pref_service
                                                browser:self.browser];
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

#pragma mark - Private

// Decides whether GLIC consent should be shown.
- (BOOL)shouldShowGLICConsent {
  PrefService* prefService = self.profile->GetPrefs();
  CHECK(prefService);
  return !prefService->GetBoolean(prefs::kIOSGLICConsent);
}

// Opens the GLIC overlay with a given page context.
- (void)openGLICOverlayForPage:
    (std::unique_ptr<optimization_guide::proto::PageContext>)pageContext {
  GlicService* glicService = GlicServiceFactory::GetForProfile(self.profile);
  glicService->PresentOverlayOnViewController(self.baseViewController,
                                              std::move(pageContext));

  // TODO(crbug.com/419064727): Dismiss glic promo/consent.
}

@end
