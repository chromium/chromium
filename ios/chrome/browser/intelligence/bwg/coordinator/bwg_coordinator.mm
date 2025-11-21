// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_coordinator.h"

#import "base/barrier_closure.h"
#import "base/functional/bind.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/bwg_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_fre_wrapper_view_controller.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
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
                              BWGMediatorDelegate>

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

  // The feature engagement tracker.
  raw_ptr<feature_engagement::Tracker> _tracker;
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
  __weak BWGCoordinator* weakSelf = self;
  [self dismissBWGFromOtherWindowsWithCompletion:^() {
    [weakSelf startCoordinator];
  }];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

#pragma mark - Public

- (void)stopWithCompletion:(ProceduralBlock)completion {
  BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];
  if (BWGTabHelper) {
    BWGTabHelper->SetBwgUiShowing(false);
    BWGTabHelper->SetPreventContextualPanelEntryPoint(NO);
  }
  ios::provider::ResetGemini();
  [self presentPageActionMenuIPH];
  _FREWrapperViewController = nil;
  _BWGCommandsHandler = nil;
  _helpCommandsHandler = nil;
  _mediator = nil;
  _prefService = nil;
  _tracker = nil;
  [self dismissPresentedViewWithCompletion:completion];
  [super stop];
}

#pragma mark - BWGMediatorDelegate

- (BOOL)maybePresentBWGFRE {
  if (_entryPoint != bwg::EntryPoint::Promo) {
    _tracker->NotifyEvent(
        feature_engagement::events::kIOSGeminiFlowStartedNonPromo);
  }

  // TODO(crbug.com/414768296): Move business logic to the mediator.
  BOOL showConsent = [self shouldShowBWGConsent];
  if (!showConsent) {
    return NO;
  }

  BOOL showPromo = [self shouldShowBWGPromo];
  BwgTabHelper* BWGTabHelper = [self activeWebStateBWGTabHelper];

  if (showPromo) {
    if (IsGeminiNavigationPromoEnabled() &&
        _entryPoint == bwg::EntryPoint::Promo) {
      _tracker->NotifyEvent(
          feature_engagement::events::kIOSFullscreenPromosGroupTrigger);
      _tracker->NotifyEvent(
          feature_engagement::events::kIOSGeminiFullscreenPromoTriggered);
    }
    int impressionCount =
        _prefService->GetInteger(prefs::kIOSBWGPromoImpressionCount) + 1;
    _prefService->SetInteger(prefs::kIOSBWGPromoImpressionCount,
                             impressionCount);

    if (impressionCount == 1) {
      _tracker->NotifyEvent(
          feature_engagement::events::kIOSGeminiPromoFirstCompletion);
      if (BWGTabHelper) {
        BWGTabHelper->SetPreventContextualPanelEntryPoint(
            [self shouldShowAIHubIPH]);
      }
    }
  }

  _FREWrapperViewController = [[BWGFREWrapperViewController alloc]
         initWithPromo:showPromo
      isAccountManaged:[self isManagedAccount]];
  _FREWrapperViewController.sheetPresentationController.delegate = self;
  _FREWrapperViewController.mutator = _mediator;

  BOOL shouldAnimatePresentation =
      BWGTabHelper ? !BWGTabHelper->GetIsBwgSessionActiveInBackground() : YES;

  [self.baseViewController presentViewController:_FREWrapperViewController
                                        animated:shouldAnimatePresentation
                                      completion:^{
                                        // Record FRE was shown.
                                        RecordFREShown();
                                      }];

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
  [_BWGCommandsHandler dismissBWGFlowWithCompletion:nil];
}

#pragma mark - Private

// Starts the BWG coordinator.
- (void)startCoordinator {
  _prefService = self.profile->GetPrefs();
  CHECK(_prefService);

  _tracker = feature_engagement::TrackerFactory::GetForProfile(self.profile);
  CHECK(_tracker);

  BOOL willShowFRE = [self shouldShowBWGConsent];
  // Record entry point with FRE context.
  RecordBWGEntryPointClick(_entryPoint, willShowFRE);

  if (_entryPoint == bwg::EntryPoint::AIHub) {
    _tracker->NotifyEvent(
        feature_engagement::events::kIOSPageActionMenuIPHUsed);
  }

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _BWGCommandsHandler = HandlerForProtocol(dispatcher, BWGCommands);
  _helpCommandsHandler = HandlerForProtocol(dispatcher, HelpCommands);

  // TODO(crbug.com/455906590): Pipe the image to the Gemini overlay.
  _mediator = [[BWGMediator alloc]
      initWithPrefService:_prefService
             webStateList:self.browser->GetWebStateList()
       baseViewController:self.baseViewController
               BWGService:BwgServiceFactory::GetForProfile(self.profile)
          BWGBrowserAgent:BwgBrowserAgent::FromBrowser(self.browser)
                  tracker:_tracker];
  _mediator.applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  _mediator.delegate = self;

  [self prepareAIHubIPH];
  [_mediator presentBWGFlow];

  [super start];
}

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

// Dismisses BWG from all other windows and executes the completion block.
- (void)dismissBWGFromOtherWindowsWithCompletion:(ProceduralBlock)completion {
  base::OnceCallback closure = base::BindOnce(completion);

  // Collect all browsers (excluding the current one) for all profiles.
  std::vector<base::WeakPtr<Browser>> otherBrowsers;
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    const std::set<Browser*>& browserList =
        BrowserListFactory::GetForProfile(profile)->BrowsersOfType(
            BrowserList::BrowserType::kRegular);
    for (Browser* browser : browserList) {
      if (browser == self.browser) {
        continue;
      }
      otherBrowsers.push_back(browser->AsWeakPtr());
    }
  }

  if (otherBrowsers.empty()) {
    std::move(closure).Run();
    return;
  }

  // Gate the completion behind this barrier closure which executes it when all
  // other browsers have dismissed their BWG sessions.
  base::RepeatingClosure barrier =
      base::BarrierClosure(otherBrowsers.size(), std::move(closure));

  // Dismiss BWG in all the other browsers for all profiles.
  for (base::WeakPtr<Browser> browser : otherBrowsers) {
    id<BWGCommands> BWGCommandsHandler =
        HandlerForProtocol(browser->GetCommandDispatcher(), BWGCommands);
    [BWGCommandsHandler dismissBWGFlowWithCompletion:^() {
      barrier.Run();
    }];
  }
}

// Prepares UI for AI Hub In-Product Help (IPH) bubble.
- (void)prepareAIHubIPH {
  if ([self shouldShowAIHubIPH]) {
    // Ensures toolbar is expanded. If the toolbar is not fully expanded, the AI
    // Hub In-Product Help (IPH) bubble will be misaligned from using anchor
    // points relative to a partially expanded toolbar.
    FullscreenController::FromBrowser(self.browser)->ExitFullscreen();
  }
}

// Returns whether to show AI Hub IPH.
- (BOOL)shouldShowAIHubIPH {
  BOOL wouldTriggerIPH =
      _tracker->WouldTriggerHelpUI(feature_engagement::kIPHIOSPageActionMenu);

  return _entryPoint != bwg::EntryPoint::AIHub && [self shouldShowBWGPromo] &&
         wouldTriggerIPH;
}

@end
