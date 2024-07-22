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
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_util.h"

namespace {

// Preference key used to store which profile is current.
NSString* kIncognitoCurrentKey = @"IncognitoActive";

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

@interface SceneState ()

// Container for this object's observers.
@property(nonatomic, strong) SceneStateObserverList* observers;

// Agents attached to this scene.
@property(nonatomic, strong) NSMutableArray<id<SceneAgent>>* agents;

@end

@implementation SceneState {
  ContentVisibility _contentVisibility;
  NSString* _sceneSessionID;
}

- (instancetype)initWithAppState:(AppState*)appState {
  self = [super init];
  if (self) {
    _appState = appState;
    _observers = [SceneStateObserverList
        observersWithProtocol:@protocol(SceneStateObserver)];
    _contentVisibility = ContentVisibility::kUnknown;
    _agents = [[NSMutableArray alloc] init];
    _sceneSessionID = @"";

    // AppState might be nil in tests.
    if (appState) {
      [self addObserver:appState];
    }
  }
  return self;
}

#pragma mark - public

- (void)addObserver:(id<SceneStateObserver>)observer {
  [self.observers addObserver:observer];
}

- (void)removeObserver:(id<SceneStateObserver>)observer {
  [self.observers removeObserver:observer];
}

- (void)addAgent:(id<SceneAgent>)agent {
  DCHECK(agent);
  [self.agents addObject:agent];
  [agent setSceneState:self];
}

- (NSArray*)connectedAgents {
  return self.agents;
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

- (void)setScene:(UIWindowScene*)scene {
  _scene = scene;
  if (_scene) {
    _sceneSessionID = SessionIdentifierForScene(_scene);
  } else {
    _sceneSessionID = @"";
  }
}

- (void)setActivationLevel:(SceneActivationLevel)newLevel {
  if (_activationLevel == newLevel) {
    return;
  }
  _activationLevel = newLevel;

  [self.observers sceneState:self transitionedToActivationLevel:newLevel];
}

- (void)setUIEnabled:(BOOL)UIEnabled {
  if (_UIEnabled == UIEnabled) {
    return;
  }

  _UIEnabled = UIEnabled;
  if (UIEnabled) {
    [self.observers sceneStateDidEnableUI:self];
  } else {
    [self.observers sceneStateDidDisableUI:self];
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
    [self.observers sceneStateWillShowModalOverlay:self];
  } else {
    [self.observers sceneStateWillHideModalOverlay:self];
  }

  _presentingModalOverlay = presentingModalOverlay;

  if (!presentingModalOverlay) {
    [self.observers sceneStateDidHideModalOverlay:self];
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
    [self.observers sceneState:self hasPendingURLs:_URLContextsToOpen];
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

  [self.observers sceneState:self
      isDisplayingIncognitoContent:incognitoContentVisible];
}

- (void)setPendingUserActivity:(NSUserActivity*)pendingUserActivity {
  _pendingUserActivity = pendingUserActivity;
  [self.observers sceneState:self receivedUserActivity:pendingUserActivity];
}

- (void)setSigninInProgress:(BOOL)signinInProgress {
  DCHECK(_signinInProgress != signinInProgress);

  _signinInProgress = signinInProgress;
  if (signinInProgress) {
    [self.observers signinDidStart:self];
  } else {
    [self.observers signinDidEnd:self];
  }
}

#pragma mark - UIBlockerTarget

- (id<UIBlockerManager>)uiBlockerManager {
  return _appState;
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
                         NOTREACHED_IN_MIGRATION();
                       }];
}

#pragma mark - debug

- (NSString*)description {
  NSString* activityString = nil;
  switch (self.activationLevel) {
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

@end
