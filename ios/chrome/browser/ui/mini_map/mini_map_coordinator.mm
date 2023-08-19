// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/mini_map/mini_map_coordinator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/mini_map/mini_map_action_handler.h"
#import "ios/chrome/browser/ui/mini_map/mini_map_interstitial_view_controller.h"
#import "ios/chrome/browser/ui/mini_map/mini_map_mediator.h"
#import "ios/chrome/browser/ui/mini_map/mini_map_mediator_delegate.h"
#import "ios/chrome/browser/web/annotations/annotations_util.h"
#import "ios/public/provider/chrome/browser/mini_map/mini_map_api.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"

@interface MiniMapCoordinator () <MiniMapActionHandler, MiniMapMediatorDelegate>

// The view controller to get user consent.
@property(nonatomic, strong)
    MiniMapInterstitialViewController* consentViewController;

// The controller that shows the mini map.
@property(nonatomic, strong) id<MiniMapController> miniMapController;

// Mediator to handle the consent logic.
@property(nonatomic, strong) MiniMapMediator* mediator;

// The WebState that triggered the request.
@property(assign) base::WeakPtr<web::WebState> webState;

// The text to be recognized as an address.
@property(nonatomic, copy) NSString* text;

// Whether the consent of the user is required.
@property(assign) BOOL consentRequired;

// The mode to display the map.
@property(assign) MiniMapMode mode;

@end

@implementation MiniMapCoordinator {
  BOOL _stopCalled;

  BOOL _showingMap;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
                                      text:(NSString*)text
                           consentRequired:(BOOL)consentRequired
                                      mode:(MiniMapMode)mode {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _text = text;
    if (webState) {
      _webState = webState->GetWeakPtr();
    }
    _consentRequired = consentRequired;
    _mode = mode;
  }
  return self;
}

- (void)start {
  [super start];

  PrefService* prefService = self.browser->GetBrowserState()->GetPrefs();
  self.mediator = [[MiniMapMediator alloc] initWithPrefs:prefService
                                                webState:self.webState.get()];
  self.mediator.delegate = self;
  [self.mediator userInitiatedMiniMapConsentRequired:self.consentRequired];
}

- (void)stop {
  _stopCalled = YES;
  [self.mediator disconnect];
  [self dismissConsentInterstitialWithCompletion:nil];
  [super stop];
}

#pragma mark - MiniMapMediatorDelegate

- (void)showConsentInterstitial {
  self.consentViewController = [[MiniMapInterstitialViewController alloc] init];
  self.consentViewController.actionHandler = self;
  self.consentViewController.modalPresentationStyle =
      UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.consentViewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents =
      @[ UISheetPresentationControllerDetent.largeDetent ];
  [self.baseViewController presentViewController:self.consentViewController
                                        animated:YES
                                      completion:nil];
}

- (void)dismissConsentInterstitialWithCompletion:(ProceduralBlock)completion {
  [self.consentViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completion];
  self.consentViewController = nil;
}

- (void)showMap {
  _showingMap = YES;
  if (!self.consentViewController) {
    [self actualShowMap];
    return;
  }
  __weak __typeof(self) weakSelf = self;
  [self dismissConsentInterstitialWithCompletion:^{
    [weakSelf actualShowMap];
  }];
}

- (void)mapDismissedRequestingURL:(NSURL*)url {
  _showingMap = NO;
  if (url) {
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithURLFromChrome:net::GURLWithNSURL(url)
                     inIncognito:self.browser->GetBrowserState()
                                     ->IsOffTheRecord()];
    id<ApplicationCommands> applicationHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationHandler openURLInNewTab:command];
  }
  [self workFlowEnded];
}

- (void)actualShowMap {
  __weak __typeof(self) weakSelf = self;
  self.miniMapController =
      ios::provider::CreateMiniMapController(self.text, ^(NSURL* url) {
        [weakSelf mapDismissedRequestingURL:url];
      });
  if (self.mode == MiniMapMode::kDirections) {
    [self.miniMapController
        presentDirectionsWithPresentingViewController:self.baseViewController];
  } else {
    [self.miniMapController
        presentMapsWithPresentingViewController:self.baseViewController];
  }
}

- (void)workFlowEnded {
  if (!_stopCalled) {
    id<MiniMapCommands> miniMapHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), MiniMapCommands);
    [miniMapHandler hideMiniMap];
  }
}

#pragma mark - MiniMapActionHandler

- (void)userPressedConsent {
  [self.mediator userConsented];
}

- (void)userPressedNoThanks {
  [self.mediator userDeclined];
  [self workflowEnded];
}

- (void)dismissed {
  if (!_showingMap) {
    [self.mediator userDismissed];
    [self workFlowEnded];
  }
}

- (void)userPressedContentSettings {
  [self.mediator userOpenedSettings];
  id<ApplicationSettingsCommands> settings_command_handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationSettingsCommands);
  [settings_command_handler
      showContentsSettingsFromViewController:self.consentViewController];
}

#pragma mark - Private methods

// Called at the end of the minimap workflow.
- (void)workflowEnded {
  if (!_stopCalled) {
    id<MiniMapCommands> miniMapHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), MiniMapCommands);
    [miniMapHandler hideMiniMap];
  }
}

@end
