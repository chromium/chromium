// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/post_restore_app_agent.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/signin_util.h"

@interface PostRestoreAppAgent () <AppStateObserver,
                                   IdentityManagerObserverBridgeDelegate>

// The app state for the app.
@property(nonatomic, weak, readonly) AppState* appState;

// Stores whether we have pre-restore account info.
@property(nonatomic, assign) BOOL hasAccountInfo;

// The PromosManager is used to register promos.
@property(nonatomic, assign) PromosManager* promosManager;

// The AuthenticationManager is used to reset the reauth infobar prompt.
@property(nonatomic, assign) AuthenticationService* authenticationService;

// Local state is used to retrieve and/or clear the pre-restore identity.
@property(nonatomic, assign) PrefService* localState;

// Returns whether or not a post restore sign-in promo should be registered
// with the PromosManager.
@property(readonly) BOOL shouldRegisterPromo;

@end

@implementation PostRestoreAppAgent {
  signin::IdentityManager* _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
}

#pragma mark - Initializers

- (instancetype)initWithPromosManager:(PromosManager*)promosManager
                authenticationService:
                    (AuthenticationService*)authenticationService
                      identityManager:(signin::IdentityManager*)identityManager
                           localState:(PrefService*)localState {
  DCHECK(authenticationService);
  DCHECK(identityManager);

  self = [super init];
  if (self) {
    _promosManager = promosManager;
    _authenticationService = authenticationService;
    _identityManager = identityManager;
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
    self.hasAccountInfo = GetPreRestoreIdentity(_localState).has_value();
    [self maybeRegisterPromo];
    // AuthenticationService is no longer needed.
    self.authenticationService = nullptr;
    [self.appState removeObserver:self];
    if (self.hasAccountInfo && _identityManager) {
      _identityObserverBridge =
          std::make_unique<signin::IdentityManagerObserverBridge>(
              _identityManager, self);
    } else {
      [self shutdown];
    }
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user adds a primary account.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (self.promosManager) {
        [self deregisterPromos];
        ClearPreRestoreIdentity(_localState);
        self.hasAccountInfo = NO;
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

#pragma mark - internal

// Returns whether or not a post restore sign-in promo should be registered
// with the PromosManager.
- (BOOL)shouldRegisterPromo {
  return _hasAccountInfo && _promosManager;
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
  CHECK(self.authenticationService == nullptr);
  [self.appState removeAgent:self];
  self.promosManager = nullptr;
  _identityManager = nullptr;
  _identityObserverBridge.reset();
}

@end
