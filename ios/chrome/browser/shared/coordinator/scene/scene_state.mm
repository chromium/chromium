// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

#import "base/apple/foundation_util.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_in_progress.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_util.h"
#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_window.h"

namespace {

// Preference key used to store which profile is current.
NSString* const kIncognitoCurrentKey = @"IncognitoActive";

// Represents the state of the -[SceneState incognitoContentVisible] property
// that is saved in session storage (and thus unknown during app startup and
// will be lazily loaded when needed).
enum class ContentVisibility {
  kUnknown,
  kRegular,
  kIncognito,
};

// Returns the value of ContentVisibility depending on `isIncognito` boolean.
ContentVisibility ContentVisibilityForIncognito(BOOL isIncognito) {
  return isIncognito ? ContentVisibility::kIncognito
                     : ContentVisibility::kRegular;
}

}  // namespace

@interface SceneStateObserverList : CRBProtocolObservers <SceneStateObserver>
@end

@implementation SceneStateObserverList
@end

#pragma mark - SceneState

@interface SceneState () <SignInInProgressAudience>

@end

@implementation SceneState {
  // Cache the session identifier.
  std::string _sceneSessionID;

  // The AppState passed to the initializer.
  AppState* _appState;

  // Container for this object's observers.
  SceneStateObserverList* _observers;

  // Agents attached to this scene.
  NSMutableArray<id<SceneAgent>>* _agents;

  // The state of the -incognitoContentVisible property.
  ContentVisibility _contentVisibility;

  // The current value of -activationLevel.
  SceneActivationLevel _activationLevel;

  // A UIBlocker that blocks other scenes if and only if a sign in is in
  // progress.
  std::unique_ptr<ScopedUIBlocker> _signinUIBlocker;

  // The number of sign-in in progress. This include both the authentication
  // flow and the sign-in prompt UI.
  // In normal usage, this number can be greater than one because a signin
  // coordinator may open another signin coordinator. It also occurs that two
  // signin coordinator are started simultaneously from different screen, for
  // example due to simultaneous tap on a IPH signin promo and on the NTP’s
  // identity disc.
  NSInteger _numberOfSigninInProgress;
}

- (instancetype)initWithAppState:(AppState*)appState {
  self = [super init];
  if (self) {
    _appState = appState;
    _observers = [SceneStateObserverList
        observersWithProtocol:@protocol(SceneStateObserver)];
    _contentVisibility = ContentVisibility::kUnknown;
    _agents = [[NSMutableArray alloc] init];

    // AppState might be nil in tests.
    if (appState) {
      [self addObserver:appState];
    }
  }
  return self;
}

#pragma mark - public

- (void)addObserver:(id<SceneStateObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<SceneStateObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)addAgent:(id<SceneAgent>)agent {
  DCHECK(agent);
  [_agents addObject:agent];
  [agent setSceneState:self];
}

- (NSArray*)connectedAgents {
  return _agents;
}

- (std::unique_ptr<SigninInProgress>)createSigninInProgress {
  return std::make_unique<SigninInProgress>(self);
}

#pragma mark - Setters & Getters.

- (UIWindow*)window {
  UIWindow* mainWindow = nil;
  for (UIWindow* window in self.scene.windows) {
    if ([window isKindOfClass:[ChromeOverlayWindow class]]) {
      mainWindow = window;
    }
  }
  return mainWindow;
}

- (const std::string&)sceneSessionID {
  return _sceneSessionID;
}

- (void)setScene:(UIWindowScene*)scene {
  _scene = scene;
  if (_scene) {
    _sceneSessionID = SessionIdentifierForScene(_scene);
  } else {
    _sceneSessionID.clear();
  }
}

- (void)setActivationLevel:(SceneActivationLevel)newLevel {
  if (_activationLevel == newLevel) {
    return;
  }
  _activationLevel = newLevel;

  [_observers sceneState:self transitionedToActivationLevel:newLevel];
}

