// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container/browser_container_coordinator.h"

#import <Availability.h>

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/link_to_text/link_to_text_payload.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/screen_time/screen_time_buildflags.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_mediator.h"
#import "ios/chrome/browser/ui/browser_container/browser_container_view_controller.h"
#import "ios/chrome/browser/ui/browser_container/browser_edit_menu_handler.h"
#import "ios/chrome/browser/ui/browser_container/edit_menu_alert_delegate.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/share_highlight_command.h"
#import "ios/chrome/browser/ui/link_to_text/link_to_text_consumer.h"
#import "ios/chrome/browser/ui/link_to_text/link_to_text_mediator.h"
#import "ios/chrome/browser/ui/overlays/overlay_container_coordinator.h"
#import "ios/chrome/browser/ui/partial_translate/partial_translate_mediator.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"
#import "url/gurl.h"

#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
#import "ios/chrome/browser/screen_time/features.h"
#import "ios/chrome/browser/ui/screen_time/screen_time_coordinator.h"
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserContainerCoordinator () <LinkToTextConsumer,
                                           EditMenuAlertDelegate>
// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// Redefine property as readwrite.
@property(nonatomic, strong, readwrite)
    BrowserContainerViewController* viewController;
// The mediator used to configure the BrowserContainerConsumer.
@property(nonatomic, strong) BrowserContainerMediator* mediator;
// The mediator used for the Link to Text feature.
@property(nonatomic, strong) LinkToTextMediator* linkToTextMediator;
// The mediator used for the Partial Translate feature.
@property(nonatomic, strong) PartialTranslateMediator* partialTranslateMediator;
// The handler for the edit menu.
@property(nonatomic, strong) BrowserEditMenuHandler* browserEditMenuHandler;
// The overlay container coordinator for OverlayModality::kWebContentArea.
@property(nonatomic, strong)
    OverlayContainerCoordinator* webContentAreaOverlayContainerCoordinator;
// The coodinator that manages ScreenTime.
@property(nonatomic, strong) ChromeCoordinator* screenTimeCoordinator;
// The handler for activity services commands.
@property(nonatomic, weak) id<ActivityServiceCommands> activityServiceHandler;
// Coordinator used to present alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
@end

@implementation BrowserContainerCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;
  self.started = YES;
  DCHECK(self.browser);
  DCHECK(!_viewController);
  Browser* browser = self.browser;
  WebStateList* webStateList = browser->GetWebStateList();
  ChromeBrowserState* browserState = browser->GetBrowserState();
  BOOL incognito = browserState->IsOffTheRecord();
  self.viewController = [[BrowserContainerViewController alloc] init];
  self.webContentAreaOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:browser
                            modality:OverlayModality::kWebContentArea];

  self.linkToTextMediator =
      [[LinkToTextMediator alloc] initWithWebStateList:webStateList
                                              consumer:self];

  self.browserEditMenuHandler = [[BrowserEditMenuHandler alloc] init];
  self.viewController.browserEditMenuHandler = self.browserEditMenuHandler;
  self.browserEditMenuHandler.linkToTextDelegate = self.linkToTextMediator;

  if (base::FeatureList::IsEnabled(kIOSEditMenuPartialTranslate)) {
    PrefService* prefService =
        browserState->GetOriginalChromeBrowserState()->GetPrefs();

    self.partialTranslateMediator = [[PartialTranslateMediator alloc]
          initWithWebStateList:webStateList->AsWeakPtr()
        withBaseViewController:self.viewController
                   prefService:prefService
                     incognito:incognito];
    self.partialTranslateMediator.alertDelegate = self;
    CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
    id<BrowserCoordinatorCommands> handler =
        HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
    self.partialTranslateMediator.browserHandler = handler;
    self.browserEditMenuHandler.partialTranslateDelegate =
        self.partialTranslateMediator;
  }

  [self.webContentAreaOverlayContainerCoordinator start];
  self.viewController.webContentsOverlayContainerViewController =
      self.webContentAreaOverlayContainerCoordinator.viewController;
  OverlayPresenter* overlayPresenter =
      OverlayPresenter::FromBrowser(browser, OverlayModality::kWebContentArea);
  self.mediator =
      [[BrowserContainerMediator alloc] initWithWebStateList:webStateList
                              webContentAreaOverlayPresenter:overlayPresenter];
  self.activityServiceHandler = HandlerForProtocol(
      browser->GetCommandDispatcher(), ActivityServiceCommands);

  self.mediator.consumer = self.viewController;

  [self setUpScreenTimeIfEnabled];

  [super start];
}

- (void)stop {
  if (!self.started)
    return;
  self.started = NO;
  [self.webContentAreaOverlayContainerCoordinator stop];
  [self.screenTimeCoordinator stop];
  [self.partialTranslateMediator shutdown];
  self.viewController = nil;
  self.mediator = nil;
  self.linkToTextMediator = nil;
  [super stop];
}

#pragma mark - LinkToTextConsumer

- (void)generatedPayload:(LinkToTextPayload*)payload {
  DCHECK(payload);
  ShareHighlightCommand* command =
      [[ShareHighlightCommand alloc] initWithURL:payload.URL
                                           title:payload.title
                                    selectedText:payload.selectedText
                                      sourceView:payload.sourceView
                                      sourceRect:payload.sourceRect];
  [self.activityServiceHandler shareHighlight:command];
}

- (void)linkGenerationFailed {
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:l10n_util::GetNSString(
                                     IDS_IOS_LINK_TO_TEXT_ERROR_TITLE)
                         message:l10n_util::GetNSString(
                                     IDS_IOS_LINK_TO_TEXT_ERROR_DESCRIPTION)];
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_APP_OK)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "SharedHighlights.LinkGenerated.Error.OK"));
                }
                 style:UIAlertActionStyleCancel];

  __weak __typeof(self) weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_SHARE_PAGE_BUTTON_LABEL)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "SharedHighlights.LinkGenerated.Error.SharePage"));
                  [weakSelf.activityServiceHandler sharePage];
                }
                 style:UIAlertActionStyleDefault];
  [self.alertCoordinator start];
}

#pragma mark - EditMenuAlertDelegate

- (void)showAlertWithTitle:(NSString*)title
                   message:(NSString*)message
                   actions:(NSArray<EditMenuAlertDelegateAction*>*)actions {
  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:title
                                                   message:message];
  for (EditMenuAlertDelegateAction* action in actions) {
    [self.alertCoordinator addItemWithTitle:action.title
                                     action:action.action
                                      style:action.style];
  }
  [self.alertCoordinator start];
}

#pragma mark - Private methods

// Sets up the ScreenTime coordinator, which installs and manages the ScreenTime
// blocking view.
- (void)setUpScreenTimeIfEnabled {
#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
  if (!IsScreenTimeIntegrationEnabled())
    return;

  ScreenTimeCoordinator* screenTimeCoordinator = [[ScreenTimeCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [screenTimeCoordinator start];
  self.viewController.screenTimeViewController =
      screenTimeCoordinator.viewController;
  self.screenTimeCoordinator = screenTimeCoordinator;

#endif
}

@end
