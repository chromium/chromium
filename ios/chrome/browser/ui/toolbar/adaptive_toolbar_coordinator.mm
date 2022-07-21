// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator.h"

#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/find_in_page_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/main/layout_guide_scene_agent.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_actions_handler.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_mediator.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AdaptiveToolbarCoordinator ()

// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
// Mediator for updating the toolbar when the WebState changes.
@property(nonatomic, strong) ToolbarMediator* mediator;
// Actions handler for the toolbar buttons.
@property(nonatomic, strong) ToolbarButtonActionsHandler* actionHandler;
// The layout guide center to use to coordinate views.
@property(nonatomic, readonly) LayoutGuideCenter* layoutGuideCenter;

@end

@implementation AdaptiveToolbarCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  if (self.started)
    return;

  self.started = YES;

  self.viewController.longPressDelegate = self.longPressDelegate;
  self.viewController.overrideUserInterfaceStyle =
      self.browser->GetBrowserState()->IsOffTheRecord()
          ? UIUserInterfaceStyleDark
          : UIUserInterfaceStyleUnspecified;
  self.viewController.layoutGuideCenter = self.layoutGuideCenter;

  self.mediator = [[ToolbarMediator alloc] init];
  self.mediator.incognito = self.browser->GetBrowserState()->IsOffTheRecord();
  self.mediator.consumer = self.viewController;
  self.mediator.webStateList = self.browser->GetWebStateList();
  self.mediator.webContentAreaOverlayPresenter = OverlayPresenter::FromBrowser(
      self.browser, OverlayModality::kWebContentArea);
  self.mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator.actionFactory =
      [[BrowserActionFactory alloc] initWithBrowser:self.browser
                                           scenario:MenuScenario::kToolbarMenu];

  self.viewController.menuProvider = self.mediator;
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
  self.mediator = nil;
}

#pragma mark - Properties

- (LayoutGuideCenter*)layoutGuideCenter {
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  LayoutGuideSceneAgent* layoutGuideSceneAgent =
      [LayoutGuideSceneAgent agentFromScene:sceneState];
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return layoutGuideSceneAgent.incognitoLayoutGuideCenter;
  } else {
    return layoutGuideSceneAgent.layoutGuideCenter;
  }
}

- (void)setLongPressDelegate:(id<PopupMenuLongPressDelegate>)longPressDelegate {
  _longPressDelegate = longPressDelegate;
  self.viewController.longPressDelegate = longPressDelegate;
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
  // Only works in primary toolbar.
  return nil;
}

#pragma mark - ToolbarCommands

- (void)triggerToolsMenuButtonAnimation {
  [self.viewController.toolsMenuButton triggerAnimation];
}

#pragma mark - ToolbarCoordinatee

- (id<PopupMenuUIUpdating>)popupMenuUIUpdater {
  return self.viewController;
}

#pragma mark - Protected

- (ToolbarButtonFactory*)buttonFactoryWithType:(ToolbarType)type {
  BOOL isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();
  ToolbarStyle style = isIncognito ? INCOGNITO : NORMAL;

  ToolbarButtonActionsHandler* actionHandler =
      [[ToolbarButtonActionsHandler alloc] init];

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();

  actionHandler.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  actionHandler.activityHandler =
      HandlerForProtocol(dispatcher, ActivityServiceCommands);
  actionHandler.menuHandler = HandlerForProtocol(dispatcher, PopupMenuCommands);
  actionHandler.findHandler =
      HandlerForProtocol(dispatcher, FindInPageCommands);
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
