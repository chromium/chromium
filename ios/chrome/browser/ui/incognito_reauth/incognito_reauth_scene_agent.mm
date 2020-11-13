// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface IncognitoReauthSceneAgent () <SceneStateObserver>

// Scene state this agent serves.
@property(nonatomic, weak) SceneState* sceneState;

// Set when the scene goes foreground. Checks if any incognito tabs were open.
@property(nonatomic, assign) BOOL windowHadIncognitoContentOnForeground;

// Tracks wether the user authenticated for incognito since last launch.
@property(nonatomic, assign) BOOL authenticatedSinceLastForeground;

@end

@implementation IncognitoReauthSceneAgent

#pragma mark - class public

+ (void)registerLocalState:(PrefRegistrySimple*)registry {
  registry->RegisterBooleanPref(prefs::kIncognitoAuthenticationSetting, false);
}

#pragma mark - public

- (BOOL)isAuthenticationRequired {
  return base::FeatureList::IsEnabled(kIncognitoAuthentication) &&
         [self authEnabledInSettings] &&
         self.windowHadIncognitoContentOnForeground &&
         !self.authenticatedSinceLastForeground;
}

- (void)authenticateWithCompletion:(void (^)(BOOL))completion {
  // TODO: provide actual implementation.
  self.authenticatedSinceLastForeground = YES;
  if (completion) {
    completion(YES);
  }
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

#pragma mark - SceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  DCHECK(!_sceneState);
  _sceneState = sceneState;
  [sceneState addObserver:self];
}

#pragma mark - private

- (PrefService*)localState {
  if (!_localState) {
    _localState = GetApplicationContext()->GetLocalState();
  }
  return _localState;
}

// Convenience method to check the pref associated with the reauth setting.
- (BOOL)authEnabledInSettings {
  return self.localState->GetBoolean(prefs::kIncognitoAuthenticationSetting);
}

@end
