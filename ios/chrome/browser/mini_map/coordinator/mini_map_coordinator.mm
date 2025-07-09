// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/coordinator/mini_map_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/mini_map/coordinator/mini_map_mediator.h"
#import "ios/chrome/browser/mini_map/coordinator/mini_map_mediator_delegate.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/web/model/annotations/annotations_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/mini_map/mini_map_api.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

@interface MiniMapCoordinator () <MiniMapMediatorDelegate>

// The controller that shows the mini map.
@property(nonatomic, strong) id<MiniMapController> miniMapController;

// Mediator to handle the consent logic.
@property(nonatomic, strong) MiniMapMediator* mediator;

// The WebState that triggered the request.
@property(assign) base::WeakPtr<web::WebState> webState;

// The mode to display the map.
@property(assign) MiniMapMode mode;

@end

@implementation MiniMapCoordinator {
  BOOL _stopCalled;
  BOOL _showingMap;

  // The text to be recognized as an address.
  NSString* _text;

  // The Universal link URL to maps to display the MiniMap for.
  NSURL* _url;

  // Whether IPH should be shown (on first presentation).
  BOOL _showIPH;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      text:(NSString*)text
                                       url:(NSURL*)URL
                                   withIPH:(BOOL)withIPH
                                      mode:(MiniMapMode)mode {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK_EQ((text ? 1 : 0) + (URL ? 1 : 0), 1);
    _text = text;
    _url = URL;
    web::WebState* currentWebState =
        browser->GetWebStateList()->GetActiveWebState();
    if (currentWebState) {
      // Keep track of the WebState so we can remove the annotations if the
      // user disable the feature.
      _webState = currentWebState->GetWeakPtr();
    }
    _showIPH = withIPH;
    _mode = mode;
  }
  return self;
}

- (void)start {
  [super start];

  PrefService* prefService = self.profile->GetPrefs();
  MiniMapQueryType type =
      _text ? MiniMapQueryType::kText : MiniMapQueryType::kURL;
  self.mediator = [[MiniMapMediator alloc] initWithPrefs:prefService
                                                    type:type
                                                webState:self.webState.get()];
  self.mediator.delegate = self;
  [self.mediator userInitiatedMiniMapWithIPH:_showIPH];
}

- (void)stop {
  _stopCalled = YES;
  [self.mediator disconnect];
  [super stop];
}

#pragma mark - MiniMapMediatorDelegate

- (void)showMapWithIPH:(BOOL)showIPH {
  _showingMap = YES;
  [self doShowMapWithIPH:showIPH];
}

#pragma mark - Private methods

- (void)doShowMapWithIPH:(BOOL)showIPH {
  __weak __typeof(self) weakSelf = self;
  MiniMapControllerCompletionWithURL completion = ^(NSURL* url) {
    [weakSelf mapDismissedRequestingURL:url];
  };
  MiniMapControllerCompletionWithString completionWithQuery =
      ^(NSString* query) {
        [weakSelf mapDismissedRequestingQuery:query];
      };
  self.miniMapController = ios::provider::CreateMiniMapController();
  if (_text) {
    [self configureForText];
  } else {
    [self configureForURL];
  }

  [self.miniMapController configureCompletion:completion];
  [self.miniMapController
      configureCompletionWithSearchQuery:completionWithQuery];

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

    [self.miniMapController
        configureDisclaimerWithTitle:[[NSAttributedString alloc]
                                         initWithString:iphTitle]
                            subtitle:attrSubtitle
                       actionHandler:^(NSURL*,
                                       UIViewController* viewController) {
                         [weakSelf
                             showContentSettingsFromMiniMapInViewController:
                                 viewController];
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

- (void)configureForText {
  __weak __typeof(self) weakSelf = self;
  [self.miniMapController configureAddress:_text];
  [self.miniMapController
      configureFooterWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_MINI_MAP_FOOTER_STRING)
      leadingButtonTitle:l10n_util::GetNSString(IDS_IOS_MINI_MAP_DISABLE_STRING)
      trailingButtonTitle:l10n_util::GetNSString(
                              IDS_IOS_OPTIONS_REPORT_AN_ISSUE)
      leadingButtonAction:^(UIViewController* viewController) {
        [weakSelf disableOneTapMinimapFromViewController:viewController];
      }
      trailingButtonAction:^(UIViewController* viewController) {
        [weakSelf reportAnIssueFromMiniMapInViewController:viewController];
      }];
}

- (void)configureForURL {
  __weak __typeof(self) weakSelf = self;
  [self.miniMapController configureURL:_url];
  [self.miniMapController
      configureFooterWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_MINI_MAP_FOOTER_STRING)
      leadingButtonTitle:l10n_util::GetNSString(
                             IDS_IOS_MINI_MAP_DISABLE_PREVIEW_STRING)
      trailingButtonTitle:l10n_util::GetNSString(
                              IDS_IOS_OPTIONS_REPORT_AN_ISSUE)
      leadingButtonAction:^(UIViewController* viewController) {
        [weakSelf disableURLHandlingFromViewContrller:viewController];
      }
      trailingButtonAction:^(UIViewController* viewController) {
        [weakSelf reportAnIssueFromMiniMapInViewController:viewController];
      }];
}

// Called at the end of the minimap workflow.
- (void)workflowEnded {
  if (!_stopCalled) {
    id<MiniMapCommands> miniMapHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), MiniMapCommands);
    [miniMapHandler hideMiniMap];
  }
}

