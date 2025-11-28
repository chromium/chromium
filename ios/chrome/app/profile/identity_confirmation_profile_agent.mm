// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/identity_confirmation_profile_agent.h"

#import "base/logging.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation IdentityConfirmationProfileAgent {
  // This code ensures only one snackbar appears at a time on an iPad in
  // split-screen mode (where two scenes are foreground active simultaneously).
  // It does this by resetting this boolean only when any of the scenes become
  // inactive or in background.
  BOOL _foregroundActiveEventAlreadyHandled;
}

#pragma mark - SceneObservingProfileAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (self.profileState.initStage != ProfileInitStage::kFinal) {
    return;
  }
  id<BrowserProviderInterface> providerInterface =
      self.profileState.foregroundActiveScene.browserProviderInterface;
  id<BrowserProvider> presentingInterface =
      providerInterface.currentBrowserProvider;
  if (presentingInterface != providerInterface.mainBrowserProvider) {
    return;
  }

  switch (level) {
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive:
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached:
      _foregroundActiveEventAlreadyHandled = NO;
      break;

    case SceneActivationLevelForegroundActive:
      if (!_foregroundActiveEventAlreadyHandled) {
        [self
            maybeShowIdentityConfirmationSnackbarWithBrowser:presentingInterface
                                                                 .browser];
        _foregroundActiveEventAlreadyHandled = YES;
      }
      break;
  }
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if ((!profileState.foregroundActiveScene) ||
      (profileState.initStage < ProfileInitStage::kFinal)) {
    return;
  }

  id<BrowserProviderInterface> providerInterface =
      profileState.foregroundActiveScene.browserProviderInterface;
  id<BrowserProvider> presentingInterface =
      providerInterface.currentBrowserProvider;
  if (presentingInterface != providerInterface.mainBrowserProvider) {
    return;
  }

  if (!_foregroundActiveEventAlreadyHandled) {
    // In case of having a foregroundActiveScene before reaching an
    // ProfileInitStage::kFinal, this will be the fallback to show the snackbar.
    [self maybeShowIdentityConfirmationSnackbarWithBrowser:presentingInterface
                                                               .browser];
    _foregroundActiveEventAlreadyHandled = YES;
  }
}

#pragma mark - Private

// Whether the identity confirmation snackbar should be shown. If not, the
// reason why it should not. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
// LINT.IfChange
enum class IdentityConfirmationSnackbarDecision {
  // The snackbar should be shown.
  kShouldShow = 0,
  // The user is signed-out, so no identity to display.
  kDontShowNoAccount = 1,
  // The user has a single account on the device, so no need to remind them that
  // they are currently using this account.
  kDontShowSingleAccount = 2,
  // The user has a personal account, and `self` is not executing on top of
  // Bling Start. Then no need to display a identity reminder.
  kDontShowNotOnStartPage = 3,
  // The reminder was shown recently, no need to show it again.
  kDontShowShownRecently = 4,
  // The reminder was shown enough time already, we won’t display it anymore.
  kDontShowImpressionLimitReached = 5,
  // Not used anymore.
  kDontShowFeatureDisabled = 6,
  kMaxValue = kDontShowFeatureDisabled
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/signin/enums.xml)

// Whether the identity snackbar should be displayed. If not, the reason for it
// not to be displayed.
- (IdentityConfirmationSnackbarDecision)
    shouldShowIdentityConfirmationSnackbarWithBrowser:(Browser*)browser {
  ProfileIOS* profile = browser->GetProfile();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (!authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // As the user is signed-out, don’t show the identity snackbar.
    return IdentityConfirmationSnackbarDecision::kDontShowNoAccount;
  }

  NSArray<id<SystemIdentity>>* identitiesOnDevice =
      signin::GetIdentitiesOnDevice(profile);
  if ([identitiesOnDevice count] <= 1) {
    // As the user has a single account on the device, it’s not necessary to
    // remind them which account they are currently using.
    return IdentityConfirmationSnackbarDecision::kDontShowSingleAccount;
  }

  // For non-managed accounts, show the snackbar only on top of Bling Start.
  if (!authenticationService->HasPrimaryIdentityManaged(
          signin::ConsentLevel::kSignin) &&
      ![self isStartSurfaceWithBrowser:browser]) {
    return IdentityConfirmationSnackbarDecision::kDontShowNotOnStartPage;
  }

  PrefService* localState = GetApplicationContext()->GetLocalState();

  const int displayCount =
      localState->GetInteger(prefs::kIdentityConfirmationSnackbarDisplayCount);
  const base::Time lastPrompted =
      localState->GetTime(prefs::kIdentityConfirmationSnackbarLastPromptTime);

  base::TimeDelta identityConfirmationMinDisplayInterval;
  if (displayCount == 0) {
    // Wait 1 day before the first reminder.
    // Note: lastPrompted in this case is equal to kLastSigninTimestamp.
    identityConfirmationMinDisplayInterval = base::Days(1);
  } else if (displayCount == 1) {
    // Wait 7 days before the second reminder.
    identityConfirmationMinDisplayInterval = base::Days(7);
  } else if (displayCount == 2) {
    // Wait 30 days before the third reminder.
    identityConfirmationMinDisplayInterval = base::Days(30);
  } else {
    // Stop showing after the third reminder.
    return IdentityConfirmationSnackbarDecision::
        kDontShowImpressionLimitReached;
  }

  if (base::Time::Now() - lastPrompted <
      identityConfirmationMinDisplayInterval) {
    return IdentityConfirmationSnackbarDecision::kDontShowShownRecently;
  }

  // At this point, the snackbar will be shown, except if the feature flag is
  // disabled. Either way, update the prefs, so that the metrics remain
  // comparable between enabled and disabled groups.
  localState->SetInteger(prefs::kIdentityConfirmationSnackbarDisplayCount,
                         displayCount + 1);
  localState->SetTime(prefs::kIdentityConfirmationSnackbarLastPromptTime,
                      base::Time::Now());

  return IdentityConfirmationSnackbarDecision::kShouldShow;
}

// Checks whether the identity confirmation snackbar should be displayed, log
// the decision and its reason, and display it if it’s relevant.
- (void)maybeShowIdentityConfirmationSnackbarWithBrowser:(Browser*)browser {
  IdentityConfirmationSnackbarDecision decision =
      [self shouldShowIdentityConfirmationSnackbarWithBrowser:browser];

  base::UmaHistogramEnumeration("Signin.IdentityConfirmationSnackbarDecision",
                                decision);

  if (decision != IdentityConfirmationSnackbarDecision::kShouldShow) {
    return;
  }

  [self showSnackbarMessageWithBrowser:browser];
}

// Shows a snackbar reminding the user which account Chrome is currently using.
- (void)showSnackbarMessageWithBrowser:(Browser*)browser {
  ProfileIOS* profile = browser->GetProfile();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> systemIdentity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  CHECK(systemIdentity, base::NotFatalUntil::M151);

  SnackbarMessage* message =
      CreateIdentitySnackbarMessage(systemIdentity, browser);

  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarCommandsHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarCommandsHandler showSnackbarMessageOverBrowserToolbar:message];
}

- (BOOL)isStartSurfaceWithBrowser:(Browser*)browser {
  web::WebState* webState = browser->GetWebStateList()->GetActiveWebState();
  // The web state is nil if the NTP is in another tab. In this case, it is
  // never a start surface.
  if (!webState) {
    return NO;
  }
  NewTabPageTabHelper* NTPHelper = NewTabPageTabHelper::FromWebState(webState);
  return NTPHelper && NTPHelper->ShouldShowStartSurface();
}

@end
