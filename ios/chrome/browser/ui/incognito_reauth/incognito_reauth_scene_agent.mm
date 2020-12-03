// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"

#include "base/check.h"
#import "base/ios/crb_protocol_observers.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface IncognitoReauthObserverList
    : CRBProtocolObservers <IncognitoReauthObserver>
@end
@implementation IncognitoReauthObserverList
@end

#pragma mark - IncognitoReauthSceneAgent

@interface IncognitoReauthSceneAgent ()

// Set when the scene goes foreground. Checks if any incognito tabs were open.
@property(nonatomic, assign) BOOL windowHadIncognitoContentOnForeground;

// Tracks wether the user authenticated for incognito since last launch.
@property(nonatomic, assign) BOOL authenticatedSinceLastForeground;

// Container for observers.
@property(nonatomic, strong) IncognitoReauthObserverList* observers;

@end

@implementation IncognitoReauthSceneAgent

#pragma mark - class public

+ (void)registerLocalState:(PrefRegistrySimple*)registry {
  registry->RegisterBooleanPref(prefs::kIncognitoAuthenticationSetting, false);
}

#pragma mark - public

- (instancetype)initWithReauthModule:
    (id<ReauthenticationProtocol>)reauthModule {
  self = [super init];
  if (self) {
    DCHECK(reauthModule);
    _reauthModule = reauthModule;
    _observers = [IncognitoReauthObserverList
        observersWithProtocol:@protocol(IncognitoReauthObserver)];
  }
  return self;
}

- (BOOL)isAuthenticationRequired {
  return [self featureEnabled] && self.windowHadIncognitoContentOnForeground &&
         !self.authenticatedSinceLastForeground;
}

- (void)authenticateIncognitoContent {
  DCHECK(self.reauthModule);

  if (!self.isAuthenticationRequired) {
    [self notifyObservers];
    return;
  }

  __weak IncognitoReauthSceneAgent* weakSelf = self;
  // TODO(crbug.com/1138892): add localized text
  [self.reauthModule
      attemptReauthWithLocalizedReason:
          @"[Test String] Authenticate for incognito access"
                  canReusePreviousAuth:false
                               handler:^(ReauthenticationResult result) {
                                 BOOL success =
                                     (result ==
                                      ReauthenticationResult::kSuccess);
                                 weakSelf.authenticatedSinceLastForeground =
                                     success;
                               }];
}

- (void)addObserver:(id<IncognitoReauthObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<IncognitoReauthObserver>)observer {
  [self.observers removeObserver:observer];
}

#pragma mark properties

- (void)setAuthenticatedSinceLastForeground:(BOOL)authenticated {
  _authenticatedSinceLastForeground = authenticated;
  if (self.featureEnabled) {
    [self notifyObservers];
  }
}

- (void)setWindowHadIncognitoContentOnForeground:(BOOL)hadIncognitoContent {
  _windowHadIncognitoContentOnForeground = hadIncognitoContent;
  if (self.featureEnabled) {
    [self notifyObservers];
  }
}

- (void)notifyObservers {
  DCHECK(self.featureEnabled);
  [self.observers reauthAgent:self
      didUpdateAuthenticationRequirement:self.isAuthenticationRequired];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelBackground) {
    self.authenticatedSinceLastForeground = NO;
  }

  if (level >= SceneActivationLevelForegroundInactive) {
    if (sceneState.interfaceProvider.hasIncognitoInterface) {
      self.windowHadIncognitoContentOnForeground =
          sceneState.interfaceProvider.incognitoInterface.browser
              ->GetWebStateList()
              ->count() > 0;
    } else {
      self.windowHadIncognitoContentOnForeground = NO;
    }
  }
}

#pragma mark - private

- (PrefService*)localState {
  if (!_localState) {
    if (!GetApplicationContext()) {
      // This is called before application context was initialized.
      return nil;
    }
    _localState = GetApplicationContext()->GetLocalState();
  }
  return _localState;
}

// Convenience method to check the pref associated with the reauth setting and
// the feature flag.
- (BOOL)featureEnabled {
  return base::FeatureList::IsEnabled(kIncognitoAuthentication) &&
         self.localState &&
         self.localState->GetBoolean(prefs::kIncognitoAuthenticationSetting);
}

@end