- (void)showContentSettingsFromMiniMapInViewController:
    (UIViewController*)viewController {
  [self.mediator userOpenedSettingsFromMiniMap];
  id<SettingsCommands> settingsCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);
  [settingsCommandHandler
      showContentsSettingsFromViewController:viewController];
}

- (void)disableOneTapMinimapFromViewController:
    (UIViewController*)viewController {
  [self.mediator userDisabledOneTapSettingFromMiniMap];

  [viewController.presentingViewController dismissViewControllerAnimated:YES
                                                              completion:nil];
  id<SnackbarCommands> snackbarCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  __weak __typeof(self) weakSelf = self;
  [snackbarCommandHandler
      showSnackbarWithMessage:l10n_util::GetNSString(
                                  IDS_IOS_MINI_MAP_DISABLE_CONFIRMATION_STRING)
      buttonText:l10n_util::GetNSString(
                     IDS_IOS_MINI_MAP_DISABLE_CONFIRMATION_BUTTON_STRING)
      messageAction:^{
        [weakSelf userOpenedSettingsFromConfirmation];
      }
      completionAction:^(BOOL) {
        [weakSelf workflowEnded];
      }];
}

- (void)disableURLHandlingFromViewContrller:(UIViewController*)viewController {
  [self.mediator userDisabledURLSettingFromMiniMap];

  [viewController.presentingViewController dismissViewControllerAnimated:YES
                                                              completion:nil];
  id<SnackbarCommands> snackbarCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);
  __weak __typeof(self) weakSelf = self;
  [snackbarCommandHandler
      showSnackbarWithMessage:
          l10n_util::GetNSString(
              IDS_IOS_MINI_MAP_DISABLE_PREVIEW_CONFIRMATION_STRING)
      buttonText:l10n_util::GetNSString(
                     IDS_IOS_MINI_MAP_DISABLE_CONFIRMATION_BUTTON_STRING)
      messageAction:^{
        [weakSelf userOpenedSettingsFromConfirmation];
      }
      completionAction:^(BOOL) {
        [weakSelf workflowEnded];
      }];
}

- (void)userOpenedSettingsFromConfirmation {
  [self.mediator userOpenedSettingsFromDisableConfirmation];
  id<SettingsCommands> settingsCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);
  [settingsCommandHandler
      showContentsSettingsFromViewController:self.baseViewController];
}

- (void)reportAnIssueFromMiniMapInViewController:
    (UIViewController*)viewController {
  [self.mediator userReportedAnIssueFromMiniMap];
  id<ApplicationCommands> applicationCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationCommandHandler
      showReportAnIssueFromViewController:viewController
                                   sender:UserFeedbackSender::MiniMap];
}

- (void)mapDismissedRequestingURL:(NSURL*)url {
  _showingMap = NO;
  if (url) {
    [self.mediator userOpenedURLFromMiniMap];
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:net::GURLWithNSURL(url)
                                        inIncognito:self.isOffTheRecord];
    id<ApplicationCommands> applicationHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationHandler openURLInNewTab:command];
  } else {
    [self.mediator userClosedMiniMap];
  }
  [self workflowEnded];
}

- (void)mapDismissedRequestingQuery:(NSString*)query {
  _showingMap = NO;
  if (query) {
    [self.mediator userOpenedQueryFromMiniMap];
    TemplateURLService* templateURLService =
        ios::TemplateURLServiceFactory::GetForProfile(self.profile);
    GURL url = templateURLService->GenerateSearchURLForDefaultSearchProvider(
        base::SysNSStringToUTF16(query));

    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:url
                                        inIncognito:self.isOffTheRecord];
    id<ApplicationCommands> applicationHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationHandler openURLInNewTab:command];
  } else {
    [self.mediator userClosedMiniMap];
  }
  [self workflowEnded];
}

@end
