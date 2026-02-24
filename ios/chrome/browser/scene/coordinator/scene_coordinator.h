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
class GURL;
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
    : RootCoordinator <SettingsCommands, SettingsNavigationControllerDelegate>

- (instancetype)initWithSceneCommandsEndpoint:
                    (id<SceneCommands>)sceneCommandsEndpoint
                                    tabOpener:(id<TabOpening>)tabOpener
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

// Shows the Safari Data Import UI.
- (void)displaySafariDataImportFromEntryPoint:
            (SafariDataImportEntryPoint)entryPoint
                                withUIHandler:
                                    (id<SafariDataImportUIHandler>)UIHandler;

// Stops the settings navigation controller.
- (void)stopSettingsAnimated:(BOOL)animated
                  completion:(ProceduralBlock)completion;

// Creates the settings navigation controller for the safety check if it doesn't
// exist.
- (void)createSafetyCheckSettingsWithReferrer:
    (password_manager::PasswordCheckReferrer)referrer;

// Shows the Password Checkup page for `referrer`.
- (void)showPasswordCheckupPageForReferrer:
    (password_manager::PasswordCheckReferrer)referrer;

// Shows the Password Issues page for `warningType`.
- (void)
    showPasswordIssuesWithWarningType:(password_manager::WarningType)warningType
                             referrer:(password_manager::PasswordCheckReferrer)
                                          referrer;

// Stops the Password Checkup coordinator.
- (void)stopPasswordCheckupCoordinator;

// Shows the History page.
- (void)showHistory;

// Shows the Youtube Incognito interstitial with the given `URLLoadParams`.
- (void)showYoutubeIncognitoWithUrlLoadParams:
    (const UrlLoadParams&)URLLoadParams;

// Shows the Incognito interstitial with the given `URLLoadParams`.
- (void)showIncognitoInterstitialWithUrlLoadParams:
    (const UrlLoadParams&)URLLoadParams;

// Shows the settings navigation controller.
- (void)presentSettingsFromViewController:(UIViewController*)baseViewController;

// Shows the settings UI, presenting from `baseViewController` and with blue dot
// for default browser settings if specified.
- (void)showSettingsFromViewController:(UIViewController*)baseViewController
              hasDefaultBrowserBlueDot:(BOOL)hasDefaultBrowserBlueDot;

// Shows the Safe Browsing settings page presenting from `baseViewController`.
- (void)showSafeBrowsingSettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the Settings UI, presenting from `baseViewController`.
- (void)showSettingsFromViewController:(UIViewController*)baseViewController;

// Shows the settings Privacy UI.
- (void)showPrivacySettingsFromViewController:
    (UIViewController*)baseViewController;

// Shows the Report an Issue UI, presenting from `baseViewController`.
- (void)showReportAnIssueFromViewController:
            (UIViewController*)baseViewController
                                     sender:(UserFeedbackSender)sender;

// Shows the Report an Issue UI, presenting from `baseViewController`, using
// `specificProductData` for additional product data to be sent in the report.
- (void)
    showReportAnIssueFromViewController:(UIViewController*)baseViewController
                                 sender:(UserFeedbackSender)sender
                    specificProductData:(NSDictionary<NSString*, NSString*>*)
                                            specificProductData;

// Shows the Settings UI if nothing else is displayed.
- (void)maybeShowSettingsFromViewController;

// Opens the Price Tracking notifications settings UI.
- (void)openPriceTrackingNotificationsSettings;

// Opens a debug menu for AI prototyping.
- (void)openAIMenu;

// Opens the assistant sheet.
- (void)showAssistant;

// Shows the application App Store page, if any.
- (void)showAppStorePage;

// Shows a notification with the signed-in user account.
- (void)showSigninAccountNotificationFromViewController:
    (UIViewController*)baseViewController;

// Shows the settings UI for price tracking notifications.
- (void)showPriceTrackingNotificationsSettings;

// Closes presented views and opens `command`.
- (void)closePresentedViewsAndOpenURL:(OpenNewTabCommand*)command;

// Closes presented views.
- (void)closePresentedViews;

// Closes presented views.
- (void)closePresentedViews:(BOOL)animated
                 completion:(ProceduralBlock)completion;

// Dismisses all modal dialogs.
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox
                         dismissSnackbars:(BOOL)dismissSnackbars;

// Dismisses all modal dialogs.
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

// Dismisses all modal dialogs.
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion;

// Dismisses all modal dialogs (if any) before showing the Password Checkup page
// for `referrer`.
- (void)dismissModalsAndShowPasswordCheckupPageForReferrer:
    (password_manager::PasswordCheckReferrer)referrer;

// Opens the `command` URL in a new tab.
- (void)openURLInNewTab:(OpenNewTabCommand*)command;

// Open a new window with `userActivity`
- (void)openNewWindowWithActivity:(NSUserActivity*)userActivity;

// Shows the TabGrid, in the chosen `mode`.
- (void)displayTabGridInMode:(TabGridOpeningMode)mode;

// Stops voice search on all browsers (regular and incognito) in the scene.
- (void)stopAllVoiceSearch;

// Sets whether the UI is displaying incognito content.
- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible;

// Prepare to show the TabSwitcher UI.
- (void)prepareTabSwitcher;

// Closes all open modals. If `dismissSnackbars` is YES, also dismisses
// all snackbars. Ensures that a non-incognito NTP tab is open. If
// incognito is forced, then it will ensure an incognito NTP tab is open.
// The `completion` block is called once all these preparations are complete.
- (void)prepareToPresentModalWithSnackbarDismissal:(BOOL)dismissSnackbars
                                        completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_SCENE_COORDINATOR_SCENE_COORDINATOR_H_
