// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_container/ui_bundled/browser_container_coordinator.h"

#import <Availability.h>

#import "base/check.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/browser_container/ui_bundled/browser_container_mediator.h"
#import "ios/chrome/browser/browser_container/ui_bundled/browser_container_view_controller.h"
#import "ios/chrome/browser/browser_container/ui_bundled/browser_edit_menu_handler.h"
#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_alert_delegate.h"
#import "ios/chrome/browser/explain_with_gemini/coordinator/explain_with_gemini_mediator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_payload.h"
#import "ios/chrome/browser/link_to_text/ui_bundled/link_to_text_mediator.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_container_coordinator.h"
#import "ios/chrome/browser/partial_translate/ui_bundled/partial_translate_mediator.h"
#import "ios/chrome/browser/screen_time/model/screen_time_buildflags.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/search_with/ui_bundled/search_with_mediator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "url/gurl.h"

#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
#import "ios/chrome/browser/screen_time/model/features.h"
#import "ios/chrome/browser/screen_time/ui_bundled/screen_time_coordinator.h"
#endif

@interface BrowserContainerCoordinator () <EditMenuAlertDelegate>

// Redefine property as readwrite.
@property(nonatomic, strong, readwrite)
    BrowserContainerViewController* viewController;

@end

@implementation BrowserContainerCoordinator {
  // Whether the coordinator is started.
  BOOL _started;
  // Coordinator used to present alerts to the user.
  AlertCoordinator* _alertCoordinator;
  // The mediator used for the Search With feature.
  SearchWithMediator* _searchWithMediator;
  // The coodinator that manages ScreenTime.
  ChromeCoordinator* _screenTimeCoordinator;
  // The overlay container coordinator for OverlayModality::kWebContentArea.
  OverlayContainerCoordinator* _webContentAreaOverlayContainerCoordinator;
  // The mediator used for the Partial Translate feature.
  PartialTranslateMediator* _partialTranslateMediator;
  // The mediator used to configure the BrowserContainerConsumer.
  BrowserContainerMediator* _mediator;
  // The mediator used for the Link to Text feature.
  LinkToTextMediator* _linkToTextMediator;
  // The mediator used for the Explain With Gemini feature.
  ExplainWithGeminiMediator* _explainWithGeminiMediator;
  // The handler for the edit menu.
  BrowserEditMenuHandler* _browserEditMenuHandler;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (_started) {
    return;
  }
  _started = YES;
  DCHECK(self.browser);
  DCHECK(!_viewController);
  Browser* browser = self.browser;
  WebStateList* webStateList = browser->GetWebStateList();
  ProfileIOS* profile = browser->GetProfile();
  BOOL incognito = profile->IsOffTheRecord();
  self.viewController = [[BrowserContainerViewController alloc] init];
  _webContentAreaOverlayContainerCoordinator =
      [[OverlayContainerCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:browser
                            modality:OverlayModality::kWebContentArea];

  _linkToTextMediator =
      [[LinkToTextMediator alloc] initWithWebStateList:webStateList];
  _linkToTextMediator.alertDelegate = self;
  _linkToTextMediator.activityServiceHandler = HandlerForProtocol(
      browser->GetCommandDispatcher(), ActivityServiceCommands);

  _browserEditMenuHandler = [[BrowserEditMenuHandler alloc] init];
  self.viewController.browserEditMenuHandler = _browserEditMenuHandler;
  _browserEditMenuHandler.linkToTextDelegate = _linkToTextMediator;
  self.viewController.linkToTextDelegate = _linkToTextMediator;

  PrefService* prefService = profile->GetOriginalProfile()->GetPrefs();
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(self.browser);

  _partialTranslateMediator = [[PartialTranslateMediator alloc]
        initWithWebStateList:webStateList
      withBaseViewController:self.viewController
                 prefService:prefService
        fullscreenController:fullscreenController
                   incognito:incognito];
  _partialTranslateMediator.alertDelegate = self;
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> browserCommandsHandler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  _partialTranslateMediator.browserHandler = browserCommandsHandler;
  _browserEditMenuHandler.partialTranslateDelegate = _partialTranslateMediator;

  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  _searchWithMediator =
      [[SearchWithMediator alloc] initWithWebStateList:webStateList
                                    templateURLService:templateURLService
                                             incognito:incognito];

  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);

  _searchWithMediator.applicationCommandHandler = applicationCommandsHandler;
  _browserEditMenuHandler.searchWithDelegate = _searchWithMediator;

  if (ExplainGeminiEditMenuPosition() !=
          PositionForExplainGeminiEditMenu::kDisabled &&
      !incognito) {
    _explainWithGeminiMediator = [[ExplainWithGeminiMediator alloc]
        initWithWebStateList:webStateList
             identityManager:IdentityManagerFactory::GetForProfile(profile)
                 authService:AuthenticationServiceFactory::GetForProfile(
                                 profile)];

    _explainWithGeminiMediator.applicationCommandHandler =
        applicationCommandsHandler;
    _browserEditMenuHandler.explainWithGeminiDelegate =
        _explainWithGeminiMediator;
  }

  [_webContentAreaOverlayContainerCoordinator start];

  self.viewController.webContentsOverlayContainerViewController =
      _webContentAreaOverlayContainerCoordinator.viewController;
  OverlayPresenter* overlayPresenter =
      OverlayPresenter::FromBrowser(browser, OverlayModality::kWebContentArea);
  _mediator =
      [[BrowserContainerMediator alloc] initWithWebStateList:webStateList
                              webContentAreaOverlayPresenter:overlayPresenter];

  _mediator.consumer = self.viewController;

  [self setUpScreenTimeIfEnabled];

  [super start];
}

- (void)stop {
  if (!_started) {
    return;
  }
  [self dismissAlertCoordinator];
  _started = NO;
  [_webContentAreaOverlayContainerCoordinator stop];
  [_screenTimeCoordinator stop];
  [_partialTranslateMediator shutdown];
  [_searchWithMediator shutdown];
  self.viewController = nil;
  _mediator = nil;
  _linkToTextMediator = nil;
  _partialTranslateMediator = nil;
  _searchWithMediator = nil;
  [super stop];
}

- (id<EditMenuBuilder>)editMenuBuilder {
  return _browserEditMenuHandler;
}

#pragma mark - EditMenuAlertDelegate

- (void)showAlertWithTitle:(NSString*)title
                   message:(NSString*)message
                   actions:(NSArray<EditMenuAlertDelegateAction*>*)actions {
  _alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self.viewController
                                                   browser:self.browser
                                                     title:title
                                                   message:message];
  __weak BrowserContainerCoordinator* weakSelf = self;
  for (EditMenuAlertDelegateAction* action in actions) {
    [_alertCoordinator addItemWithTitle:action.title
                                 action:^{
                                   action.action();
                                   [weakSelf dismissAlertCoordinator];
                                 }
                                  style:action.style
                              preferred:action.preferred
                                enabled:YES];
  }
  [_alertCoordinator start];
}

#pragma mark - Private methods

// Sets up the ScreenTime coordinator, which installs and manages the ScreenTime
// blocking view.
- (void)setUpScreenTimeIfEnabled {
#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
  if (!IsScreenTimeIntegrationEnabled()) {
    return;
  }

  ScreenTimeCoordinator* screenTimeCoordinator = [[ScreenTimeCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  [screenTimeCoordinator start];
  self.viewController.screenTimeViewController =
      screenTimeCoordinator.viewController;
  _screenTimeCoordinator = screenTimeCoordinator;

#endif
}

- (void)dismissAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

@end
