// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GRID_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/root_coordinator/root_coordinator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

@class BrowserLayoutViewController;
class Browser;
@protocol SceneCommands;
@protocol TabGridCoordinatorDelegate;

@interface TabGridCoordinator : RootCoordinator

- (instancetype)initWithSceneCommandsEndpoint:
                    (id<SceneCommands>)sceneCommandsEndpoint
                               regularBrowser:(Browser*)regularBrowser
                              inactiveBrowser:(Browser*)inactiveBrowser
                             incognitoBrowser:(Browser*)incognitoBrowser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<TabGridCoordinatorDelegate> delegate;

// Updates the incognito browser. Should only be sets when both the current
// incognito browser and the new incognito browser are either nil or contain no
// tabs. This must be called after the incognito browser has been deleted
// because the incognito profile is deleted.
@property(nonatomic, assign) Browser* incognitoBrowser;

// The view controller, if any, that is active.
@property(nonatomic, readonly, strong) UIViewController* activeViewController;

// The view controller for the Tab Grid.
@property(nonatomic, readonly) UIViewController* viewController;

// If this property is YES, calls to `showTabGridPage:animated:` and
// `showBrowserLayoutViewController:completion:` will present the
// given view controllers without animation.  This should only be used by
// unittests.
@property(nonatomic, readwrite, assign) BOOL animationsDisabledForTesting;

// If this property is YES, it means the tab grid is the main user interface at
// the moment.
@property(nonatomic, readonly, getter=isTabGridActive) BOOL tabGridActive;

// Stops all child coordinators then calls `completion`. `completion` is called
// whether or not child coordinators exist.
- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion;

// Displays the TabGrid at `page`.
- (void)showTabGridPage:(TabGridPage)page;

// Displays the given browser layout view controller.
// Runs the given `completion` block after the view controller is visible.
- (void)showBrowserLayoutViewController:
            (BrowserLayoutViewController*)viewController
                              incognito:(BOOL)incognito
                             completion:(ProceduralBlock)completion;

// Sets the `mode` as the active one.
- (void)setActiveMode:(TabGridMode)mode;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GRID_COORDINATOR_H_
