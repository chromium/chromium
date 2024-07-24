// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/identity_confirmation_app_agent.h"

#import "base/logging.h"
#import "base/metrics/field_trial_params.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation IdentityConfirmationAppAgent {
  // This code ensures only one snackbar appears at a time on an iPad in
  // split-screen mode (where two scenes are foreground active simultaneously).
  // It does this by resetting this boolean only when any of the scenes become
  // inactive or in background.
  BOOL _snackbarAlreadyShownForForegroundActive;
}

#pragma mark - SceneObservingAppAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (!base::FeatureList::IsEnabled(kIdentityConfirmationSnackbar)) {
    return;
  }
  if (self.appState.initStage != InitStageFinal) {
    return;
  }

  [super sceneState:sceneState transitionedToActivationLevel:level];
  switch (level) {
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive:
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached:
      _snackbarAlreadyShownForForegroundActive = NO;
      break;

    case SceneActivationLevelForegroundActive:
      if (!_snackbarAlreadyShownForForegroundActive) {
        [self showIdentityConfirmationSnackbarWithSceneState:sceneState];
        _snackbarAlreadyShownForForegroundActive = YES;
      }
      break;
  }
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (!base::FeatureList::IsEnabled(kIdentityConfirmationSnackbar)) {
    return;
  }
  if (!appState.foregroundActiveScene) {
    return;
  }
  if (self.appState.initStage != InitStageFinal) {
    return;
  }

  [super appState:appState didTransitionFromInitStage:previousInitStage];
  if (!_snackbarAlreadyShownForForegroundActive) {
    // In case of having a foregroundActiveScene before reaching an
    // InitStageFinal, this will be the fallback to show the snackbar.
    [self showIdentityConfirmationSnackbarWithSceneState:
              appState.foregroundActiveScene];
    _snackbarAlreadyShownForForegroundActive = YES;
  }
}

#pragma mark - Private

- (void)showIdentityConfirmationSnackbarWithSceneState:(SceneState*)sceneState {
  CHECK(base::FeatureList::IsEnabled(kIdentityConfirmationSnackbar));

  Browser* browser =
      sceneState.browserProviderInterface.mainBrowserProvider.browser;
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          browser->GetBrowserState());
  if (!authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    return;
  }

  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          browser->GetBrowserState());
  NSArray<id<SystemIdentity>>* allIdentities =
      accountManagerService->GetAllIdentities();
  if ([allIdentities count] <= 1) {
    return;
  }

  PrefService* prefService = browser->GetBrowserState()->GetPrefs();
  base::Time lastSignin = prefService->GetTime(prefs::kLastSigninTimestamp);
  if (base::Time::Now() - lastSignin <
      kIdentityConfirmationMinTimeSinceSignin.Get()) {
    return;
  }

  const base::Time lastPrompted =
      prefService->GetTime(prefs::kIdentityConfirmationSnackbarLastPromptTime);
  if (base::Time::Now() - lastPrompted <
      kIdentityConfirmationMinDisplayInterval.Get()) {
    return;
  }
  prefService->SetTime(prefs::kIdentityConfirmationSnackbarLastPromptTime,
                       base::Time::Now());

  id<SystemIdentity> systemIdentity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  DCHECK(systemIdentity);
  NSString* accountName = systemIdentity.userGivenName
                              ? systemIdentity.userGivenName
                              : systemIdentity.userEmail;
  MDCSnackbarMessage* snackbarTitle = CreateSnackbarMessage(
      l10n_util::GetNSStringF(IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
                              base::SysNSStringToUTF16(accountName)));
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarCommandsHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarCommandsHandler showSnackbarMessage:snackbarTitle bottomOffset:0];
}

@end
