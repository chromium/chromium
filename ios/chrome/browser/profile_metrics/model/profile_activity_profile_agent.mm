// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile_metrics/model/profile_activity_profile_agent.h"

#import "base/time/time.h"
#import "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@implementation ProfileActivityProfileAgent

#pragma mark - Private methods

- (void)recordActivationForSceneState:(SceneState*)sceneState {
  ProfileIOS* profile = sceneState.profileState.profile;

  // Update the ProfileIOS's last-active time stored in the preferences.
  GetApplicationContext()
      ->GetProfileManager()
      ->GetProfileAttributesStorage()
      ->UpdateAttributesForProfileWithName(
          profile->GetProfileName(),
          base::BindOnce([](ProfileAttributesIOS attr) {
            attr.SetLastActiveTime(base::Time::Now());
            return attr;
          }));

  // Update the primary account's last-active time (if there is a primary
  // account).
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  signin::ActivePrimaryAccountsMetricsRecorder* activeAccountsTracker =
      GetApplicationContext()->GetActivePrimaryAccountsMetricsRecorder();
  // IdentityManager is null for incognito profiles.
  if (activeAccountsTracker && identityManager &&
      identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    CoreAccountInfo accountInfo =
        identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    activeAccountsTracker->MarkAccountAsActiveNow(accountInfo.gaia);
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  DCHECK_GE(self.profileState.initStage, ProfileInitStage::kUIReady);
  if (level == SceneActivationLevelForegroundActive) {
    [self recordActivationForSceneState:sceneState];
  }
}

@end
