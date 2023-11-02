// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/post_restore_app_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PostRestoreAppAgent () <AppStateObserver>

// The app state for the app.
@property(nonatomic, weak, readonly) AppState* appState;

// Stores whether the IOSNewPostRestoreExperience is enabled, in either
// variation.
@property(nonatomic) BOOL featureEnabled;

// Stores whether we have pre-restore account info.
@property(nonatomic) BOOL hasAccountInfo;

// The PromosManager is used to register promos.
@property(nonatomic, assign) PromosManager* promosManager;

// The AuthenticationManager is used to reset the reauth infobar prompt.
@property(nonatomic, assign) AuthenticationService* authenticationService;

// Local state is used to retrieve and/or clear the pre-restore identity.
@property(nonatomic, assign) PrefService* localState;

// Stores the PostRestoreSignInType which can be kAlert, kFullscreen, or
// kDisabled.
@property(nonatomic)
    post_restore_signin::features::PostRestoreSignInType postRestoreSignInType;

// Returns the appropriate post restore sign-in promo, depending on which
// feature variation is enabled.
@property(readonly) promos_manager::Promo promoForEnabledFeature;

// Returns whether or not a post restore sign-in promo should be registered
// with the PromosManager.
@property(readonly) BOOL shouldRegisterPromo;

@end

@implementation PostRestoreAppAgent

#pragma mark - Initializers

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                authenticationService:
                    (AuthenticationService*)authenticationService
                           localState:(PrefService*)localState {
  DCHECK(authenticationService);

  self = [super init];
  if (self) {
    _promosManager = promosManager;
    _authenticationService = authenticationService;
    _localState = localState;
  }
  return self;
}

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (self.appState.initStage == InitStageFinal) {
    [self loadProperties];
    [self maybeRegisterPromo];
    [self.appState removeObserver:self];
    [self.appState removeAgent:self];
    _promosManager = nil;
    _authenticationService = nil;
  }
}

#pragma mark - internal

// Loads all the properties to allow this app agent to decide whether to
// register the post restore sign-in promo with the PromosManager.
- (void)loadProperties {
  _postRestoreSignInType =
      post_restore_signin::features::CurrentPostRestoreSignInType();
  _featureEnabled =
      _postRestoreSignInType !=
      post_restore_signin::features::PostRestoreSignInType::kDisabled;
  _hasAccountInfo = GetPreRestoreIdentity(_localState).has_value();
}

// Returns the correct promo type depending on which feature variation is
// enabled.
- (promos_manager::Promo)promoForEnabledFeature {
  switch (_postRestoreSignInType) {
    case post_restore_signin::features::PostRestoreSignInType::kFullscreen:
      return promos_manager::Promo::PostRestoreSignInFullscreen;
    case post_restore_signin::features::PostRestoreSignInType::kAlert:
      return promos_manager::Promo::PostRestoreSignInAlert;
    case post_restore_signin::features::PostRestoreSignInType::kDisabled:
      NOTREACHED();
      return promos_manager::Promo::PostRestoreSignInFullscreen;
  }
}

// Returns whether or not a post restore sign-in promo should be registered
// with the PromosManager.
- (BOOL)shouldRegisterPromo {
  return _featureEnabled && _hasAccountInfo && _promosManager;
}

// Register the promo with the PromosManager, if the conditions are met,
// otherwise deregister the promo.
- (void)maybeRegisterPromo {
  if (!self.shouldRegisterPromo) {
    if (_promosManager) {
      [self deregisterPromos];
    }
    if (_hasAccountInfo) {
      ClearPreRestoreIdentity(_localState);
    }
    return;
  }

  [self registerPromo];
}

// Registers the promo with the PromosManager.
- (void)registerPromo {
  DCHECK(_promosManager);
  // Disable the reauth infobar so that we don't prompt the user twice about
  // reauthenticating.
  _authenticationService->ResetReauthPromptForSignInAndSync();

  // Deregister any previously registered promos.
  [self deregisterPromos];
  _promosManager->RegisterPromoForSingleDisplay(self.promoForEnabledFeature);
}

- (void)deregisterPromos {
  DCHECK(_promosManager);
  _promosManager->DeregisterPromo(
      promos_manager::Promo::PostRestoreSignInFullscreen);
  _promosManager->DeregisterPromo(
      promos_manager::Promo::PostRestoreSignInAlert);
}

@end
