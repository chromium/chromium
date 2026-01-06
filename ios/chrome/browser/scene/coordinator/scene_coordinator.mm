// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/scene_coordinator.h"

#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_coordinator.h"

@interface SceneCoordinator () <AccountMenuCoordinatorDelegate>
@end

@implementation SceneCoordinator {
  id<SceneCommands> _sceneCommandsEndpoint;
  base::WeakPtr<Browser> _inactiveBrowser;
  base::WeakPtr<Browser> _regularBrowser;
  // Coordinator for the Tab Grid
  TabGridCoordinator* _tabGridCoordinator;
  // Coordinator for the account menu.
  AccountMenuCoordinator* _accountMenuCoordinator;
}

- (instancetype)initWithSceneCommandsEndpoint:
                    (id<SceneCommands>)sceneCommandsEndpoint
                               regularBrowser:(Browser*)regularBrowser
                              inactiveBrowser:(Browser*)inactiveBrowser
                             incognitoBrowser:(Browser*)incognitoBrowser {
  if ((self = [super init])) {
    _sceneCommandsEndpoint = sceneCommandsEndpoint;
    _regularBrowser = regularBrowser->AsWeakPtr();
    _inactiveBrowser = inactiveBrowser->AsWeakPtr();
    _incognitoBrowser = incognitoBrowser;
  }
  return self;
}

- (void)start {
  _tabGridCoordinator = [[TabGridCoordinator alloc]
      initWithSceneCommandsEndpoint:_sceneCommandsEndpoint
                     regularBrowser:_regularBrowser.get()
                    inactiveBrowser:_inactiveBrowser.get()
                   incognitoBrowser:_incognitoBrowser];
  _tabGridCoordinator.delegate = self.delegate;
  [_tabGridCoordinator start];
}

- (void)stop {
  [self stopAccountMenu];
  [_tabGridCoordinator stop];
}

#pragma mark - Public

- (BOOL)isTabGridActive {
  return _tabGridCoordinator.isTabGridActive;
}

- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion {
  [_tabGridCoordinator stopChildCoordinatorsWithCompletion:completion];
}

- (void)showTabGridPage:(TabGridPage)page {
  [_tabGridCoordinator showTabGridPage:page];
}

- (void)showTabViewController:(UIViewController*)viewController
                    incognito:(BOOL)incognito
                   completion:(ProceduralBlock)completion {
  [_tabGridCoordinator showTabViewController:viewController
                                   incognito:incognito
                                  completion:completion];
}

- (void)setActiveMode:(TabGridMode)mode {
  [_tabGridCoordinator setActiveMode:mode];
}

- (void)showAccountMenuFromWebWithURL:(const GURL&)url {
  if (_accountMenuCoordinator) {
    return;
  }
  _accountMenuCoordinator = [[AccountMenuCoordinator alloc]
      initWithBaseViewController:self.activeViewController
                         browser:_regularBrowser.get()
                      anchorView:nil
                     accessPoint:AccountMenuAccessPoint::kWeb
                             URL:url];
  _accountMenuCoordinator.delegate = self;
  [_accountMenuCoordinator start];
}

#pragma mark - Properties

- (void)setDelegate:(id<TabGridCoordinatorDelegate>)delegate {
  _delegate = delegate;
  _tabGridCoordinator.delegate = delegate;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  _incognitoBrowser = incognitoBrowser;
  _tabGridCoordinator.incognitoBrowser = incognitoBrowser;
}

- (UIViewController*)activeViewController {
  return _tabGridCoordinator.activeViewController;
}

#pragma mark - AccountMenuCoordinatorDelegate

- (void)accountMenuCoordinatorWantsToBeStopped:
    (AccountMenuCoordinator*)coordinator {
  CHECK_EQ(_accountMenuCoordinator, coordinator);
  [self stopAccountMenu];
}

#pragma mark - Private

// Stops the account menu coordinator.
- (void)stopAccountMenu {
  [_accountMenuCoordinator stop];
  _accountMenuCoordinator.delegate = nil;
  _accountMenuCoordinator = nil;
}

@end
