// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_navigation_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

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
  id<BWGCommands> _BWGCommandsHandler;

  // Returns the `_entryPoint` the coordinator was intialized from.
  bwg::EntryPoint _entryPoint;

  // Handler for sending IPH commands.
  id<HelpCommands> _helpCommandsHandler;

  // Pref service.
  raw_ptr<PrefService> _prefService;

  // FET(Feature engagement tracker) for promo updates.
  raw_ptr<feature_engagement::Tracker> _tracker;

  // Promo was shown.
  BOOL _wasPromoShown;
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
  _prefService = self.profile->GetPrefs();
  CHECK(_prefService);

  _tracker = feature_engagement::TrackerFactory::GetForProfile(self.profile);

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _BWGCommandsHandler = HandlerForProtocol(dispatcher, BWGCommands);
  _helpCommandsHandler = HandlerForProtocol(dispatcher, HelpCommands);

  _mediator = [[BWGMediator alloc] initWithPrefService:_prefService
                                               browser:self.browser
                                    baseViewController:self.baseViewController];
  _mediator.delegate = self;
  [_mediator presentBWGFlow];

  [super start];
}

- (void)stop {
  _navigationController = nil;
  _BWGCommandsHandler = nil;
  _helpCommandsHandler = nil;
  _mediator = nil;
  _prefService = nil;
  _tracker = nil;
  [self dismissPresentedViewWithCompletion:nil];
  [super stop];
}

#pragma mark - BWGMediatorDelegate

- (BOOL)maybePresentBWGFRE {
  // TODO(crbug.com/414768296): Move business logic to the mediator.
  BOOL showPromo = [self shouldShowBWGPromo];
  BOOL showConsent = [self shouldShowBWGConsent];

  if (!showPromo && !showConsent) {
    // Record the entry point metrics for the non-FRE case.
    base::UmaHistogramEnumeration(kEntryPointHistogram, _entryPoint);

    return NO;
  }

  base::UmaHistogramEnumeration(kFREEntryPointHistogram, _entryPoint);

  // If promo was shown outside the promos manager, ensure the promo doesn't
  // show through the promos manager.
  if (_entryPoint != bwg::EntryPointPromo) {
    _prefService->SetBoolean(prefs::kIOSBWGManualPromo, true);
    _tracker->UnregisterPriorityNotificationHandler(
        feature_engagement::kIPHIOSBWGPromoFeature);
  }

  _navigationController =
      [[BWGNavigationController alloc] initWithPromo:showPromo
                                    isAccountManaged:[self isManagedAccount]];
  _navigationController.sheetPresentationController.delegate = self;
  _navigationController.BWGNavigationDelegate = self;
  _navigationController.mutator = _mediator;

  __weak __typeof(self) weakSelf = self;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:^{
                                        BWGCoordinator* strongSelf = weakSelf;
                                        strongSelf->_wasPromoShown = showPromo;
                                      }];
  return YES;
}

- (void)dismissBWGConsentUIWithCompletion:(void (^)())completion {
  [self dismissPresentedViewWithCompletion:completion];
  _navigationController = nil;
}

- (BOOL)shouldShowBWGConsent {
  return !_prefService->GetBoolean(prefs::kIOSBwgConsent);
}

- (void)dismissBWGFlow {
  __weak __typeof(self) weakSelf = self;
  [self dismissPresentedViewWithCompletion:^{
    BWGCoordinator* strongSelf = weakSelf;
    [strongSelf presentPageActionMenuIPH];
    [strongSelf->_BWGCommandsHandler dismissBWGFlow];
  }];
}

#pragma mark - UISheetPresentationControllerDelegate

// Handles the dismissal of the UI.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // TODO(crbug.com/419064727): Add metric for dismissing coordinator.
  [_BWGCommandsHandler dismissBWGFlow];
}

#pragma mark - BWGNavigationControllerDelegate

- (void)promoWasDismissed:(BWGNavigationController*)navigationController {
  if (_entryPoint == bwg::EntryPointPromo) {
    [self.promosUIHandler promoWasDismissed];
  }
}

#pragma mark - Private

// Dismisses presented view.
- (void)dismissPresentedViewWithCompletion:(void (^)())completion {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES
                                                completion:completion];
  }
}

// If YES, BWG Promo should be shown.
- (BOOL)shouldShowBWGPromo {
  BOOL promoShownManually = _prefService->GetBoolean(prefs::kIOSBWGManualPromo);
  BOOL promoTriggered = _tracker->HasEverTriggered(
      feature_engagement::kIPHIOSBWGPromoFeature, true);
  BOOL isPromoEntry = _entryPoint == bwg::EntryPointPromo;

  return isPromoEntry || (!promoTriggered && !promoShownManually);
}

// Presents the page action menu IPH.
- (void)presentPageActionMenuIPH {
  if (_wasPromoShown) {
    [_helpCommandsHandler
        presentInProductHelpWithType:InProductHelpType::kPageActionMenu];
  }
}

// Returns YES if the account is managed.
- (BOOL)isManagedAccount {
  raw_ptr<AuthenticationService> authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  return authService->HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin);
}

@end
