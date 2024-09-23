// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "base/debug/dump_without_crashing.h"
#import "base/metrics/histogram_functions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_constants.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/passcode_settings/passcode_settings_api.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Whether the passcode settings action should be displayed in the alert asking
// the user to set a passcode before accessing the Password Manager.
bool IsPasscodeSettingsAvailable() {
  // Use both kill switch and auth on entry feature flag to control the
  // dispalying of the action.
  return password_manager::features::IsPasscodeSettingsEnabled() &&
         ios::provider::SupportsPasscodeSettings();
}

}  // namespace

@interface ReauthenticationCoordinator () <
    ReauthenticationViewControllerDelegate,
    SceneStateObserver>

// Module used for requesting Local Authentication.
@property(nonatomic, strong) id<ReauthenticationProtocol> reauthModule;

// Application Commands dispatcher for closing settings ui and opening tabs.
@property(nonatomic, strong) id<ApplicationCommands> dispatcher;

// The view controller presented by the coordinator.
@property(nonatomic, strong)
    ReauthenticationViewController* reauthViewController;

// Coordinator for displaying an alert requesting the user to set up a
// passcode.
@property(nonatomic, strong) AlertCoordinator* passcodeRequestAlertCoordinator;

// Whether authentication should be required when the coordinator is started.
@property(nonatomic) BOOL authOnStart;

// Whether authentication should be required when the scene goes back to the
// foreground active state.
@property(nonatomic) BOOL authOnForegroundActive;

@end

@implementation ReauthenticationCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                          reauthenticationModule:(id<ReauthenticationProtocol>)
                                                     reauthenticationModule
                                     authOnStart:(BOOL)authOnStart {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    // Build reauth module if none is supplied.
    // Callers can supply one for testing or for reusing the authentication
    // result. See ReauthenticationProtocol.
    _reauthModule = reauthenticationModule
                        ? reauthenticationModule
                        : password_manager::BuildReauthenticationModule();
    _baseNavigationController = navigationController;
    _dispatcher =
        static_cast<id<ApplicationCommands>>(browser->GetCommandDispatcher());
    _authOnStart = authOnStart;
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self.browser->GetSceneState() addObserver:self];

  if (_authOnStart) {
    [self pushReauthenticationViewControllerWithRequestAuth:YES];
  }
}

- (void)stop {
  [self.browser->GetSceneState() removeObserver:self];
  _reauthViewController.delegate = nil;
  _reauthViewController = nil;
}

#pragma mark - ReauthenticationCoordinator

- (void)stopAndPopViewController {
  [self popReauthenticationViewController];
  [self stop];
}

#pragma mark - ReauthenticationViewControllerDelegate

// Creates and displays an alert requesting the user to set a passcode.
- (void)showSetUpPasscodeDialog {
  // TODO(crbug.com/40274927): Open iOS Passcode Settings for phase 2 launch in
  // M118. See i/p/p/c/b/password_auto_fill/password_auto_fill_api.h for
  // reference.
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_CONTENT);
  _passcodeRequestAlertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:_reauthViewController
                                                   browser:self.browser
                                                     title:title
                                                   message:message];

  __weak __typeof(self) weakSelf = self;

  if (IsPasscodeSettingsAvailable()) {
    // Action Go to Settings.
    [_passcodeRequestAlertCoordinator
        addItemWithTitle:l10n_util::GetNSString(
                             IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_OPEN_SETTINGS)
                  action:^{
                    [weakSelf openPasscodeSettings];
                  }
                   style:UIAlertActionStyleDefault
               preferred:YES
                 enabled:YES];

  } else {
    // Action OK -> Close UI.
    [_passcodeRequestAlertCoordinator
        addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                  action:^{
                    [weakSelf closeUI];
                  }
                   style:UIAlertActionStyleCancel];
  }

  // Action Learn How -> Close settings and open passcode help page.
  [_passcodeRequestAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                action:^{
                  [weakSelf openPasscodeHelpPage];
                }
                 style:UIAlertActionStyleDefault];

  [_passcodeRequestAlertCoordinator start];
}

