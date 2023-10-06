// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_coordinator.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_mediator.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"

@implementation IncognitoGridCoordinator {
  // Mediator of incognito grid.
  IncognitoGridMediator* _mediator;
  // Mutator that handle toolbars changes.
  __weak id<GridToolbarsMutator> _toolbarsMutator;
  // Delegate to handle presenting the action sheet.
  __weak id<GridMediatorDelegate> _gridMediatorDelegate;
  // Mediator for incognito reauth.
  IncognitoReauthMediator* _incognitoAuthMediator;
  // Commad dispatcher used while this coordinator's view controller is active.
  CommandDispatcher* _dispatcher;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                           toolbarsMutator:
                               (id<GridToolbarsMutator>)toolbarsMutator
                      gridMediatorDelegate:(id<GridMediatorDelegate>)delegate {
  CHECK(baseViewController);
  CHECK(browser);
  if (self = [super initWithBaseViewController:baseViewController
                                       browser:browser]) {
    CHECK(toolbarsMutator);
    CHECK(delegate);
    _toolbarsMutator = toolbarsMutator;
    _gridMediatorDelegate = delegate;
  }
  return self;
}

#pragma mark - Property Implementation.

- (IncognitoGridMediator*)incognitoGridMediator {
  CHECK(_mediator)
      << "IncognitoGridCoordinator's -start should be called before.";
  return _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // TODO(crbug.com/1246931): refactor to call setIncognitoBrowser from this
  // function.
  IncognitoReauthSceneAgent* reauthAgent = [IncognitoReauthSceneAgent
      agentFromScene:SceneStateBrowserAgent::FromBrowser(self.browser)
                         ->GetSceneState()];

  _dispatcher = [[CommandDispatcher alloc] init];
  [_dispatcher startDispatchingToTarget:reauthAgent
                            forProtocol:@protocol(IncognitoReauthCommands)];

  // TODO(crbug.com/1457146): Init view controller here instead of having a
  // public property.
  self.incognitoViewController.reauthAgent = reauthAgent;
  self.incognitoViewController.reauthHandler =
      HandlerForProtocol(_dispatcher, IncognitoReauthCommands);

  _mediator = [[IncognitoGridMediator alloc]
      initWithConsumer:self.incognitoViewController.incognitoTabsConsumer];
  _mediator.browser = self.browser;
  _mediator.delegate = _gridMediatorDelegate;
  _mediator.toolbarsMutator = _toolbarsMutator;
  _mediator.actionWrangler = self.incognitoViewController;
  _mediator.incognitoDelegate = self;

  _incognitoAuthMediator =
      [[IncognitoReauthMediator alloc] initWithReauthAgent:reauthAgent];
  _incognitoAuthMediator.consumer =
      self.incognitoViewController.incognitoTabsConsumer;
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _incognitoAuthMediator = nil;

  [_dispatcher stopDispatchingForProtocol:@protocol(IncognitoReauthCommands)];
  _dispatcher = nil;
}

#pragma mark - IncognitoGridMediatorDelegate

- (void)shouldDisableIncognito:(BOOL)disable {
  // TODO(crbug.com/1457146): When the incognito view controller will be managed
  // here, the view controller should be updated here (disabled VC <-> enabled
  // VC)

  _incognitoAuthMediator.consumer =
      self.incognitoViewController.incognitoTabsConsumer;
  self.incognitoViewController.reauthHandler =
      HandlerForProtocol(_dispatcher, IncognitoReauthCommands);
}

@end
