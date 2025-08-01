// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_fre_wrapper_view_controller.h"
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
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/public/web_state.h"

namespace {

// The max number of times the promo page should be shown.
const CGFloat kPromoMaxImpressionCount = 3;

}  // namespace

@interface BWGCoordinator () <UISheetPresentationControllerDelegate,
                              BWGMediatorDelegate,
                              BWGFREWrapperViewControllerDelegate>

@end

@implementation BWGCoordinator {
  // Mediator for handling all logic related to BWG.
  BWGMediator* _mediator;

  // Wrapper view controller for the First Run Experience (FRE) UI.
  BWGFREWrapperViewController* _FREWrapperViewController;

  // Handler for sending BWG commands.
  id<BWGCommands> _BWGCommandsHandler;

  // Returns the `_entryPoint` the coordinator was intialized from.
  bwg::EntryPoint _entryPoint;

  // Handler for sending IPH commands.
  id<HelpCommands> _helpCommandsHandler;

  // Pref service.
  raw_ptr<PrefService> _prefService;
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

  if (_entryPoint == bwg::EntryPoint::AIHub) {
    feature_engagement::TrackerFactory::GetForProfile(self.profile)
        ->NotifyEvent(feature_engagement::events::kIOSPageActionMenuIPHUsed);
  }

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
  BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];
  if (BWGTabHelper) {
    BWGTabHelper->SetBwgUiShowing(false);
  }
  ios::provider::ResetGemini();
  [self presentPageActionMenuIPH];
  _FREWrapperViewController = nil;
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
  BOOL showConsent = [self shouldShowBWGConsent];
  if (!showConsent) {
    // Record the entry point metrics for the non-FRE case.
    base::UmaHistogramEnumeration(kEntryPointHistogram, _entryPoint);

    return NO;
  }

  BOOL showPromo = [self shouldShowBWGPromo];

  base::UmaHistogramEnumeration(kFREEntryPointHistogram, _entryPoint);

  if (showPromo) {
    _prefService->SetInteger(
        prefs::kIOSBWGPromoImpressionCount,
        _prefService->GetInteger(prefs::kIOSBWGPromoImpressionCount) + 1);
  }

  _FREWrapperViewController = [[BWGFREWrapperViewController alloc]
         initWithPromo:showPromo
      isAccountManaged:[self isManagedAccount]];
  _FREWrapperViewController.sheetPresentationController.delegate = self;
  _FREWrapperViewController.BWGFREWrapperViewControllerDelegate = self;
  _FREWrapperViewController.mutator = _mediator;

  BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];
  BOOL shouldAnimatePresentation =
      BWGTabHelper ? !BWGTabHelper->GetIsBwgSessionActiveInBackground() : YES;

  [self.baseViewController presentViewController:_FREWrapperViewController
                                        animated:shouldAnimatePresentation
                                      completion:nil];

  if (BWGTabHelper) {
    BWGTabHelper->SetBwgUiShowing(true);
  }

  return YES;
}

- (void)dismissBWGConsentUIWithCompletion:(void (^)())completion {
  [self dismissPresentedViewWithCompletion:completion];
  _FREWrapperViewController = nil;
}

- (BOOL)shouldShowBWGConsent {
  return !_prefService->GetBoolean(prefs::kIOSBwgConsent);
}

- (void)dismissBWGFlow {
  [_BWGCommandsHandler dismissBWGFlowWithCompletion:nil];
}

#pragma mark - UISheetPresentationControllerDelegate

// Handles the dismissal of the UI.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // TODO(crbug.com/419064727): Add metric for dismissing coordinator.
  [_BWGCommandsHandler dismissBWGFlowWithCompletion:nil];
}

#pragma mark - BWGFREWrapperViewControllerDelegate

- (void)promoWasDismissed:(BWGFREWrapperViewController*)wrapperViewController {
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
  BOOL promoImpressionsExhausted =
      _prefService->GetInteger(prefs::kIOSBWGPromoImpressionCount) >=
      kPromoMaxImpressionCount;

  return ShouldForceBWGPromo() ||
         ([self shouldShowBWGConsent] && !promoImpressionsExhausted);
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

// Attemps to present the entry point IPH the user hasn't used the AI Hub entry
// point yet.
- (void)presentPageActionMenuIPH {
  if (_entryPoint != bwg::EntryPoint::AIHub) {
    [_helpCommandsHandler
        presentInProductHelpWithType:InProductHelpType::kPageActionMenu];
  }
}

@end
