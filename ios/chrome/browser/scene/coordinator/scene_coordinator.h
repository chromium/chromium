// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_COORDINATOR_SCENE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SCENE_COORDINATOR_SCENE_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/coordinator/root_coordinator/root_coordinator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

class Browser;
class GURL;
enum class SafariDataImportEntryPoint;
@protocol SafariDataImportUIHandler;
@protocol SceneCommands;
@class ShowSigninCommand;
@class SigninCoordinator;
@protocol TabGridCoordinatorDelegate;

// Coordinator for the scene, managing the top-level UI.
@interface SceneCoordinator : RootCoordinator

- (instancetype)initWithSceneCommandsEndpoint:
                    (id<SceneCommands>)sceneCommandsEndpoint
                               regularBrowser:(Browser*)regularBrowser
                              inactiveBrowser:(Browser*)inactiveBrowser
                             incognitoBrowser:(Browser*)incognitoBrowser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, weak) id<TabGridCoordinatorDelegate> delegate;

// Proxy properties for TabGridCoordinator.
@property(nonatomic, readonly, strong) UIViewController* activeViewController;

// Updates the incognito browser. Should only be sets when both the current
// incognito browser and the new incognito browser are either nil or contain no
// tabs. This must be called after the incognito browser has been deleted
// because the incognito profile is deleted.
@property(nonatomic, assign) Browser* incognitoBrowser;

// Returns YES if sign-in is in progress.
@property(nonatomic, readonly) BOOL isSigninInProgress;

// YES if the Tab Grid is currently being shown.
- (BOOL)isTabGridActive;

// Stops all child coordinators then calls `completion`. `completion` is called
// whether or not child coordinators exist.
- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion;

// Displays the TabGrid at `page`.
- (void)showTabGridPage:(TabGridPage)page;

// Displays the given view controller.
// Runs the given `completion` block after the view controller is visible.
- (void)showTabViewController:(UIViewController*)viewController
                    incognito:(BOOL)incognito
                   completion:(ProceduralBlock)completion;

// Sets the `mode` as the active one.
- (void)setActiveMode:(TabGridMode)mode;

// Shows the account menu.
- (void)showAccountMenuFromWebWithURL:(const GURL&)url;

// Shows the signin UI.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController;

// Shows the fullscreen sign-in promo.
- (void)showFullscreenSigninPromoWithCompletion:
    (SigninCoordinatorCompletionCallback)completion;

// Shows the web sign-in promo.
- (void)showWebSigninPromoFromViewController:(UIViewController*)viewController
                                         URL:(const GURL&)URL;

// Stops the sign-in coordinator actions and dismisses its views either
// with or without animation. Executes its signinCompletion. It’s expected to be
// not already executed.
- (void)stopSigninCoordinatorWithCompletionAnimated:(BOOL)animated;

// Shows the Safari Data Import UI.
- (void)
    displaySafariDataImportFromEntryPoint:(SafariDataImportEntryPoint)entryPoint
                            withUIHandler:
                                (id<SafariDataImportUIHandler>)UIHandler
                       baseViewController:(UIViewController*)baseViewController;

// Stops the Safari Data Import coordinator.
- (void)stopSafariDataImportCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_COORDINATOR_SCENE_COORDINATOR_H_
