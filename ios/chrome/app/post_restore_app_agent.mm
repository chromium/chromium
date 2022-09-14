// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/post_restore_app_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PostRestoreAppAgent () <AppStateObserver>

// The app state for the app.
@property(nonatomic, weak, readonly) AppState* appState;

// Stores the pre-restore account info, if available.
@property(nonatomic) absl::optional<AccountInfo> accountInfo;

// Stores whether the IOSNewPostRestoreExperience is enabled, in either
// variation.
@property(nonatomic) BOOL featureEnabled;

// Stores whether we have pre-restore account info.
@property(nonatomic) BOOL hasAccountInfo;

// Stores whether this is the first session after a device restore.
@property(nonatomic) BOOL isFirstSessionAfterDeviceRestore;

// Stores the PostRestoreSignInType which can be kAlert, kFullscreen, or
// kDisabled.
@property(nonatomic)
    post_restore_signin::features::PostRestoreSignInType postRestoreSignInType;

// Returns the appropriate post restore sign-in promo, depending on which
// feature variation is enabled.
@property(readonly) promos_manager::Promo promoForEnabledFeature;

// Stores the PromosManager, which is used to register the post restore
// sign-in promo, when appropriate.
@property(nonatomic) PromosManager* promosManager;

// Returns whether or not a post restore sign-in promo should be registered
// with the PromosManager.
@property(readonly) BOOL shouldRegisterPromo;

@end

@implementation PostRestoreAppAgent

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
  _isFirstSessionAfterDeviceRestore =
      IsFirstSessionAfterDeviceRestore() == signin::Tribool::kTrue;
  _hasAccountInfo = GetPreRestoreIdentity().has_value();
  _promosManager = GetApplicationContext()->GetPromosManager();
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
  return _isFirstSessionAfterDeviceRestore && _featureEnabled &&
         _hasAccountInfo && _promosManager;
}

// Register the promo with the PromosManager, if the conditions are met.
- (void)maybeRegisterPromo {
  if (!self.shouldRegisterPromo) {
    return;
  }

  [self registerPromo];
}

// Registers the promo with the PromosManager.
- (void)registerPromo {
  DCHECK(_promosManager);
  _promosManager->RegisterPromoForSingleDisplay(self.promoForEnabledFeature);
}

@end
