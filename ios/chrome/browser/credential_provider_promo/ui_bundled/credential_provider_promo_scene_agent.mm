// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_scene_agent.h"

#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface CredentialProviderPromoSceneAgent ()

// The PromosManager is used to register promos.
@property(nonatomic, assign) PromosManager* promosManager;

@end

@implementation CredentialProviderPromoSceneAgent

- (instancetype)initWithPromosManager:(PromosManager*)promosManager {
  if ((self = [super init])) {
    _promosManager = promosManager;
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelUnattached:
    case SceneActivationLevelDisconnected:
      // no-op.
      break;
    case SceneActivationLevelBackground:
      // no-op.
      break;
    case SceneActivationLevelForegroundInactive:
      // no-op.
      break;
    case SceneActivationLevelForegroundActive:
      bool isPromoRegistered =
          GetApplicationContext()->GetLocalState()->GetBoolean(
              prefs::kIosCredentialProviderPromoHasRegisteredWithPromoManager);
      bool shouldNotShowPromo = [self isCPEEnabled];
      if (isPromoRegistered && shouldNotShowPromo) {
        _promosManager->DeregisterPromo(
            promos_manager::Promo::CredentialProviderExtension);
        GetApplicationContext()->GetLocalState()->SetBoolean(
            prefs::kIosCredentialProviderPromoHasRegisteredWithPromoManager,
            false);
      }
      break;
  }
}

#pragma mark - Private

- (BOOL)isCPEEnabled {
  return password_manager_util::IsCredentialProviderEnabledOnStartup(
      GetApplicationContext()->GetLocalState());
}

@end
