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
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/mini_map/mini_map_api.h"
#import "ios/web/public/web_state.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

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

- (void)showMapWithIPH:(BOOL)showIPH {
  _showingMap = YES;
  if (!self.consentViewController) {
    [self doShowMapWithIPH:showIPH];
    return;
  }
  __weak __typeof(self) weakSelf = self;
  [self dismissConsentInterstitialWithCompletion:^{
    [weakSelf doShowMapWithIPH:showIPH];
  }];
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
    [self workflowEnded];
  }
}

- (void)userPressedContentSettings {
  [self.mediator userOpenedSettingsInConsent];
  id<ApplicationSettingsCommands> settingsCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationSettingsCommands);
  [settingsCommandHandler
      showContentsSettingsFromViewController:self.consentViewController];
}

#pragma mark - Private methods

- (void)doShowMapWithIPH:(BOOL)showIPH {
  __weak __typeof(self) weakSelf = self;
  self.miniMapController =
      ios::provider::CreateMiniMapController(self.text, ^(NSURL* url) {
        [weakSelf mapDismissedRequestingURL:url];
      });
  [self.miniMapController
      configureFooterWithText:l10n_util::GetNSString(
                                  IDS_IOS_MINI_MAP_FOOTER_STRING)
      leadingButtonText:l10n_util::GetNSString(IDS_IOS_CONTENT_SETTINGS_TITLE)
      trailingButtonText:l10n_util::GetNSString(IDS_IOS_OPTIONS_REPORT_AN_ISSUE)
      leadingButtonAction:^{
        [weakSelf showContentSettingsFromMiniMap];
      }
      trailingButtonAction:^{
        [weakSelf reportAnIssueFromMiniMap];
      }];

  if (showIPH) {
    NSString* iphTitle = l10n_util::GetNSString(IDS_IOS_MINI_MAP_IPH_TITLE);
    NSString* iphSubtitle =
        l10n_util::GetNSString(IDS_IOS_MINI_MAP_IPH_SUBTITLE);

    NSDictionary* linkAttributes = @{
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
      NSLinkAttributeName : net::NSURLWithGURL(GURL("chrome://settings")),
    };
    NSAttributedString* attrSubtitle =
        AttributedStringFromStringWithLink(iphSubtitle, nil, linkAttributes);

    [self.miniMapController configureIPHWithTitle:[[NSAttributedString alloc]
                                                      initWithString:iphTitle]
                                         subtitle:attrSubtitle
                                    actionHandler:^(NSURL*) {
                                      [weakSelf showContentSettingsFromMiniMap];
                                    }];
  }
  if (self.mode == MiniMapMode::kDirections) {
    [self.miniMapController
        presentDirectionsWithPresentingViewController:self.baseViewController];
  } else {
    [self.miniMapController
        presentMapsWithPresentingViewController:self.baseViewController];
  }
}

// Called at the end of the minimap workflow.
- (void)workflowEnded {
  if (!_stopCalled) {
    id<MiniMapCommands> miniMapHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), MiniMapCommands);
    [miniMapHandler hideMiniMap];
  }
}

- (void)showContentSettingsFromMiniMap {
  [self.mediator userOpenedSettingsFromMiniMap];
  // The command is comming from the minimap which is currently presented.
  // The contents settings screen must be presented on top of it.
  UIViewController* presentingViewController = self.baseViewController;
  while (presentingViewController.presentedViewController) {
    presentingViewController = presentingViewController.presentedViewController;
  }
  id<ApplicationSettingsCommands> settingsCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationSettingsCommands);
  [settingsCommandHandler
      showContentsSettingsFromViewController:presentingViewController];
}

- (void)reportAnIssueFromMiniMap {
  [self.mediator userReportedAnIssueFromMiniMap];
  // The command is comming from the minimap which is currently presented.
  // The contents settings screen must be presented on top of it.
  UIViewController* presentingViewController = self.baseViewController;
  while (presentingViewController.presentedViewController) {
    presentingViewController = presentingViewController.presentedViewController;
  }
  id<ApplicationCommands> applicationCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationCommandHandler
      showReportAnIssueFromViewController:presentingViewController
                                   sender:UserFeedbackSender::MiniMap];
}

- (void)mapDismissedRequestingURL:(NSURL*)url {
  _showingMap = NO;
  if (url) {
    [self.mediator userOpenedURLFromMiniMap];
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithURLFromChrome:net::GURLWithNSURL(url)
                     inIncognito:self.browser->GetBrowserState()
                                     ->IsOffTheRecord()];
    id<ApplicationCommands> applicationHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationHandler openURLInNewTab:command];
  } else {
    [self.mediator userClosedMiniMap];
  }
  [self workflowEnded];
}

@end
