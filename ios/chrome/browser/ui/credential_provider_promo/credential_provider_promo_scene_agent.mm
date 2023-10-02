// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_scene_agent.h"

#import "components/password_manager/core/browser/password_manager_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/credential_provider_promo/model/features.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

@interface CredentialProviderPromoSceneAgent ()

// The PromosManager is used to register promos.
@property(nonatomic, assign) PromosManager* promosManager;

// The PrefService is used to retrieve user pref.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation CredentialProviderPromoSceneAgent

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                          prefService:(PrefService*)prefService {
  if (self = [super init]) {
    _promosManager = promosManager;
    _prefService = prefService;
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
      bool shouldNotShowPromo =
          !IsCredentialProviderExtensionPromoEnabled() || [self isCPEEnabled];
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
  DCHECK(_prefService);
  return password_manager_util::IsCredentialProviderEnabledOnStartup(
      _prefService);
}

@end
