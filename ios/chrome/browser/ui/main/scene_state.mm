// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_state.h"

#import "base/ios/crb_protocol_observers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

@implementation SceneState
@synthesize window = _window;

- (instancetype)initWithAppState:(AppState*)appState {
  self = [super init];
  if (self) {
    _appState = appState;
    _observers = [SceneStateObserverList
        observersWithProtocol:@protocol(SceneStateObserver)];
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

- (void)setWindow:(UIWindow*)window {
  if (IsSceneStartupSupported()) {
    // No need to set anything, instead the getter is backed by scene.windows
    // property.
    return;
  }
  _window = window;
}

- (UIWindow*)window {
  if (IsSceneStartupSupported()) {
    UIWindow* mainWindow = nil;
    if (@available(ios 13, *)) {
      for (UIWindow* window in self.scene.windows) {
        if ([window isKindOfClass:[ChromeOverlayWindow class]]) {
          mainWindow = window;
        }
      }
    }
    return mainWindow;
  }
  return _window;
}

- (NSString*)sceneSessionID {
  NSString* sessionID = nil;
  if (@available(ios 13, *)) {
    sessionID = _scene.session.persistentIdentifier;
  }
  return sessionID;
}

- (void)setActivationLevel:(SceneActivationLevel)newLevel {
  if (_activationLevel == newLevel) {
    return;
  }
  _activationLevel = newLevel;

  [self.observers sceneState:self transitionedToActivationLevel:newLevel];
}

- (void)setHasInitializedUI:(BOOL)hasInitializedUI {
  if (_hasInitializedUI == hasInitializedUI) {
    return;
  }
  _hasInitializedUI = hasInitializedUI;
  if (hasInitializedUI) {
    [self.observers sceneStateHasInitializedUI:self];
  }
}

- (id<BrowserInterfaceProvider>)interfaceProvider {
  return self.controller.interfaceProvider;
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

- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible {
  if (incognitoContentVisible == _incognitoContentVisible) {
    return;
  }
  _incognitoContentVisible = incognitoContentVisible;
  [self.observers sceneState:self
      isDisplayingIncognitoContent:incognitoContentVisible];
}

- (void)setPendingUserActivity:(NSUserActivity*)pendingUserActivity {
  _pendingUserActivity = pendingUserActivity;
  [self.observers sceneState:self receivedUserActivity:pendingUserActivity];
}

#pragma mark - UIBlockerTarget

- (id<UIBlockerManager>)uiBlockerManager {
  return _appState;
}

- (void)bringBlockerToFront:(UIScene*)requestingScene API_AVAILABLE(ios(13)) {
  if (!IsMultipleScenesSupported()) {
    return;
  }
  if (@available(iOS 13, *)) {
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
}

#pragma mark - debug

- (NSString*)description {
  NSString* activityString = nil;
  switch (self.activationLevel) {
    case SceneActivationLevelUnattached: {
      activityString = @"Unattached";
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

@end
