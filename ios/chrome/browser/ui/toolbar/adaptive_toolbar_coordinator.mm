// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"

#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/ntp/new_tab_page_util.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
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
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
      browser->GetBrowserState()->IsOffTheRecord()
          ? UIUserInterfaceStyleDark
          : UIUserInterfaceStyleUnspecified;
  self.viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(browser);
  self.viewController.adaptiveDelegate = self;

  self.mediator = [[AdaptiveToolbarMediator alloc] init];
  self.mediator.incognito = browser->GetBrowserState()->IsOffTheRecord();
  self.mediator.consumer = self.viewController;
  self.mediator.navigationBrowserAgent =
      WebNavigationBrowserAgent::FromBrowser(browser);
  self.mediator.webStateList = browser->GetWebStateList();
  self.mediator.webContentAreaOverlayPresenter =
      OverlayPresenter::FromBrowser(browser, OverlayModality::kWebContentArea);
  self.mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          browser->GetBrowserState());
  self.mediator.actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:browser
             scenario:MenuScenarioHistogram::kToolbarMenu];

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

#pragma mark - AdaptiveToolbarViewControllerDelegate

- (void)exitFullscreen {
  FullscreenController::FromBrowser(self.browser)->ExitFullscreen();
}

#pragma mark - SideSwipeToolbarSnapshotProviding

- (UIImage*)toolbarSideSwipeSnapshotForWebState:(web::WebState*)webState {
  [self updateToolbarForSideSwipeSnapshot:webState];

  UIImage* toolbarSnapshot = CaptureViewWithOption(
      [self.viewController view], [[UIScreen mainScreen] scale],
      kClientSideRendering);

  [self resetToolbarAfterSideSwipeSnapshot];

  return toolbarSnapshot;
}

#pragma mark - NewTabPageControllerDelegate

- (void)setScrollProgressForTabletOmnibox:(CGFloat)progress {
  [self.viewController setScrollProgressForTabletOmnibox:progress];
}

- (UIResponder<UITextInput>*)fakeboxScribbleForwardingTarget {
  // Implemented in `ToolbarCoordinator`.
  return nil;
}

#pragma mark - ToolbarCommands

- (void)triggerToolbarSlideInAnimation {
  // Implemented in primary and secondary toolbars directly.
}

- (void)setTabGridButtonIPHHighlighted:(BOOL)iphHighlighted {
  [self.mediator updateConsumerWithTabGridButtonIPHHighlighted:iphHighlighted];
}

- (void)setNewTabButtonIPHHighlighted:(BOOL)iphHighlighted {
  [self.mediator updateConsumerWithNewTabButtonIPHHighlighted:iphHighlighted];
}

#pragma mark - ToolbarCoordinatee

- (id<PopupMenuUIUpdating>)popupMenuUIUpdater {
  return self.viewController;
}

#pragma mark - Protected

- (ToolbarButtonFactory*)buttonFactoryWithType:(ToolbarType)type {
  BOOL isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();
  ToolbarStyle style =
      isIncognito ? ToolbarStyle::kIncognito : ToolbarStyle::kNormal;

  ToolbarButtonActionsHandler* actionHandler =
      [[ToolbarButtonActionsHandler alloc]
          initWithEngagementTracker:feature_engagement::TrackerFactory::
                                        GetForBrowserState(
                                            self.browser->GetBrowserState())];

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

  self.actionHandler = actionHandler;

  ToolbarButtonFactory* buttonFactory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];
  buttonFactory.actionHandler = actionHandler;
  buttonFactory.visibilityConfiguration =
      [[ToolbarButtonVisibilityConfiguration alloc] initWithType:type];

  return buttonFactory;
}

- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState {
  BOOL isNTP = IsVisibleURLNewTabPage(webState);

  [self.mediator updateConsumerForWebState:webState];
  [self.viewController updateForSideSwipeSnapshotOnNTP:isNTP];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [self.mediator updateConsumerForWebState:self.browser->GetWebStateList()
                                               ->GetActiveWebState()];
  [self.viewController resetAfterSideSwipeSnapshot];
}

@end
