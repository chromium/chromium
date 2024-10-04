// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"

#import "base/apple/foundation_util.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_mediator.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller_delegate.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_actions_handler.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "services/metrics/public/cpp/ukm_builders.h"

@interface AdaptiveToolbarCoordinator () <AdaptiveToolbarViewControllerDelegate>

// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
// Mediator for updating the toolbar when the WebState changes.
@property(nonatomic, strong) AdaptiveToolbarMediator* mediator;
// Actions handler for the toolbar buttons.
@property(nonatomic, strong) ToolbarButtonActionsHandler* actionHandler;

@end

@implementation AdaptiveToolbarCoordinator {
  // Observer that updates `toolbarViewController` for fullscreen events.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
}

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  if (_started) {
    return;
  }
  Browser* browser = self.browser;

  _started = YES;

  self.viewController.overrideUserInterfaceStyle =
      browser->GetProfile()->IsOffTheRecord() ? UIUserInterfaceStyleDark
                                              : UIUserInterfaceStyleUnspecified;
  self.viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(browser);
  self.viewController.adaptiveDelegate = self;

  self.mediator = [[AdaptiveToolbarMediator alloc] init];
  self.mediator.incognito = browser->GetProfile()->IsOffTheRecord();
  self.mediator.consumer = self.viewController;
  self.mediator.navigationBrowserAgent =
      WebNavigationBrowserAgent::FromBrowser(browser);
  self.mediator.webStateList = browser->GetWebStateList();
  self.mediator.webContentAreaOverlayPresenter =
      OverlayPresenter::FromBrowser(browser, OverlayModality::kWebContentArea);
  self.mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(browser->GetProfile());
  self.mediator.actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:browser
             scenario:kMenuScenarioHistogramToolbarMenu];

  _fullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      FullscreenController::FromBrowser(browser), self.viewController);

  self.viewController.menuProvider = self.mediator;
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
  self.mediator = nil;
  _fullscreenUIUpdater = nullptr;
  _started = NO;
}

#pragma mark - Public

- (void)setLocationBarViewController:
    (UIViewController*)locationBarViewController {
  self.viewController.locationBarViewController = locationBarViewController;
}

- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState {
  BOOL isNonIncognitoNTP = !self.browser->GetProfile()->IsOffTheRecord() &&
                           IsVisibleURLNewTabPage(webState);

  [self.mediator updateConsumerForWebState:webState];
  [self.viewController updateForSideSwipeSnapshot:isNonIncognitoNTP];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [self.mediator updateConsumerForWebState:self.browser->GetWebStateList()
                                               ->GetActiveWebState()];
  [self.viewController resetAfterSideSwipeSnapshot];
}

- (void)showPrerenderingAnimation {
  [self.viewController showPrerenderingAnimation];
}

#pragma mark - AdaptiveToolbarViewControllerDelegate

- (void)exitFullscreen {
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(self.browser);
  fullscreenController->ExitFullscreen();

  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  ukm::SourceId sourceID = ukm::GetSourceIdForWebStateDocument(webState);
  if (sourceID != ukm::kInvalidSourceId) {
    ukm::builders::IOS_FullscreenActions(sourceID)
        .SetHasExitedManually(true)
        .Record(ukm::UkmRecorder::Get());
  }
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  [self.viewController setScrollProgressForTabletOmnibox:progress];
}

- (UIResponder<UITextInput>*)fakeboxScribbleForwardingTarget {
  // Implemented in `ToolbarCoordinator`.
  return nil;
}

- (void)didNavigateToNTPOnActiveWebState {
  // Implemented in `ToolbarCoordinator`.
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  // Implemented in primary and secondary toolbars directly.
}

#pragma mark - ToolbarCoordinatee

- (id<PopupMenuUIUpdating>)popupMenuUIUpdater {
  return self.viewController;
}

#pragma mark - Protected

- (ToolbarButtonFactory*)buttonFactoryWithType:(ToolbarType)type {
  BOOL isIncognito = self.browser->GetProfile()->IsOffTheRecord();
  ToolbarStyle style =
      isIncognito ? ToolbarStyle::kIncognito : ToolbarStyle::kNormal;

  ToolbarButtonActionsHandler* actionHandler =
      [[ToolbarButtonActionsHandler alloc] init];

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();

  actionHandler.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  actionHandler.activityHandler =
      HandlerForProtocol(dispatcher, ActivityServiceCommands);
  actionHandler.menuHandler = HandlerForProtocol(dispatcher, PopupMenuCommands);
  actionHandler.omniboxHandler =
      HandlerForProtocol(dispatcher, OmniboxCommands);

  actionHandler.incognito = isIncognito;
  actionHandler.navigationAgent =
      WebNavigationBrowserAgent::FromBrowser(self.browser);
  actionHandler.tabBasedIPHAgent =
      TabBasedIPHBrowserAgent::FromBrowser(self.browser);

  self.actionHandler = actionHandler;

  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];
  buttonFactory.actionHandler = actionHandler;
  buttonFactory.visibilityConfiguration =
      [[ToolbarButtonVisibilityConfiguration alloc] initWithType:type];

  return buttonFactory;
}

@end
