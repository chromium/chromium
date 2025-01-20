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
#import "base/time/time.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_constants.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
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

@interface IncognitoReauthSceneAgent () <PrefObserverDelegate>

// Whether the window had incognito content (e.g. at least one open tab) upon
// backgrounding.
@property(nonatomic, assign) BOOL windowHadIncognitoContentWhenBackgrounded;

// Tracks whether the user authenticated for incognito since last launch.
@property(nonatomic, assign) BOOL authenticatedSinceLastForeground;

// Tracks whether Chrome was backgrounded for more that the required threshold
// to trigger soft lock.
@property(nonatomic, assign) BOOL backgroundedForEnoughTime;

// Container for observers.
@property(nonatomic, strong) IncognitoReauthObserverList* observers;

// Tracks the time in which Chrome was last backgrounded.
@property(nonatomic, assign) base::Time lastBackgroundedTime;

@end

@implementation IncognitoReauthSceneAgent {
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
}

@synthesize lastBackgroundedTime = _lastBackgroundedTime;

#pragma mark - class public

+ (void)registerLocalState:(PrefRegistrySimple*)registry {
  // TODO(crbug.com/370804664): Consider merging both Incognito soft lock and
  // authentication prefs into a state pref instead of two boolean prefs after
  // completing the soft lock experiment.
  registry->RegisterBooleanPref(prefs::kIncognitoAuthenticationSetting, false);
  // TODO(crbug.com/370804664): Guard pref behind a flag. Currently doing so
  // causes crashes due to unregistered pref. Needs futher investigation.
  // This pref reflects enabling Incognito Soft Lock by default for the
  // experiment to see user engagement and whether they prefer to keep it on,
  // upgrade to biometric authentication, or turn it off.
  registry->RegisterBooleanPref(prefs::kIncognitoSoftLockSetting, true);
  registry->RegisterTimePref(prefs::kLastBackgroundedTime, base::Time());
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
    } else if ([self isSoftLockFeatureEnabled] &&
               self.backgroundedForEnoughTime) {
      return IncognitoLockState::kSoftLock;
    }
  }

  return IncognitoLockState::kNone;
}

