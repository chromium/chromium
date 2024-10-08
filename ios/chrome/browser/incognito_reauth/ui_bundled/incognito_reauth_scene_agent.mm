// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"

#import "base/check.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

@interface IncognitoReauthObserverList
    : CRBProtocolObservers <IncognitoReauthObserver>
@end
@implementation IncognitoReauthObserverList
@end

#pragma mark - IncognitoReauthSceneAgent

@interface IncognitoReauthSceneAgent ()

// Whether the window had incognito content (e.g. at least one open tab) upon
// backgrounding.
@property(nonatomic, assign) BOOL windowHadIncognitoContentWhenBackgrounded;

// Tracks whether the user authenticated for incognito since last launch.
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
  return self.incognitoLockState != IncognitoLockState::kNone;
}

- (IncognitoLockState)incognitoLockState {
  if (self.windowHadIncognitoContentWhenBackgrounded &&
      !self.authenticatedSinceLastForeground) {
    if ([self isReauthFeatureEnabled]) {
      return IncognitoLockState::kReauth;
    } else if ([self isSoftLockFeatureEnabled]) {
      return IncognitoLockState::kSoftLock;
    }
  }

  return IncognitoLockState::kNone;
}

- (void)authenticateIncognitoContent {
  [self authenticateIncognitoContentWithCompletionBlock:nil];
}

- (void)authenticateIncognitoContentWithCompletionBlock:
    (void (^)(BOOL success))completion {
  DCHECK(self.reauthModule);

  if (![self isAuthenticationRequired]) {
    if ([self areLockFeaturesEnabled]) {
      [self notifyObservers];
    }
    // If reauthentication is not required, it should be considered a success
    // for the caller, but do not update the authenticatedSinceLastForeground
    // as the authentication did not happen.
    if (completion) {
      completion(YES);
    }
    return;
  }

  if ([self isReauthFeatureEnabled]) {
    [self reauthIncognitoContentWithCompletionBlock:completion];
  } else if ([self isSoftLockFeatureEnabled]) {
    [self unlockIncognitoContentWithCompletionBlock:completion];
  }
}

- (void)addObserver:(id<IncognitoReauthObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<IncognitoReauthObserver>)observer {
  [self.observers removeObserver:observer];
}

#pragma mark - properties

- (void)setAuthenticatedSinceLastForeground:(BOOL)authenticated {
  _authenticatedSinceLastForeground = authenticated;
  if ([self areLockFeaturesEnabled]) {
    [self notifyObservers];
  }
}

- (void)updateWindowHasIncognitoContent:(SceneState*)sceneState {
  BOOL hasIncognitoContent = YES;
  if (sceneState.browserProviderInterface.hasIncognitoBrowserProvider) {
    hasIncognitoContent =
        sceneState.browserProviderInterface.incognitoBrowserProvider.browser
            ->GetWebStateList()
            ->count() > 0;
    // If there is no tabs, act as if the user authenticated since last
    // foreground to avoid issue with multiwindows.
    if (!hasIncognitoContent)
      self.authenticatedSinceLastForeground = YES;
  }

  self.windowHadIncognitoContentWhenBackgrounded = hasIncognitoContent;

  if ([self areLockFeaturesEnabled]) {
    [self notifyObservers];
  }
}

- (void)setWindowHadIncognitoContentWhenBackgrounded:(BOOL)hadIncognitoContent {
  if (_windowHadIncognitoContentWhenBackgrounded == hadIncognitoContent) {
    return;
  }
  _windowHadIncognitoContentWhenBackgrounded = hadIncognitoContent;
  if ([self areLockFeaturesEnabled]) {
    [self notifyObservers];
  }
}

- (void)notifyObservers {
  DCHECK([self areLockFeaturesEnabled]);
  [self.observers reauthAgent:self
      didUpdateAuthenticationRequirement:self.isAuthenticationRequired];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelBackground) {
    [self updateWindowHasIncognitoContent:sceneState];
    self.authenticatedSinceLastForeground = NO;
  } else if (level >= SceneActivationLevelForegroundInactive) {
    [self updateWindowHasIncognitoContent:sceneState];
    // Close media presentations when the app is foregrounded rather than
    // backgrounded to avoid freezes.
    [self closeMediaPresentations];
  }
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  [self logEnabledHistogramOnce];
}

#pragma mark - private

// Log authentication setting histogram to determine the feature usage.
// This is done once per app launch.
// Since this agent is created per-scene, guard it with dispatch_once.
- (void)logEnabledHistogramOnce {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    DCHECK(self.localState)
        << "Local state is not yet available when trying to log "
           "IOS.Incognito.BiometricAuthEnabled. This code is called too "
           "soon.";
    BOOL settingEnabled =
        self.localState &&
        self.localState->GetBoolean(prefs::kIncognitoAuthenticationSetting);
    base::UmaHistogramBoolean("IOS.Incognito.BiometricAuthEnabled",
                              settingEnabled);
  });
}

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
- (BOOL)isReauthFeatureEnabled {
  return self.localState &&
         self.localState->GetBoolean(prefs::kIncognitoAuthenticationSetting);
}

// Convenience method to check the pref associated with the soft lock setting
// and the feature flag.
- (BOOL)isSoftLockFeatureEnabled {
  // TODO(crbug.com/370804664): Add pref check when the settings page is
  // available.
  return IsIOSSoftLockEnabled();
}

// Convenience method to check whether any of the locking features are enabled.
- (BOOL)areLockFeaturesEnabled {
  return [self isReauthFeatureEnabled] || [self isSoftLockFeatureEnabled];
}

// Closes the media presentations to avoid having the fullscreen video on
// top of the blocker.
- (void)closeMediaPresentations {
  if (![self areLockFeaturesEnabled]) {
    return;
  }

  Browser* browser =
      self.sceneState.browserProviderInterface.incognitoBrowserProvider.browser;
  if (browser) {
    if (browser->GetWebStateList() &&
        browser->GetWebStateList()->GetActiveWebState()) {
      browser->GetWebStateList()
          ->GetActiveWebState()
          ->CloseMediaPresentations();
    }
  }
}

// Marks the scene as authenticated and, upon completion, will notify observers
// and call the completion block (passing authentication result).
- (void)unlockIncognitoContentWithCompletionBlock:
    (void (^)(BOOL success))completion {
  self.authenticatedSinceLastForeground = YES;
  if (completion) {
    completion(YES);
  }
}

// Requests authentication and marks the scene as authenticated until the next
// scene foregrounding.
// The authentication will require user interaction. Upon completion, will
// notify observers and call the completion block (passing authentication
// result).
- (void)reauthIncognitoContentWithCompletionBlock:
    (void (^)(BOOL success))completion {
  base::RecordAction(base::UserMetricsAction(
      "MobileIncognitoBiometricAuthenticationRequested"));

  NSString* authReason = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_SYSTEM_DIALOG_REASON,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));

  __weak IncognitoReauthSceneAgent* weakSelf = self;
  void (^completionHandler)(ReauthenticationResult) =
      ^(ReauthenticationResult result) {
        BOOL success = (result == ReauthenticationResult::kSuccess);
        base::UmaHistogramBoolean(
            "IOS.Incognito.BiometricReauthAttemptSuccessful", success);

        weakSelf.authenticatedSinceLastForeground = success;
        if (completion) {
          completion(success);
        }
      };
  [self.reauthModule attemptReauthWithLocalizedReason:authReason
                                 canReusePreviousAuth:false
                                              handler:completionHandler];
}

@end
