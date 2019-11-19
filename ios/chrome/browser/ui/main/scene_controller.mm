// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_controller.h"

#import "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/main_controller_guts.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#include "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_coordinator.h"
#include "ios/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"
#import "ios/chrome/browser/url_loading/app_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// A rough estimate of the expected duration of a view controller transition
// animation. It's used to temporarily disable mutally exclusive chrome
// commands that trigger a view controller presentation.
const int64_t kExpectedTransitionDurationInNanoSeconds = 0.2 * NSEC_PER_SEC;

// Possible results of snapshotting at the moment the user enters the tab
// switcher. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class EnterTabSwitcherSnapshotResult {
  // Page was loading at the time of the snapshot request, and the snapshot
  // failed.
  kPageLoadingAndSnapshotFailed = 0,
  // Page was loading at the time of the snapshot request, and the snapshot
  // succeeded.
  kPageLoadingAndSnapshotSucceeded = 1,
  // Page was not loading at the time of the snapshot request, and the snapshot
  // failed.
  kPageNotLoadingAndSnapshotFailed = 2,
  // Page was not loading at the time of the snapshot request, and the snapshot
  // succeeded.
  kPageNotLoadingAndSnapshotSucceeded = 3,
  // kMaxValue should share the value of the highest enumerator.
  kMaxValue = kPageNotLoadingAndSnapshotSucceeded,
};

}  // namespace

@interface SceneController ()

// A flag that keeps track of the UI initialization for the controlled scene.
@property(nonatomic, assign) BOOL hasInitializedUI;

@end

@implementation SceneController

- (instancetype)initWithSceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    _sceneState = sceneState;
    [_sceneState addObserver:self];
    // The window is necessary very early in the app/scene lifecycle, so it
    // should be created right away.
    if (!self.sceneState.window) {
      DCHECK(!IsMultiwindowSupported())
          << "The window must be created by the scene delegate";
      self.sceneState.window = [[ChromeOverlayWindow alloc]
          initWithFrame:[[UIScreen mainScreen] bounds]];
    }
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level > SceneActivationLevelBackground && !self.hasInitializedUI) {
    [self initializeUI];
  }
}

#pragma mark - private

- (void)initializeUI {
  self.hasInitializedUI = YES;
}

#pragma mark - ApplicationCommands

- (void)dismissModalDialogs {
  [self.mainController dismissModalDialogsWithCompletion:nil
                                          dismissOmnibox:YES];
}

- (void)showHistory {
  self.mainController.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.mainController.currentBVC
                    browserState:self.mainController.mainBrowserState];
  self.mainController.historyCoordinator.loadStrategy =
      [self currentPageIsIncognito] ? UrlLoadStrategy::ALWAYS_IN_INCOGNITO
                                    : UrlLoadStrategy::NORMAL;
  self.mainController.historyCoordinator.dispatcher =
      self.mainController.mainBVC.dispatcher;
  [self.mainController.historyCoordinator start];
}

// Opens an url from a link in the settings UI.
- (void)closeSettingsUIAndOpenURL:(OpenNewTabCommand*)command {
  [self openUrlFromSettings:command];
}

- (void)closeSettingsUI {
  [self.mainController closeSettingsAnimated:YES completion:nullptr];
}

- (void)prepareTabSwitcher {
  web::WebState* currentWebState =
      self.mainController.currentBVC.tabModel.webStateList->GetActiveWebState();
  if (currentWebState) {
    BOOL loading = currentWebState->IsLoading();
    SnapshotTabHelper::FromWebState(currentWebState)
        ->UpdateSnapshotWithCallback(^(UIImage* snapshot) {
          EnterTabSwitcherSnapshotResult snapshotResult;
          if (loading && !snapshot) {
            snapshotResult =
                EnterTabSwitcherSnapshotResult::kPageLoadingAndSnapshotFailed;
          } else if (loading && snapshot) {
            snapshotResult = EnterTabSwitcherSnapshotResult::
                kPageLoadingAndSnapshotSucceeded;
          } else if (!loading && !snapshot) {
            snapshotResult = EnterTabSwitcherSnapshotResult::
                kPageNotLoadingAndSnapshotFailed;
          } else {
            DCHECK(!loading && snapshot);
            snapshotResult = EnterTabSwitcherSnapshotResult::
                kPageNotLoadingAndSnapshotSucceeded;
          }
          UMA_HISTOGRAM_ENUMERATION("IOS.EnterTabSwitcherSnapshotResult",
                                    snapshotResult);
        });
  }
  [self.mainController.mainCoordinator
      prepareToShowTabSwitcher:self.mainController.tabSwitcher];
}