- (void)manualAuthenticationOverride {
  self.authenticatedSinceLastForeground = YES;
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
    base::UmaHistogramEnumeration(
        kIncognitoLockOverlayInteractionHistogram,
        IncognitoLockOverlayInteraction::kUnlockWithReauthButtonClicked);
    base::RecordAction(
        base::UserMetricsAction("IOS.IncognitoLock.Overlay.UnlockWithReauth"));
  } else if ([self isSoftLockFeatureEnabled]) {
    [self unlockIncognitoContentWithCompletionBlock:completion];
    base::UmaHistogramEnumeration(
        kIncognitoLockOverlayInteractionHistogram,
        IncognitoLockOverlayInteraction::kContinueInIncognitoButtonClicked);
    base::RecordAction(base::UserMetricsAction(
        "IOS.IncognitoLock.Overlay.ContinueInIncognito"));
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

- (void)setWindowHadIncognitoContentWhenBackgrounded:(BOOL)hadIncognitoContent {
  if (_windowHadIncognitoContentWhenBackgrounded == hadIncognitoContent) {
    return;
  }
  _windowHadIncognitoContentWhenBackgrounded = hadIncognitoContent;
  if ([self areLockFeaturesEnabled]) {
    [self notifyObservers];
  }
}

- (void)setBackgroundedForEnoughTime:(BOOL)backgroundedForEnoughTime {
  if (_backgroundedForEnoughTime == backgroundedForEnoughTime) {
    return;
  }

  _backgroundedForEnoughTime = backgroundedForEnoughTime;

  if ([self areLockFeaturesEnabled]) {
    [self notifyObservers];
  }
}

- (void)setLastBackgroundedTime:(base::Time)lastBackgroundedTime {
  _lastBackgroundedTime = lastBackgroundedTime;

  if (self.localState) {
    self.localState->SetTime(prefs::kLastBackgroundedTime,
                             lastBackgroundedTime);
  }
}

- (base::Time)lastBackgroundedTime {
  if (_lastBackgroundedTime.is_null() && self.localState) {
    _lastBackgroundedTime =
        self.localState->GetTime(prefs::kLastBackgroundedTime);
  }
  return _lastBackgroundedTime;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level <= SceneActivationLevelBackground) {
    [self updateWindowHasIncognitoContent:sceneState];
    [self updateBackgroundedForEnoughTimeOnBackground];
    self.authenticatedSinceLastForeground = NO;
  } else if (level >= SceneActivationLevelForegroundInactive) {
    [self updateWindowHasIncognitoContent:sceneState];
    [self updateBackgroundedForEnoughTimeOnForeground];
    // Close media presentations when the app is foregrounded rather than
    // backgrounded to avoid freezes.
    [self closeMediaPresentations];
  }

  if (IsIOSSoftLockEnabled()) {
    [self recordIncognitoLockImpressionForSceneState:sceneState];
  }
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  [self logEnabledHistogramOnce];
  if (IsIOSSoftLockEnabled()) {
    [self setUpPrefObservers];
    [self logIncognitoLockStateHistogramOnce];
    [self recordIncognitoLockImpressionForSceneState:sceneState];
  }
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  if (IsIOSSoftLockEnabled()) {
    [self tearDownPrefObservers];
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  [self notifyObservers];
}

- (void)sceneState:(SceneState*)sceneState
    isDisplayingIncognitoContent:(BOOL)level {
  if (IsIOSSoftLockEnabled()) {
    [self recordIncognitoLockImpressionForSceneState:sceneState];
  }
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

// Log Incognito lock setting state histogram to determine the feature usage.
// This is done once per app launch.
// Since this agent is created per-scene, guard it with dispatch_once.
- (void)logIncognitoLockStateHistogramOnce {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    DCHECK(self.localState)
        << "Local state is not yet available when trying to log "
           "IOS.IncognitoLockSettingStartupState This code is called too soon.";
    if ([self isReauthFeatureEnabled]) {
      base::UmaHistogramEnumeration(
          kIncognitoLockSettingStartupStateHistogram,
          IncognitoLockSettingStartupState::kHideWithReauth);
    } else if ([self isSoftLockFeatureEnabled]) {
      base::UmaHistogramEnumeration(
          kIncognitoLockSettingStartupStateHistogram,
          IncognitoLockSettingStartupState::kHideWithSoftLock);
    } else {
      base::UmaHistogramEnumeration(
          kIncognitoLockSettingStartupStateHistogram,
          IncognitoLockSettingStartupState::kDoNotHide);
    }
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
  return IsIOSSoftLockEnabled() && self.localState &&
         self.localState->GetBoolean(prefs::kIncognitoSoftLockSetting);
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

// Checks whether the window has any Incognito tabs. Called when the browser is
// backgrounded or foregrounded.
- (void)updateWindowHasIncognitoContent:(SceneState*)sceneState {
  BOOL hasIncognitoContent = YES;
  if (sceneState.browserProviderInterface.hasIncognitoBrowserProvider) {
    hasIncognitoContent =
        sceneState.browserProviderInterface.incognitoBrowserProvider.browser
            ->GetWebStateList()
            ->count() > 0;
    // If there is no tabs, act as if the user authenticated since last
    // foreground to avoid issue with multiwindows.
    if (!hasIncognitoContent) {
      self.authenticatedSinceLastForeground = YES;
    }
  }

  self.windowHadIncognitoContentWhenBackgrounded = hasIncognitoContent;

  if ([self areLockFeaturesEnabled]) {
    [self notifyObservers];
  }
}

// Stores the current timestamp when the browser is backgrounded. This happens
// only if authentication is not required as to not wrongly reset the timer.
- (void)updateBackgroundedForEnoughTimeOnBackground {
  if (!IsIOSSoftLockEnabled()) {
    return;
  }

  if (!self.isAuthenticationRequired) {
    self.lastBackgroundedTime = base::Time::Now();
    self.backgroundedForEnoughTime = NO;
  }
}

// Checks whether the browser was backgrounded for more than the required soft
// lock display time.
- (void)updateBackgroundedForEnoughTimeOnForeground {
  if (!IsIOSSoftLockEnabled()) {
    return;
  }
  if (self.lastBackgroundedTime.is_null()) {
    self.backgroundedForEnoughTime = NO;
    return;
  }

  base::TimeDelta duration = base::Time::Now() - self.lastBackgroundedTime;
  self.backgroundedForEnoughTime =
      duration >= kIOSSoftLockBackgroundThreshold.Get();
}

// Notifies the observers of changes to the state of isAuthenticationRequired.
- (void)notifyObservers {
  if (IsIOSSoftLockEnabled()) {
    [self.observers reauthAgent:self
        didUpdateIncognitoLockState:self.incognitoLockState];
  } else {
    [self.observers reauthAgent:self
        didUpdateAuthenticationRequirement:self.isAuthenticationRequired];
  }
}

// Registers observers for the relevant preferences, so that settings changes
// can be picked up in real time.
- (void)setUpPrefObservers {
  // TODO(crbug.com/370804664): Adding a DCHECK instead of a CHECK for the
  // moment as its not clear whether the localState will be available at this
  // point.
  DCHECK(self.localState);
  if (!_prefObserverBridge) {
    _prefChangeRegistrar.Init(self.localState);

    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIncognitoAuthenticationSetting, &_prefChangeRegistrar);
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kIncognitoSoftLockSetting, &_prefChangeRegistrar);
  }
}

// Removes the already setup preference observers.
- (void)tearDownPrefObservers {
  if (_prefObserverBridge) {
    _prefChangeRegistrar.RemoveAll();
    _prefObserverBridge.reset();
  }
}

// Records impressions of the Incognito lock for reauth and soft lock states.
- (void)recordIncognitoLockImpressionForSceneState:(SceneState*)sceneState {
  // sceneState.UIEnabled guarantees that sceneState.controller has been
  // initialized.
  if (sceneState.UIEnabled && sceneState.incognitoContentVisible &&
      sceneState.activationLevel == SceneActivationLevelForegroundActive) {
    switch ([self incognitoLockState]) {
      case IncognitoLockState::kNone:
        // No impression metrics to be recorded when the lock is disabled.
        break;
      case IncognitoLockState::kReauth:
        base::UmaHistogramEnumeration(
            kIncognitoLockImpressionHistogram,
            sceneState.controller.isTabGridVisible
                ? IncognitoLockImpression::kReauthLockTabGrid
                : IncognitoLockImpression::kReauthLockSingleTab);
        break;
      case IncognitoLockState::kSoftLock:
        base::UmaHistogramEnumeration(
            kIncognitoLockImpressionHistogram,
            sceneState.controller.isTabGridVisible
                ? IncognitoLockImpression::kSoftLockTabGrid
                : IncognitoLockImpression::kSoftLockSingleTab);
        break;
    }
  }
}

@end
