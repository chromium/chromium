// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_navigation_controller.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/web/public/web_state.h"

namespace {

// The max number of times the promo page should be shown.
const CGFloat kPromoMaxImpressionCount = 3;

}  // namespace

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
  [self stopWithCompletion:nil];
}

#pragma mark - Public

- (void)stopWithCompletion:(ProceduralBlock)completion {
  _navigationController = nil;
  _BWGCommandsHandler = nil;
  _helpCommandsHandler = nil;
  _mediator = nil;
  _prefService = nil;
  [self dismissPresentedViewWithCompletion:completion];
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

  if (showPromo) {
    _prefService->SetInteger(
        prefs::kIOSBWGPromoImpressionCount,
        _prefService->GetInteger(prefs::kIOSBWGPromoImpressionCount) + 1);
  }

  _navigationController =
      [[BWGNavigationController alloc] initWithPromo:showPromo
                                    isAccountManaged:[self isManagedAccount]];
  _navigationController.sheetPresentationController.delegate = self;
  _navigationController.BWGNavigationDelegate = self;
  _navigationController.mutator = _mediator;

  BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];
  BOOL shouldAnimatePresentation =
      BWGTabHelper ? !BWGTabHelper->GetIsBwgSessionActiveInBackground() : NO;

  __weak __typeof(self) weakSelf = self;
  [self.baseViewController presentViewController:_navigationController
                                        animated:shouldAnimatePresentation
                                      completion:^{
                                        BWGCoordinator* strongSelf = weakSelf;
                                        strongSelf->_wasPromoShown = showPromo;
                                      }];

  if (BWGTabHelper) {
    BWGTabHelper->SetBwgUiShowing(true);
  }

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
    [strongSelf->_BWGCommandsHandler dismissBWGFlowWithCompletion:nil];
  }];

  BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];
  if (BWGTabHelper) {
    BWGTabHelper->SetBwgUiShowing(false);
  }
}

#pragma mark - UISheetPresentationControllerDelegate

// Handles the dismissal of the UI.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // TODO(crbug.com/419064727): Add metric for dismissing coordinator.
  [_BWGCommandsHandler dismissBWGFlowWithCompletion:nil];
}

#pragma mark - BWGNavigationControllerDelegate

- (void)promoWasDismissed:(BWGNavigationController*)navigationController {
  if (_entryPoint == bwg::EntryPoint::Promo) {
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
  BOOL forcePromo = ShouldForceBWGPromo();
  BOOL promoImpressionsExhausted =
      _prefService->GetInteger(prefs::kIOSBWGPromoImpressionCount) >=
      kPromoMaxImpressionCount;

  return forcePromo || !promoImpressionsExhausted;
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

// Returns the currently active WebState's BWG tab helper.
- (BwgTabHelper*)activeWebStateBWGTabHelper {
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!activeWebState) {
    return nil;
  }

  return BwgTabHelper::FromWebState(activeWebState);
}

@end
