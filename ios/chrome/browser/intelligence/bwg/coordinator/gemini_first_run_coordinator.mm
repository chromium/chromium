// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_coordinator.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_fre_wrapper_view_controller.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/public/web_state.h"

@interface GeminiFirstRunCoordinator () <UISheetPresentationControllerDelegate,
                                         GeminiFirstRunMediatorDelegate>

@end

@implementation GeminiFirstRunCoordinator {
  // Mediator for handling all logic related to Gemini first run promo.
  GeminiFirstRunMediator* _mediator;

  // Wrapper view controller for the First Run Experience (FRE) UI.
  BWGFREWrapperViewController* _viewController;

  // Handler for sending Gemini commands.
  id<BWGCommands> _geminiCommandsHandler;

  // Returns the `_entryPoint` the coordinator was initialized from.
  gemini::EntryPoint _entryPoint;

  // Handler for sending IPH commands.
  id<HelpCommands> _helpCommandsHandler;

  // Pref service.
  raw_ptr<PrefService> _prefService;

  // The feature engagement tracker.
  raw_ptr<feature_engagement::Tracker> _tracker;

  // Completion block to be called when the FRE flow finishes.
  void (^_completion)(BOOL success);
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            fromEntryPoint:(gemini::EntryPoint)entryPoint
                         completionHandler:(void (^)(BOOL success))completion {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entryPoint = entryPoint;
    _completion = completion;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _prefService = self.profile->GetPrefs();
  CHECK(_prefService);

  _tracker = feature_engagement::TrackerFactory::GetForProfile(self.profile);
  CHECK(_tracker);

  if (_entryPoint == gemini::EntryPoint::AIHub) {
    _tracker->NotifyEvent(
        feature_engagement::events::kIOSPageActionMenuIPHUsed);
  }

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _geminiCommandsHandler = HandlerForProtocol(dispatcher, BWGCommands);
  _helpCommandsHandler = HandlerForProtocol(dispatcher, HelpCommands);

  _mediator = [[GeminiFirstRunMediator alloc]
      initWithPrefService:_prefService
             webStateList:self.browser->GetWebStateList()
       baseViewController:self.baseViewController
               BWGService:BwgServiceFactory::GetForProfile(self.profile)
       geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(self.browser)
                  tracker:_tracker
               entryPoint:_entryPoint
        completionHandler:_completion];
  _mediator.sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);

  _mediator.delegate = self;

  [self prepareAIHubIPH];

  _viewController = [[BWGFREWrapperViewController alloc]
         initWithPromo:_mediator.shouldShowPromo
      isAccountManaged:[self isManagedAccount]];
  _viewController.sheetPresentationController.delegate = self;
  _viewController.mutator = _mediator;

  BwgTabHelper* BWGTabHelper = [self activeWebStateGeminiTabHelper];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:^{
                                        // Record FRE was shown.
                                        RecordFREShown();
                                      }];

  if (BWGTabHelper) {
    BWGTabHelper->SetBwgUiShowing(true);
  }

  [super start];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

#pragma mark - Public

- (void)stopWithCompletion:(ProceduralBlock)completion {
  BwgTabHelper* BWGTabHelper = [self activeWebStateGeminiTabHelper];
  if (BWGTabHelper) {
    BWGTabHelper->SetBwgUiShowing(false);
    BWGTabHelper->SetPreventContextualPanelEntryPoint(NO);
  }

  [self presentPageActionMenuIPH];
  _viewController = nil;
  _geminiCommandsHandler = nil;
  _helpCommandsHandler = nil;
  _mediator = nil;
  _prefService = nil;
  _tracker = nil;
  _completion = nil;
  [self dismissPresentedViewWithCompletion:completion];
  [super stop];
}

#pragma mark - GeminiMediatorDelegate

- (void)dismissGeminiConsentUIWithCompletion:(void (^)())completion {
  [self dismissPresentedViewWithCompletion:completion];
  _viewController = nil;
}

- (void)dismissGeminiFlow {
  [_geminiCommandsHandler dismissGeminiFlowWithCompletion:nil];
}

#pragma mark - UISheetPresentationControllerDelegate

// Handles the dismissal of the UI.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [_geminiCommandsHandler dismissGeminiFlowWithCompletion:nil];
}

#pragma mark - Private

// Dismisses presented view.
- (void)dismissPresentedViewWithCompletion:(void (^)())completion {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES
                                                completion:completion];
  }
}

// Returns YES if the account is managed.
- (BOOL)isManagedAccount {
  raw_ptr<AuthenticationService> authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  return authService->HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin);
}

// Returns the currently active WebState's Gemini tab helper.
- (BwgTabHelper*)activeWebStateGeminiTabHelper {
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
  if (_entryPoint != gemini::EntryPoint::AIHub) {
    [_helpCommandsHandler
        presentInProductHelpWithType:InProductHelpType::kPageActionMenu];
  }
}

// Prepares UI for AI Hub In-Product Help (IPH) bubble.
- (void)prepareAIHubIPH {
  if (_mediator.shouldShowAIHubIPH) {
    // Ensures toolbar is expanded. If the toolbar is not fully expanded, the AI
    // Hub In-Product Help (IPH) bubble will be misaligned from using anchor
    // points relative to a partially expanded toolbar.
    FullscreenController::FromBrowser(self.browser)->ExitFullscreen();
  }
}

@end
