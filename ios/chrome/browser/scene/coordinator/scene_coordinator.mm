// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/scene_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_coordinator.h"

@implementation SceneCoordinator {
  id<ApplicationCommands> _applicationCommandEndpoint;
  base::WeakPtr<Browser> _inactiveBrowser;
  base::WeakPtr<Browser> _regularBrowser;
  // Coordinator for the Tab Grid
  TabGridCoordinator* _tabGridCoordinator;
}

- (instancetype)initWithApplicationCommandEndpoint:
                    (id<ApplicationCommands>)applicationCommandEndpoint
                                    regularBrowser:(Browser*)regularBrowser
                                   inactiveBrowser:(Browser*)inactiveBrowser
                                  incognitoBrowser:(Browser*)incognitoBrowser {
  if ((self = [super init])) {
    _applicationCommandEndpoint = applicationCommandEndpoint;
    _regularBrowser = regularBrowser->AsWeakPtr();
    _inactiveBrowser = inactiveBrowser->AsWeakPtr();
    _incognitoBrowser = incognitoBrowser;
  }
  return self;
}

- (void)start {
  _tabGridCoordinator = [[TabGridCoordinator alloc]
      initWithApplicationCommandEndpoint:_applicationCommandEndpoint
                          regularBrowser:_regularBrowser.get()
                         inactiveBrowser:_inactiveBrowser.get()
                        incognitoBrowser:_incognitoBrowser];
  _tabGridCoordinator.delegate = self.delegate;
  [_tabGridCoordinator start];
}

- (void)stop {
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

@end
