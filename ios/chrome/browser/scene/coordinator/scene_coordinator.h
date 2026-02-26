// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCENE_COORDINATOR_SCENE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SCENE_COORDINATOR_SCENE_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/coordinator/root_coordinator/root_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_paging.h"

enum class ApplicationMode;
class Browser;
@protocol BrowserProviderInterface;
enum class SafariDataImportEntryPoint;
@protocol SafariDataImportUIHandler;
@protocol SceneCommands;
@class BrowserLayoutViewController;
@class OpenNewTabCommand;
@class SceneCoordinator;
class SceneUrlLoadingService;
@protocol SceneURLLoadingServiceDelegate;
@class SettingsNavigationController;
@class ShowSigninCommand;
@class SigninCoordinator;
@protocol TabOpening;
enum class UserFeedbackSender;
struct UrlLoadParams;

namespace password_manager {
enum class PasswordCheckReferrer;
enum class WarningType;
}  // namespace password_manager

// Protocol to handle Scene UI changes that require access to the BVC.
@protocol SceneUIHandler

// Sets the current interface to `ApplicationMode::INCOGNITO` or
// `ApplicationMode::NORMAL`.
- (void)setCurrentInterfaceForMode:(ApplicationMode)mode;

// Displays current (incognito/normal) BVC and calls `completion`.
- (void)displayCurrentBVC:(ProceduralBlock)completion;

// Checks the target BVC's current tab's URL. If `urlLoadParams` has an empty
// URL, no new tab will be opened and `tabOpenedCompletion` will be run. If this
// URL is chrome://newtab, loads `urlLoadParams` in this tab. Otherwise, open
// `urlLoadParams` in a new tab in the target BVC. `tabOpenedCompletion` will be
// called on the new tab (if not nil).
- (void)openOrReuseTabInMode:(ApplicationMode)targetMode
           withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
         tabOpenedCompletion:(ProceduralBlock)tabOpenedCompletion;

@end

// Coordinator for the scene, managing the top-level UI.
@interface SceneCoordinator
    : RootCoordinator <SceneCommands,
                       SettingsCommands,
                       SettingsNavigationControllerDelegate>

- (instancetype)initWithTabOpener:(id<TabOpening>)tabOpener
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// An object to handle scene ui changes.
@property(nonatomic, weak) id<SceneUIHandler> UIHandler;

// A delegate for the Tab Grid coordinator.
@property(nonatomic, weak) id<TabGridCoordinatorDelegate> tabGridDelegate;

// Proxy properties for TabGridCoordinator.
@property(nonatomic, readonly, strong) UIViewController* activeViewController;

// Updates the incognito browser. Should only be sets when both the current
// incognito browser and the new incognito browser are either nil or contain no
// tabs. This must be called after the incognito browser has been deleted
// because the incognito profile is deleted.
@property(nonatomic, assign) Browser* incognitoBrowser;

// Returns YES if sign-in is in progress.
@property(nonatomic, readonly) BOOL isSigninInProgress;

// The scene level component for url loading.
@property(nonatomic, assign) raw_ptr<SceneUrlLoadingService>
    sceneURLLoadingService;

// Sets the main, inactive, and incognito browsers from the given provider.
- (void)setBrowsersFromProvider:(id<BrowserProviderInterface>)provider;

// YES if the Tab Grid is currently being shown.
- (BOOL)isTabGridActive;

// Returns YES if the current Tab is available to present a view controller.
- (BOOL)isTabAvailableToPresentViewController;

// Open a non-incognito tab, if one exists. If one doesn't exist, open a new
// one. If incognito is forced, an incognito tab will be opened.
- (void)openNonIncognitoTab:(ProceduralBlock)completion;

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

// Shows the Youtube Incognito interstitial with the given `URLLoadParams`.
- (void)showYoutubeIncognitoWithUrlLoadParams:
    (const UrlLoadParams&)URLLoadParams;

// Shows the Incognito interstitial with the given `URLLoadParams`.
- (void)showIncognitoInterstitialWithUrlLoadParams:
    (const UrlLoadParams&)URLLoadParams;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_COORDINATOR_SCENE_COORDINATOR_H_
