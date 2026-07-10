// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_coordinator.h"

#import <AVFoundation/AVFoundation.h>

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_first_run_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_first_run_wrapper_view_controller.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

@interface GeminiFirstRunCoordinator () <GeminiFirstRunMediatorDelegate,
                                         UISheetPresentationControllerDelegate>

@end

@implementation GeminiFirstRunCoordinator {
  // Mediator for handling all logic related to Gemini first run promo.
  GeminiFirstRunMediator* _mediator;

  // Wrapper view controller for the First Run Experience UI.
  GeminiFirstRunWrapperViewController* _viewController;

  // Handler for sending Gemini commands.
  id<GeminiCommands> _geminiHandler;

  // The `gemini::EntryPoint` the coordinator was initialized from.
  gemini::EntryPoint _entryPoint;

  // Type of Gemini First Run.
  GeminiFirstRunType _firstRunType;

  // The completion block passed from Mediator when consent UI is dismissed.
  void (^_consentCompletion)(void);

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
                              firstRunType:(GeminiFirstRunType)firstRunType
                         completionHandler:(void (^)(BOOL success))completion {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entryPoint = entryPoint;
    _firstRunType = firstRunType;
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
  _geminiHandler = HandlerForProtocol(dispatcher, GeminiCommands);
  _helpCommandsHandler = HandlerForProtocol(dispatcher, HelpCommands);

  _mediator = [[GeminiFirstRunMediator alloc]
        initWithPrefService:_prefService
               webStateList:self.browser->GetWebStateList()
         baseViewController:self.baseViewController
              geminiService:GeminiServiceFactory::GetForProfile(self.profile)
      authenticationService:AuthenticationServiceFactory::GetForProfile(
                                self.profile)
            identityManager:IdentityManagerFactory::GetForProfile(self.profile)
                    tracker:_tracker
                 entryPoint:_entryPoint
          completionHandler:_completion];
  _mediator.sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);

  _mediator.delegate = self;

  [self prepareAIHubIPH];

  GeminiConsentConfiguration* consentConfig =
      [_mediator consentConfigurationForFirstRunType:_firstRunType];
  BOOL showPromo =
      _mediator.shouldShowPromo && (_firstRunType != GeminiFirstRunType::kLive);
  _viewController =
      [[GeminiFirstRunWrapperViewController alloc] initWithPromo:showPromo
                                                    firstRunType:_firstRunType
                                            consentConfiguration:consentConfig];
  _viewController.sheetPresentationController.delegate = self;
  _viewController.mutator = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:^{
                                        // Record the First Run was shown.
                                        RecordFirstRunShown();
                                      }];

  [super start];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

#pragma mark - Public

- (void)stopWithCompletion:(ProceduralBlock)completion {
  GeminiTabHelper* geminiTabHelper = [self activeWebStateGeminiTabHelper];
  if (geminiTabHelper) {
    geminiTabHelper->SetPreventContextualPanelEntryPoint(NO);
  }

  [self presentPageActionMenuIPH];
  _viewController = nil;
  _geminiHandler = nil;
  _helpCommandsHandler = nil;
  [_mediator disconnect];
  _mediator = nil;
  _prefService = nil;
  _tracker = nil;
  _completion = nil;
  if (!_consentCompletion) {
    [self dismissPresentedViewWithCompletion:completion];
  }
  [super stop];
}

#pragma mark - GeminiFirstRunMediatorDelegate

- (void)dismissGeminiConsentUIWithCompletion:(void (^)())completion {
  if (_firstRunType == GeminiFirstRunType::kLive) {
    if (completion) {
      _consentCompletion = completion;
    }
    [self handleLiveMicPermission];
    return;
  }

  [self dismissPresentedViewWithCompletion:^{
    if (completion) {
      completion();
    }
  }];
  _viewController = nil;
}

- (void)dismissGeminiFlow {
  [_geminiHandler dismissGeminiFlowWithCompletion:nil];
}

#pragma mark - UISheetPresentationControllerDelegate

// Handles the dismissal of the UI.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [_geminiHandler dismissGeminiFlowWithCompletion:nil];
}

#pragma mark - Private

// Checks the current microphone permission status and prompts the user if
// needed.
- (void)handleLiveMicPermission {
  CHECK(_firstRunType == GeminiFirstRunType::kLive);
  GeminiBrowserAgent* geminiBrowserAgent =
      GeminiBrowserAgent::FromBrowser(self.browser);
  if (!geminiBrowserAgent) {
    [self handleLiveMicPermissionResult:NO];
    return;
  }
  __weak __typeof(self) weakSelf = self;
  geminiBrowserAgent->ShowGeminiLiveMicrophoneAlert(
      _viewController ? _viewController : self.baseViewController,
      ^(BOOL granted) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
          [strongSelf handleLiveMicPermissionResult:granted];
        }
      });
}

// Handles the result of the microphone permission request.
- (void)handleLiveMicPermissionResult:(BOOL)granted {
  if (granted) {
    // TODO(crbug.com/462400054): Start the Live session.
    __weak __typeof(self) weakSelf = self;
    [self dismissPresentedViewWithCompletion:^{
      __strong __typeof(weakSelf) strongSelf = weakSelf;
      if (!strongSelf) {
        return;
      }
      if (strongSelf->_consentCompletion) {
        strongSelf->_consentCompletion();
      }
      strongSelf->_consentCompletion = nil;
    }];
    _viewController = nil;
  } else {
    _consentCompletion = nil;
    [_mediator didRefuseLiveMicPermission];
  }
}

// Dismisses presented view.
- (void)dismissPresentedViewWithCompletion:(void (^)())completion {
  if (self.baseViewController.presentedViewController) {
    [self.baseViewController dismissViewControllerAnimated:YES
                                                completion:completion];
  }
}

// Returns the currently active WebState's Gemini tab helper.
- (GeminiTabHelper*)activeWebStateGeminiTabHelper {
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!activeWebState) {
    return nil;
  }

  return GeminiTabHelper::FromWebState(activeWebState);
}

// Attempts to present the entry point IPH if the user hasn't used the AI Hub
// entry point yet.
- (void)presentPageActionMenuIPH {
  if (IsChromeNextIaEnabled()) {
    return;
  }

  if (_entryPoint == gemini::EntryPoint::ExternalAppStoreEvent) {
    [_helpCommandsHandler presentInProductHelpWithType:
                              InProductHelpType::kGeminiExternalAppStoreEvent];
  } else if (_entryPoint != gemini::EntryPoint::AIHub) {
    [_helpCommandsHandler
        presentInProductHelpWithType:InProductHelpType::kPageActionMenu];
  }
}

// Prepares UI for AI Hub In-Product Help (IPH) bubble.
- (void)prepareAIHubIPH {
  if (IsChromeNextIaEnabled()) {
    return;
  }

  if (_mediator.shouldShowAIHubIPH) {
    // Ensures toolbar is expanded. If the toolbar is not fully expanded, the AI
    // Hub In-Product Help (IPH) bubble will be misaligned from using anchor
    // points relative to a partially expanded toolbar.
    FullscreenController::FromBrowser(self.browser)->ExitFullscreen();
  }
}

@end
