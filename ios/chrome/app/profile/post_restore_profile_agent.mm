// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/post_restore_profile_agent.h"

#import "base/memory/raw_ptr.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"

@interface PostRestoreProfileAgent () <IdentityManagerObserverBridgeDelegate>
@end

@implementation PostRestoreProfileAgent {
  // The identity manager.
  raw_ptr<signin::IdentityManager> _identityManager;

  // The PromosManager used to register promos.
  raw_ptr<PromosManager> _promosManager;

  // Profile pref service used to retrieve and/or clear the pre-restore
  // identity.
  raw_ptr<PrefService> _prefService;

  // Observes changes in identity.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;

  // Stores whether we have pre-restore account info.
  BOOL _hasAccountInfo;
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  switch (nextInitStage) {
    case ProfileInitStage::kProfileLoaded: {
      ProfileIOS* profile = self.profileState.profile;
      _promosManager = PromosManagerFactory::GetForProfile(profile);
      _identityManager = IdentityManagerFactory::GetForProfile(profile);
      _prefService = profile->GetPrefs();
      break;
    }
    case ProfileInitStage::kFinal: {
      _hasAccountInfo = GetPreRestoreIdentity(_prefService).has_value();
      [self maybeRegisterPromo];
      if (_hasAccountInfo && _identityManager) {
        _identityObserverBridge =
            std::make_unique<signin::IdentityManagerObserverBridge>(
                _identityManager, self);
      } else {
        [self shutdown];
      }
      break;
    }
    default:
      // Nothing to do for other stages.
      break;
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user adds a primary account.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (_promosManager) {
        [self deregisterPromos];
        ClearPreRestoreIdentity(_prefService);
        _hasAccountInfo = NO;
        [self shutdown];
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

- (void)onIdentityManagerShutdown:(signin::IdentityManager*)identityManager {
  [self shutdown];
}

#pragma mark - Private

// Register the promo with the PromosManager, if the conditions are met,
// otherwise deregister the promo.
- (void)maybeRegisterPromo {
  if (_promosManager && _hasAccountInfo) {
    [self registerPromo];
  } else if (_promosManager) {
    [self deregisterPromos];
  } else if (_hasAccountInfo) {
    ClearPreRestoreIdentity(_prefService);
  }
}

// Registers the promo with the PromosManager.
- (void)registerPromo {
  DCHECK(_promosManager);
  DCHECK(self.profileState.profile);
  // Disable the reauth infobar so that we don't prompt the user twice about
  // reauthenticating.
  AuthenticationServiceFactory::GetForProfile(self.profileState.profile)
      ->ResetReauthPromptForSignInAndSync();

  // Deregister any previously registered promos.
  [self deregisterPromos];
  _promosManager->RegisterPromoForSingleDisplay(
      promos_manager::Promo::PostRestoreSignInAlert);
}

// Deregisters the promo with the PromosManager.
- (void)deregisterPromos {
  DCHECK(_promosManager);
  _promosManager->DeregisterPromo(
      promos_manager::Promo::PostRestoreSignInFullscreen);
  _promosManager->DeregisterPromo(
      promos_manager::Promo::PostRestoreSignInAlert);
}

// Stops observers and clears pointers.
- (void)shutdown {
  [self.profileState removeAgent:self];
  _promosManager = nullptr;
  _identityManager = nullptr;
  _identityObserverBridge.reset();
}

@end