- (void)displayTabSwitcher {
  DCHECK(!self.mainController.isTabSwitcherActive);
  if (!self.mainController.isProcessingVoiceSearchCommand) {
    [self.mainController.currentBVC userEnteredTabSwitcher];
    [self.mainController showTabSwitcher];
    self.mainController.isProcessingTabSwitcherCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     self.mainController.isProcessingTabSwitcherCommand = NO;
                   });
  }
}

// TODO(crbug.com/779791) : Remove showing settings from MainController.
- (void)showAutofillSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.mainController.signinInteractionCoordinator
              .isSettingsViewPresented);
  if (self.mainController.settingsNavigationController)
    return;

  Browser* browser =
      self.mainController.interfaceProvider.mainInterface.browser;
  self.mainController.settingsNavigationController =
      [SettingsNavigationController
          autofillProfileControllerForBrowser:browser
                                     delegate:self.mainController];
  [baseViewController
      presentViewController:self.mainController.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showReportAnIssueFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(baseViewController);
  // This dispatch is necessary to give enough time for the tools menu to
  // disappear before taking a screenshot.
  dispatch_async(dispatch_get_main_queue(), ^{
    DCHECK(!self.mainController.signinInteractionCoordinator
                .isSettingsViewPresented);
    if (self.mainController.settingsNavigationController)
      return;
    Browser* browser =
        self.mainController.interfaceProvider.mainInterface.browser;
    self.mainController.settingsNavigationController =
        [SettingsNavigationController
            userFeedbackControllerForBrowser:browser
                                    delegate:self.mainController
                          feedbackDataSource:self.mainController
                                  dispatcher:self];
    [baseViewController
        presentViewController:self.mainController.settingsNavigationController
                     animated:YES
                   completion:nil];
  });
}

- (void)openURLInNewTab:(OpenNewTabCommand*)command {
  UrlLoadParams params =
      UrlLoadParams::InNewTab(command.URL, command.virtualURL);
  params.SetInBackground(command.inBackground);
  params.web_params.referrer = command.referrer;
  params.in_incognito = command.inIncognito;
  params.append_to = command.appendTo;
  params.origin_point = command.originPoint;
  params.from_chrome = command.fromChrome;
  params.user_initiated = command.userInitiated;
  params.should_focus_omnibox = command.shouldFocusOmnibox;
  self.mainController.appURLLoadingService->LoadUrlInNewTab(params);
}

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController {
  if (!self.mainController.signinInteractionCoordinator) {
    Browser* mainBrowser =
        self.mainController.interfaceProvider.mainInterface.browser;
    self.mainController.signinInteractionCoordinator =
        [[SigninInteractionCoordinator alloc]
            initWithBrowser:mainBrowser
                 dispatcher:self.mainController.mainBVC.dispatcher];
  }

  switch (command.operation) {
    case AUTHENTICATION_OPERATION_REAUTHENTICATE:
      [self.mainController.signinInteractionCoordinator
          reAuthenticateWithAccessPoint:command.accessPoint
                            promoAction:command.promoAction
               presentingViewController:baseViewController
                             completion:command.callback];
      break;
    case AUTHENTICATION_OPERATION_SIGNIN:
      [self.mainController.signinInteractionCoordinator
                signInWithIdentity:command.identity
                       accessPoint:command.accessPoint
                       promoAction:command.promoAction
          presentingViewController:baseViewController
                        completion:command.callback];
      break;
  }
}

- (void)showAdvancedSigninSettingsFromViewController:
    (UIViewController*)baseViewController {
  Browser* mainBrowser =
      self.mainController.interfaceProvider.mainInterface.browser;
  self.mainController.signinInteractionCoordinator =
      [[SigninInteractionCoordinator alloc]
          initWithBrowser:mainBrowser
               dispatcher:self.mainController.mainBVC.dispatcher];
  [self.mainController.signinInteractionCoordinator
      showAdvancedSigninSettingsWithPresentingViewController:
          baseViewController];
}

// TODO(crbug.com/779791) : Remove settings commands from MainController.
- (void)showAddAccountFromViewController:(UIViewController*)baseViewController {
  Browser* mainBrowser =
      self.mainController.interfaceProvider.mainInterface.browser;
  if (!self.mainController.signinInteractionCoordinator) {
    self.mainController.signinInteractionCoordinator =
        [[SigninInteractionCoordinator alloc]
            initWithBrowser:mainBrowser
                 dispatcher:self.mainController.mainBVC.dispatcher];
  }

  [self.mainController.signinInteractionCoordinator
      addAccountWithAccessPoint:signin_metrics::AccessPoint::
                                    ACCESS_POINT_UNKNOWN
                    promoAction:signin_metrics::PromoAction::
                                    PROMO_ACTION_NO_SIGNIN_PROMO
       presentingViewController:baseViewController
                     completion:nil];
}

- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible {
  _incognitoContentVisible = incognitoContentVisible;
}

- (void)startVoiceSearch {
  if (!self.mainController.isProcessingTabSwitcherCommand) {
    [self.mainController startVoiceSearchInCurrentBVC];
    self.mainController.isProcessingVoiceSearchCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     self.mainController.isProcessingVoiceSearchCommand = NO;
                   });
  }
}

- (void)showSettingsFromViewController:(UIViewController*)baseViewController {
  DCHECK(!self.mainController.signinInteractionCoordinator
              .isSettingsViewPresented);
  if (self.mainController.settingsNavigationController)
    return;
  [[DeferredInitializationRunner sharedInstance]
      runBlockIfNecessary:kPrefObserverInit];

  Browser* browser =
      self.mainController.interfaceProvider.mainInterface.browser;

  self.mainController.settingsNavigationController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser
                                  delegate:self.mainController];
  [baseViewController
      presentViewController:self.mainController.settingsNavigationController
                   animated:YES
                 completion:nil];
}

#pragma mark - ApplicationSettingsCommands

// TODO(crbug.com/779791) : Remove show settings from MainController.
- (void)showAccountsSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.mainController.signinInteractionCoordinator
              .isSettingsViewPresented);
  if (!baseViewController) {
    DCHECK_EQ(self.mainController.currentBVC,
              self.mainController.mainCoordinator.activeViewController);
    baseViewController = self.mainController.currentBVC;
  }

  if ([self.mainController currentBrowserState] -> IsOffTheRecord()) {
    NOTREACHED();
    return;
  }
  if (self.mainController.settingsNavigationController) {
    [self.mainController.settingsNavigationController
        showAccountsSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser =
      self.mainController.interfaceProvider.mainInterface.browser;
  self.mainController.settingsNavigationController =
      [SettingsNavigationController
          accountsControllerForBrowser:browser
                              delegate:self.mainController];
  [baseViewController
      presentViewController:self.mainController.settingsNavigationController
                   animated:YES
                 completion:nil];
}

// TODO(crbug.com/779791) : Remove Google services settings from MainController.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.mainController.signinInteractionCoordinator
              .isSettingsViewPresented);
  if (!baseViewController) {
    DCHECK_EQ(self.mainController.currentBVC,
              self.mainController.mainCoordinator.activeViewController);
    baseViewController = self.mainController.currentBVC;
  }

  if (self.mainController.settingsNavigationController) {
    // Navigate to the Google services settings if the settings dialog is
    // already opened.
    [self.mainController.settingsNavigationController
        showGoogleServicesSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser =
      self.mainController.interfaceProvider.mainInterface.browser;
  self.mainController.settingsNavigationController =
      [SettingsNavigationController
          googleServicesControllerForBrowser:browser
                                    delegate:self.mainController];

  [baseViewController
      presentViewController:self.mainController.settingsNavigationController
                   animated:YES
                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.mainController.signinInteractionCoordinator
              .isSettingsViewPresented);
  if (self.mainController.settingsNavigationController) {
    [self.mainController.settingsNavigationController
        showSyncPassphraseSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser =
      self.mainController.interfaceProvider.mainInterface.browser;
  self.mainController.settingsNavigationController =
      [SettingsNavigationController
          syncPassphraseControllerForBrowser:browser
                                    delegate:self.mainController];
  [baseViewController
      presentViewController:self.mainController.settingsNavigationController
                   animated:YES
                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showSavedPasswordsSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.mainController.signinInteractionCoordinator
              .isSettingsViewPresented);
  if (self.mainController.settingsNavigationController) {
    [self.mainController.settingsNavigationController
        showSavedPasswordsSettingsFromViewController:baseViewController];
    return;
  }
  Browser* browser =
      self.mainController.interfaceProvider.mainInterface.browser;
  self.mainController.settingsNavigationController =
      [SettingsNavigationController
          savePasswordsControllerForBrowser:browser
                                   delegate:self.mainController];
  [baseViewController
      presentViewController:self.mainController.settingsNavigationController
                   animated:YES
                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.mainController.signinInteractionCoordinator
              .isSettingsViewPresented);
  if (self.mainController.settingsNavigationController) {
    [self.mainController.settingsNavigationController
        showProfileSettingsFromViewController:baseViewController];
    return;
  }
  Browser* browser =
      self.mainController.interfaceProvider.mainInterface.browser;

  self.mainController.settingsNavigationController =
      [SettingsNavigationController
          autofillProfileControllerForBrowser:browser
                                     delegate:self.mainController];
  [baseViewController
      presentViewController:self.mainController.settingsNavigationController
                   animated:YES
                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showCreditCardSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.mainController.signinInteractionCoordinator
              .isSettingsViewPresented);
  if (self.mainController.settingsNavigationController) {
    [self.mainController.settingsNavigationController
        showCreditCardSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser =
      self.mainController.interfaceProvider.mainInterface.browser;
  self.mainController.settingsNavigationController =
      [SettingsNavigationController
          autofillCreditCardControllerForBrowser:browser
                                        delegate:self.mainController];
  [baseViewController
      presentViewController:self.mainController.settingsNavigationController
                   animated:YES
                 completion:nil];
}

#pragma mark - BrowsingDataCommands

- (void)removeBrowsingDataForBrowserState:(ios::ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock {
  // TODO(crbug.com/632772): https://bugs.webkit.org/show_bug.cgi?id=149079
  // makes it necessary to disable web usage while clearing browsing data.
  // It is however unnecessary for off-the-record BrowserState (as the code
  // is not invoked) and has undesired side-effect (cause all regular tabs
  // to reload, see http://crbug.com/821753 for details).
  BOOL disableWebUsageDuringRemoval =
      !browserState->IsOffTheRecord() &&
      IsRemoveDataMaskSet(removeMask, BrowsingDataRemoveMask::REMOVE_SITE_DATA);
  BOOL showActivityIndicator = NO;

  if (@available(iOS 13, *)) {
    // TODO(crbug.com/632772): Visited links clearing doesn't require disabling
    // web usage with iOS 13. Stop disabling web usage once iOS 12 is not
    // supported.
    showActivityIndicator = disableWebUsageDuringRemoval;
    disableWebUsageDuringRemoval = NO;
  }

  if (disableWebUsageDuringRemoval) {
    // Disables browsing and purges web views.
    // Must be called only on the main thread.
    DCHECK([NSThread isMainThread]);
    self.mainController.interfaceProvider.mainInterface.userInteractionEnabled =
        NO;
    self.mainController.interfaceProvider.incognitoInterface
        .userInteractionEnabled = NO;
  } else if (showActivityIndicator) {
    // Show activity overlay so users know that clear browsing data is in
    // progress.
    [self.mainController.mainBVC.dispatcher showActivityOverlay:YES];
  }

  BrowsingDataRemoverFactory::GetForBrowserState(browserState)
      ->Remove(timePeriod, removeMask, base::BindOnce(^{
                 // Activates browsing and enables web views.
                 // Must be called only on the main thread.
                 DCHECK([NSThread isMainThread]);
                 if (showActivityIndicator) {
                   // User interaction still needs to be disabled as a way to
                   // force reload all the web states and to reset NTPs.
                   self.mainController.interfaceProvider.mainInterface
                       .userInteractionEnabled = NO;
                   self.mainController.interfaceProvider.incognitoInterface
                       .userInteractionEnabled = NO;

                   [self.mainController.mainBVC.dispatcher
                       showActivityOverlay:NO];
                 }
                 self.mainController.interfaceProvider.mainInterface
                     .userInteractionEnabled = YES;
                 self.mainController.interfaceProvider.incognitoInterface
                     .userInteractionEnabled = YES;
                 [self.mainController.currentBVC setPrimary:YES];

                 if (completionBlock)
                   completionBlock();
               }));
}

#pragma mark - ApplicationCommandsHelpers

- (BOOL)currentPageIsIncognito {
  return self.mainController.currentBrowserState->IsOffTheRecord();
}

- (void)openUrlFromSettings:(OpenNewTabCommand*)command {
  DCHECK([command fromChrome]);
  UrlLoadParams params = UrlLoadParams::InNewTab([command URL]);
  params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  ProceduralBlock completion = ^{
    [self.mainController dismissModalsAndOpenSelectedTabInMode:
                             ApplicationModeForTabOpening::NORMAL
                                             withUrlLoadParams:params
                                                dismissOmnibox:YES
                                                    completion:nil];
  };
  [self.mainController closeSettingsAnimated:YES completion:completion];
}

@end
