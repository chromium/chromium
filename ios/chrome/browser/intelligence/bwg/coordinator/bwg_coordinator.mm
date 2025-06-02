// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_coordinator.h"

#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_navigation_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

@interface BWGCoordinator () <UISheetPresentationControllerDelegate,
                              BWGMediatorDelegate,
                              BWGNavigationControllerDelegate>

@end

@implementation BWGCoordinator {
  // Mediator for handling all logic related to BWG.
  BWGMediator* _mediator;

  // Navigation view controller owning the promo and the consent UI.
  BWGNavigationController* _navigationController;

  // Handler for sending BWG commands.
  id<BWGCommands> _handler;

  // Returns the `_entryPoint` the coordinator was intialized from.
  bwg::EntryPoint _entryPoint;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            fromEntryPoint:(bwg::EntryPoint)entryPoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entryPoint = entryPoint;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  PrefService* pref_service = self.profile->GetPrefs();
  CHECK(pref_service);

  // If the user sees the GLIC feature, we can consider this feature as
  // discovered. Therefore, we can mark the GLIC feature as used.
  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);
  if (engagementTracker) {
    engagementTracker->NotifyUsedEvent(
        feature_engagement::kIPHIOSBWGPromoFeature);
  }

  _handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), BWGCommands);

  _mediator = [[BWGMediator alloc] initWithPrefService:pref_service
                                               browser:self.browser
                                    baseViewController:self.baseViewController];
  _mediator.delegate = self;

  [_mediator presentBWGFlow];

  [super start];
}

- (void)stop {
  [super stop];
}

#pragma mark - BWGMediatorDelegate

- (BOOL)maybePresentBWGFRE {
  BOOL showPromo = _entryPoint == bwg::EntryPointPromo;
  BOOL showConsent = [self shouldShowBWGConsent];

  if (!showPromo && !showConsent) {
    return NO;
  }

  _navigationController =
      [[BWGNavigationController alloc] initWithPromo:showPromo];
  _navigationController.sheetPresentationController.delegate = self;
  _navigationController.BWGNavigationDelegate = self;
  _navigationController.mutator = _mediator;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
  return YES;
}

#pragma mark - UISheetPresentationControllerDelegate

// Handles the dismissal of the UI.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // TODO(crbug.com/419064727): Add metric for dismissing coordinator.
  [_handler dismissBWGFlow];
}

#pragma mark - BWGConsentMediatorDelegate

// Dismisses the UI by stopping the coordinator.
- (void)dismissBWGConsentUI {
  [_handler dismissBWGFlow];
}

- (BOOL)shouldShowBWGConsent {
  PrefService* prefService = self.profile->GetPrefs();
  CHECK(prefService);
  return !prefService->GetBoolean(prefs::kIOSBwgConsent);
}

#pragma mark - BWGNavigationControllerDelegate

- (void)promoWasDismissed:(BWGNavigationController*)navigationController {
  if (_entryPoint == bwg::EntryPointPromo) {
    [self.promosUIHandler promoWasDismissed];
  }
}

@end