- (void)setUIEnabled:(BOOL)UIEnabled {
  if (_UIEnabled == UIEnabled) {
    return;
  }

  _UIEnabled = UIEnabled;
  if (UIEnabled) {
    [_observers sceneStateDidEnableUI:self];
  } else {
    [_observers sceneStateDidDisableUI:self];
  }
}

- (id<BrowserProviderInterface>)browserProviderInterface {
  return self.controller.browserProviderInterface;
}

- (void)setPresentingModalOverlay:(BOOL)presentingModalOverlay {
  if (_presentingModalOverlay == presentingModalOverlay) {
    return;
  }
  if (presentingModalOverlay) {
    [_observers sceneStateWillShowModalOverlay:self];
  } else {
    [_observers sceneStateWillHideModalOverlay:self];
  }

  _presentingModalOverlay = presentingModalOverlay;

  if (!presentingModalOverlay) {
    [_observers sceneStateDidHideModalOverlay:self];
  }
}

- (void)setURLContextsToOpen:(NSSet<UIOpenURLContext*>*)URLContextsToOpen {
  if (_URLContextsToOpen == nil || URLContextsToOpen == nil) {
    _URLContextsToOpen = URLContextsToOpen;
  } else {
    _URLContextsToOpen =
        [_URLContextsToOpen setByAddingObjectsFromSet:URLContextsToOpen];
  }
  if (_URLContextsToOpen) {
    [_observers sceneState:self hasPendingURLs:_URLContextsToOpen];
  }
}

- (BOOL)incognitoContentVisible {
  switch (_contentVisibility) {
    case ContentVisibility::kRegular:
      return NO;

    case ContentVisibility::kIncognito:
      return YES;

    case ContentVisibility::kUnknown: {
      const BOOL incognitoContentVisible = [base::apple::ObjCCast<NSNumber>(
          [self sessionObjectForKey:kIncognitoCurrentKey]) boolValue];

      _contentVisibility =
          ContentVisibilityForIncognito(incognitoContentVisible);
      DCHECK_NE(_contentVisibility, ContentVisibility::kUnknown);

      return incognitoContentVisible;
    }
  }
}

- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible {
  const ContentVisibility contentVisibility =
      ContentVisibilityForIncognito(incognitoContentVisible);
  if (contentVisibility == _contentVisibility) {
    return;
  }

  _contentVisibility = contentVisibility;

  [self setSessionObject:@(incognitoContentVisible)
                  forKey:kIncognitoCurrentKey];

  [_observers sceneState:self
      isDisplayingIncognitoContent:incognitoContentVisible];
}

- (void)setPendingUserActivity:(NSUserActivity*)pendingUserActivity {
  _pendingUserActivity = pendingUserActivity;
  [_observers sceneState:self receivedUserActivity:pendingUserActivity];
}

- (BOOL)signinInProgress {
  return _numberOfSigninInProgress > 0;
}

- (void)setProfileState:(ProfileState*)profileState {
  _profileState = profileState;
  [_observers sceneState:self profileStateConnected:_profileState];
}

#pragma mark - UIBlockerTarget

- (BOOL)isUIBlocked {
  return _presentingModalOverlay;
}

- (id<UIBlockerManager>)uiBlockerManagerForExtent:(UIBlockerExtent)extent {
  switch (extent) {
    case UIBlockerExtent::kProfile:
      return _profileState;
    case UIBlockerExtent::kApplication:
      return _appState;
  }
}

- (void)bringBlockerToFront:(UIScene*)requestingScene {
  if (!base::ios::IsMultipleScenesSupported()) {
    return;
  }
  UISceneActivationRequestOptions* options =
      [[UISceneActivationRequestOptions alloc] init];
  options.requestingScene = requestingScene;

  [[UIApplication sharedApplication]
      requestSceneSessionActivation:self.scene.session
                       userActivity:nil
                            options:options
                       errorHandler:^(NSError* error) {
                         LOG(ERROR) << base::SysNSStringToUTF8(
                             error.localizedDescription);
                         NOTREACHED();
                       }];
}

