// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/utils.h"

#import "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

bool ShouldPromoManagerDisplayPromos() {
  return GetApplicationContext()->WasLastShutdownClean();
}

bool IsUIAvailableForPromo(SceneState* scene_state) {
  // The following app & scene conditions need to be met to enable a promo's
  // display (please note the Promos Manager may still decide *not* to display a
  // promo, based on its own internal criteria):

  // (1) The profile initialization is over (the stage ProfileInitStage::kFinal
  // is reached).
  if (scene_state.profileState.initStage < ProfileInitStage::kFinal) {
    return NO;
  }

  // (2) The scene is in the foreground.
  if (scene_state.activationLevel < SceneActivationLevelForegroundActive) {
    return NO;
  }

  // (3) There is no UI blocker.
  if (scene_state.appState.currentUIBlocker) {
    return NO;
  }

  // (4) The app isn't shutting down.
  if (scene_state.appState.appIsTerminating) {
    return NO;
  }

  // (5) There are no launch intents (external intents).
  if (scene_state.startupHadExternalIntent) {
    return NO;
  }

  // Additional, sensible checks to add to minimize user annoyance:

  // (6) The user isn't currently signing in.
  if (scene_state.signinInProgress) {
    return NO;
  }

  // (7) The user isn't currently looking at a modal overlay.
  if (scene_state.presentingModalOverlay) {
    return NO;
  }

  // (8) User Policy notification has priority over showing promos.
  // This will only prevent showing a promo before policy notification but might
  // show a promo within same user session.
  DCHECK(scene_state.profileState.profile);
  ProfileIOS* profile = scene_state.profileState.profile;
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);

  // Don't show promo until auth service is initialized and we are sure that
  // there is no conflict.
  if (!auth_service) {
    return NO;
  }
  PrefService* pref_service = profile->GetPrefs();
  policy::UserCloudPolicyManager* user_policy_manager =
      profile->GetUserCloudPolicyManager();
  return !IsUserPolicyNotificationNeeded(auth_service, pref_service,
                                         user_policy_manager);
}
