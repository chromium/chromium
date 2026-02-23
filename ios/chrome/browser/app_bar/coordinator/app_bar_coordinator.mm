// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_coordinator.h"

#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_controller.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

@implementation AppBarCoordinator {
  AppBarContainerViewController* _containerViewController;
  AppBarViewController* _viewController;
  AppBarMediator* _mediator;
  raw_ptr<Browser> _incognitoBrowser;
  raw_ptr<Browser> _regularBrowser;
}

- (instancetype)initWithRegularBrowser:(Browser*)regularBrowser
                      incognitoBrowser:(Browser*)incognitoBrowser {
  self = [super init];
  if (self) {
    _incognitoBrowser = incognitoBrowser;
    _regularBrowser = regularBrowser;
  }
  return self;
}

- (void)start {
  CommandDispatcher* regularDispatcher =
      _regularBrowser->GetCommandDispatcher();
  // It is ok to use the regular browser here as the Scene commands are
  // handled by the same object for both modes.
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(regularDispatcher, SceneCommands);
  id<TabGridCommands> tabGridHandler =
      HandlerForProtocol(regularDispatcher, TabGridCommands);

  _viewController = [[AppBarViewController alloc] init];
  _viewController.sceneHandler = sceneHandler;
  _viewController.tabGridHandler = tabGridHandler;
  _viewController.layoutGuideCenter = LayoutGuideCenterForBrowser(nil);

  _containerViewController = [[AppBarContainerViewController alloc] init];
  [_containerViewController setAppBar:_viewController];

  SceneState* sceneState = _regularBrowser->GetSceneState();

  _mediator = [[AppBarMediator alloc]
      initWithRegularWebStateList:_regularBrowser->GetWebStateList()
            incognitoWebStateList:_incognitoBrowser->GetWebStateList()
                      prefService:_regularBrowser->GetProfile()->GetPrefs()
                        URLLoader:UrlLoadingBrowserAgent::FromBrowser(
                                      _regularBrowser)
                     tabGridState:sceneState.tabGridState
                   incognitoState:sceneState.incognitoState];
  _mediator.consumer = _viewController;
  _mediator.sceneHandler = sceneHandler;
  _mediator.tabGridHandler = tabGridHandler;
  _mediator.regularTabGroupsCommands =
      HandlerForProtocol(regularDispatcher, TabGroupsCommands);

  _viewController.mutator = _mediator;
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
  _regularBrowser = nullptr;
  _incognitoBrowser = nullptr;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return _containerViewController;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  _incognitoBrowser = incognitoBrowser;
  [_mediator setIncognitoWebStateList:incognitoBrowser
                                          ? incognitoBrowser->GetWebStateList()
                                          : nullptr];
}

@end