#pragma mark - debug

- (NSString*)description {
  NSString* activityString = nil;
  switch (_activationLevel) {
    case SceneActivationLevelUnattached: {
      activityString = @"Unattached";
      break;
    }

    case SceneActivationLevelDisconnected: {
      activityString = @"Disconnected";
      break;
    }

    case SceneActivationLevelBackground: {
      activityString = @"Background";
      break;
    }
    case SceneActivationLevelForegroundInactive: {
      activityString = @"Foreground, Inactive";
      break;
    }
    case SceneActivationLevelForegroundActive: {
      activityString = @"Active";
      break;
    }
  }

  return
      [NSString stringWithFormat:@"SceneState %p (%@)", self, activityString];
}

#pragma mark - Session scoped defaults.

// Helper methods to get/set values that are "per-scene" (such as whether the
// incognito or regular UI is presented, ...). Those methods store/fetch the
// values from -userInfo property of UISceneSession for devices that support
// multi-window or in NSUserDefaults for other device.
//
// The reason the values are not always stored in UISceneSession -userInfo is
// that iOS consider that the "swipe gesture" can mean "close the window" even
// on device that do not support multi-window (such as iPhone) if multi-window
// support is enabled. As enabling the support is done in the Info.plist and
// Chrome does not want to distribute a different app to phones and tablets,
// this means that on iPhone the scene may be closed by the OS and the session
// destroyed. On device that support multi-window, the user has the option to
// re-open the window via a shortcut presented by the OS, but there is no such
// options for device that do not support multi-window.
//
// Finally, the methods also support moving the value from NSUserDefaults to
// UISceneSession -userInfo as required when Chrome is updated from an old
// version to one where multi-window is enabled (or when the users upgrade
// their devices).
//
// The heuristic is:
// -  if the device does not support multi-window, NSUserDefaults is used,
// -  otherwise, the value is first looked up in UISceneSession -userInfo,
//    if present, it is used (and any copy in NSUserDefaults is deleted),
//    if not present, the value is looked in NSUserDefaults.

- (NSObject*)sessionObjectForKey:(NSString*)key {
  if (base::ios::IsMultipleScenesSupported()) {
    NSObject* value = [_scene.session.userInfo objectForKey:key];
    if (value) {
      NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
      if ([userDefaults objectForKey:key]) {
        [userDefaults removeObjectForKey:key];
        [userDefaults synchronize];
      }
      return value;
    }
  }

  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  return [userDefaults objectForKey:key];
}

- (void)setSessionObject:(NSObject*)object forKey:(NSString*)key {
  if (base::ios::IsMultipleScenesSupported()) {
    NSMutableDictionary<NSString*, id>* userInfo =
        [NSMutableDictionary dictionaryWithDictionary:_scene.session.userInfo];
    [userInfo setObject:object forKey:key];
    _scene.session.userInfo = userInfo;
    return;
  }

  NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
  [userDefaults setObject:object forKey:key];
  [userDefaults synchronize];
}

#pragma mark - SignInInProgressAudience

- (void)signInStarted {
  if (_numberOfSigninInProgress == 0) {
    [_observers signinDidStart:self];
    CHECK(!_signinUIBlocker, base::NotFatalUntil::M146);
    _signinUIBlocker = std::make_unique<ScopedUIBlocker>(self);
  } else {
    CHECK(_signinUIBlocker, base::NotFatalUntil::M146);
  }
  _numberOfSigninInProgress++;
}

- (void)signinFinished {
  _numberOfSigninInProgress--;
  CHECK_GE(_numberOfSigninInProgress, 0, base::NotFatalUntil::M146);
  if (_numberOfSigninInProgress < 0) {
    _numberOfSigninInProgress = 0;
  }
  if (_numberOfSigninInProgress > 0) {
    return;
  }
  _signinUIBlocker.reset();
  [_observers signinDidEnd:self];
}

@end