- (void)reauthenticationDidFinishWithSuccess:(BOOL)success {
  if (success) {
    [self popReauthenticationViewController];

    [_delegate successfulReauthenticationWithCoordinator:self];
    // The user has been authenticated. No need to reauth until the scene goes
    // back go the background.
    _authOnForegroundActive = NO;
  } else {
    [self closeUI];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // Observing scene activation level changes to block the surface below on app
  // switch or device lock.
  //
  // When the app is moving to the background -> Block the surface below:
  // 1 - Foreground active: initial state.
  // 2 - Foreground inactive: push reauth view controller to prevent the surface
  // below from being captured in the app snapshot taken by iOS ( visible in the
  // app switcher).
  // 3 - Background: we might not get to this state if user opens
  // the app switcher but goes back to the browser, in which case we don't want
  // to request auth. Only when the app is backgrounded we will request auth
  // once it is foregrounded.
  //
  // When the backgrounded app is moving back to the foreground -> Request auth
  // and unblock on success or dismiss settings on failure:
  // 1 - Background: initial state if the app was fully backgrounded otherwise
  // it is foreground inactive.
  // 2 - Foreground inactive: Nothing to do. At this point the reauth view
  // controller should be blocking the surface below.
  // 3 - Foreground active: Request auth if the app was fully
  // backgrounded. Otherwise just pop the reauth view controller and unblock the
  // surface below.
  switch (level) {
    case SceneActivationLevelBackground:
      // Require auth next time the scene is foregrounded.
      _authOnForegroundActive = YES;
      [[fallthrough]];
    case SceneActivationLevelForegroundInactive:
      // Present reauth vc if not presented already.
      // Ideally do it while the scene is still in the foreground to prevent the
      // top surface in the navigation stack from being visible in the app
      // switcher. This is not always possible as the app some times goes
      // straight to `SceneActivationLevelBackground`. See crbug.com/40074678.
      if (!_reauthViewController) {
        [self pushReauthenticationViewControllerWithRequestAuth:NO];
      }
      break;

    case SceneActivationLevelForegroundActive:
      // Either ask for reauth if the scene was fully backgrounded or just
      // remove the blocking view controller.
      if (_authOnForegroundActive) {
        _authOnForegroundActive = NO;
        if (!_reauthViewController) {
          base::debug::DumpWithoutCrashing();
        }
        [_reauthViewController requestAuthentication];
      } else {
        [self popReauthenticationViewController];
      }
      break;
    case SceneActivationLevelUnattached:
    case SceneActivationLevelDisconnected:
      break;
  }
}

#pragma mark - Private

// Pushes the ReauthenticationViewController in the navigation stack.
- (void)pushReauthenticationViewControllerWithRequestAuth:(BOOL)requestAuth {
  [_delegate willPushReauthenticationViewController];

  // Dismiss any presented state.
  UIViewController* topViewController =
      _baseNavigationController.topViewController;
  UIViewController* presentedViewController =
      topViewController.presentedViewController;
  // Do not dismiss the Search Controller, otherwise pushViewController does not
  // add the new view controller to the top of the navigation stack.
  if (![presentedViewController isKindOfClass:[UISearchController class]] &&
      !presentedViewController.isBeingDismissed) {
    [presentedViewController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
  }

  _reauthViewController = [[ReauthenticationViewController alloc]
      initWithReauthenticationModule:_reauthModule
              reauthUponPresentation:requestAuth];
  _reauthViewController.delegate = self;

  // Don't animate presentation to block top view controller right away.
  [_baseNavigationController pushViewController:_reauthViewController
                                       animated:NO];
}

// Pops the ReauthenticationViewController from the navigation stack.
- (void)popReauthenticationViewController {
  // No op if vc was already dismissed. This happens when auth is triggered
  // which moves the scene to foreground inactive, then both successful auth and
  // the scene going back to foreground active dismiss the vc.
  if (!_reauthViewController || !_baseNavigationController) {
    return;
  }

  DCHECK_EQ(_baseNavigationController.topViewController, _reauthViewController);

  [_baseNavigationController popViewControllerAnimated:NO];
  _reauthViewController.delegate = nil;
  _reauthViewController = nil;
}

// Dismisses the UI protected with Local Authentication.
- (void)closeUI {
  [_delegate dismissUIAfterFailedReauthenticationWithCoordinator:self];
}

// Closes the UI and open the support page on setting up a passcode.
- (void)openPasscodeHelpPage {
  // TODO(crbug.com/40274927): Move to ReauthenticationCoordinatorDelegate.
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL(kPasscodeArticleURL)];
  [_dispatcher closeSettingsUIAndOpenURL:command];
}

- (void)openPasscodeSettings {
  [self closeUI];

  base::UmaHistogramEnumeration(
      /*name=*/password_manager::kReauthenticationUIEventHistogram,
      /*sample=*/ReauthenticationEvent::kOpenPasscodeSettings);

  ios::provider::OpenPasscodeSettings();
}

@end
