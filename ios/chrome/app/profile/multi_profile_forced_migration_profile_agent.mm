
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/app/profile/multi_profile_forced_migration_profile_agent.h"

#import "base/logging.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

@interface MultiProfileForcedMigrationProfileAgent () <
    ManagedProfileCreationCoordinatorDelegate>
@end

@implementation MultiProfileForcedMigrationProfileAgent {
  // Dialog for the managed confirmation screen.
  ManagedProfileCreationCoordinator* _managedConfirmationScreenCoordinator;
}

#pragma mark - SceneObservingProfileAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (self.profileState.initStage != ProfileInitStage::kFinal) {
    return;
  }

  if ([self isIncognitoBrowserProvider]) {
    return;
  }

  switch (level) {
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive:
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelUnattached:
      break;
    case SceneActivationLevelForegroundActive:
      [self maybeShowMultiProfileForceMigrationScreen];
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

  if ([self isIncognitoBrowserProvider]) {
    return;
  }
  [self maybeShowMultiProfileForceMigrationScreen];
}

#pragma mark - ManagedProfileCreationCoordinatorDelegate

- (void)managedProfileCreationCoordinator:
            (ManagedProfileCreationCoordinator*)coordinator
                                didAccept:(BOOL)accepted
                     browsingDataSeparate:(BOOL)browsingDataSeparate {
  CHECK_EQ(_managedConfirmationScreenCoordinator, coordinator);
  base::RecordAction(base::UserMetricsAction(
      "Signin_MultiProfileForcedMigration_DialogAcknowleged"));
  _managedConfirmationScreenCoordinator.delegate = nil;
  [_managedConfirmationScreenCoordinator stop];
  _managedConfirmationScreenCoordinator = nil;
}

#pragma mark - Private

// Checks if multi-profile force migration screen should be presented to the
// user or not, and presents it if so.
- (void)maybeShowMultiProfileForceMigrationScreen {
  id<BrowserProvider> presentingInterface =
      self.profileState.foregroundActiveScene.browserProviderInterface
          .currentBrowserProvider;
  Browser* browser = presentingInterface.browser;
  // Sign-in related work should be done on regular browser.
  CHECK_EQ(browser->type(), Browser::Type::kRegular, base::NotFatalUntil::M145);

  ProfileIOS* profile = browser->GetProfile();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);

  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (!localState->GetBoolean(prefs::kMultiProfileForcedMigrationDone)) {
    return;
  }

  CHECK(AreSeparateProfilesForManagedAccountsEnabled(),
        base::NotFatalUntil::M148);

  id<SystemIdentity> systemIdentity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  CHECK(systemIdentity, base::NotFatalUntil::M148);

  SystemIdentityManager* systemIdentityManager =
      GetApplicationContext()->GetSystemIdentityManager();
  _managedConfirmationScreenCoordinator = [[ManagedProfileCreationCoordinator
      alloc] initWithBaseViewController:presentingInterface.viewController
                                   identity:systemIdentity
                               hostedDomain:
                                   systemIdentityManager
                                       ->GetCachedHostedDomainForIdentity(
                                           systemIdentity)
                                    browser:browser
                  skipBrowsingDataMigration:YES
                 mergeBrowsingDataByDefault:NO
      browsingDataMigrationDisabledByPolicy:NO
                 multiProfileForceMigration:YES];
  _managedConfirmationScreenCoordinator.delegate = self;

  [_managedConfirmationScreenCoordinator start];
  localState->SetBoolean(prefs::kMultiProfileForcedMigrationDone, false);
  base::RecordAction(base::UserMetricsAction(
      "Signin_MultiProfileForcedMigration_DialogShown"));
}

// Returns YES if the browser provider is incognito.
- (BOOL)isIncognitoBrowserProvider {
  id<BrowserProviderInterface> providerInterface =
      self.profileState.foregroundActiveScene.browserProviderInterface;
  id<BrowserProvider> presentingInterface =
      providerInterface.currentBrowserProvider;
  if (presentingInterface == providerInterface.incognitoBrowserProvider) {
    return YES;
  }
  return NO;
}

@end
