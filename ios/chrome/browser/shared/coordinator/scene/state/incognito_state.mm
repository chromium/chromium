// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"

#import "base/apple/foundation_util.h"
#import "base/ios/crb_protocol_observers.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

namespace {

// Preference key used to store which profile is current.
NSString* const kIncognitoCurrentKey = @"IncognitoActive";

}  // namespace

@interface IncognitoStateObserverList
    : CRBProtocolObservers <IncognitoStateObserver>
@end
@implementation IncognitoStateObserverList
@end

@implementation IncognitoState {
  IncognitoStateObserverList* _observers;
}

- (instancetype)initWithSceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    _observers = [IncognitoStateObserverList
        observersWithProtocol:@protocol(IncognitoStateObserver)];
    _sceneState = sceneState;

    const BOOL incognitoContentVisible = [base::apple::ObjCCast<NSNumber>(
        [_sceneState sessionObjectForKey:kIncognitoCurrentKey]) boolValue];
    self.incognitoContentVisible = incognitoContentVisible;
  }
  return self;
}

- (void)addObserver:(id<IncognitoStateObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<IncognitoStateObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible {
  if (_incognitoContentVisible == incognitoContentVisible) {
    return;
  }
  _incognitoContentVisible = incognitoContentVisible;
  if (incognitoContentVisible) {
    [_observers willEnterIncognitoForState:self];
  } else {
    [_observers willExitIncognitoForState:self];
  }
  [self.sceneState setSessionObject:@(incognitoContentVisible)
                             forKey:kIncognitoCurrentKey];
}

@end
