// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"

@interface TTCSessionController () <TTCConversationDelegate>
@end

@implementation TTCSessionController {
  NSHashTable<id<TTCSessionControllerObserver>>* _observers;
}

- (instancetype)initWithConversation:(TTCConversation*)conversation {
  CHECK(conversation);
  self = [super init];
  if (self) {
    _conversation = conversation;
    _conversation.delegate = self;
    _lifecycle = TTCSessionLifecycle::kInitializing;
    _observers = [NSHashTable weakObjectsHashTable];
    [self registerBackgroundObserver];
  }
  return self;
}

- (instancetype)init {
  return [self initWithConversation:[[TTCConversation alloc] init]];
}

#pragma mark - Session Lifecycle

- (void)startSession {
  if (_lifecycle == TTCSessionLifecycle::kFinished) {
    return;
  }
  [_conversation start];
}

- (void)onSessionInitialized {
  if (_lifecycle != TTCSessionLifecycle::kInitializing) {
    return;
  }
  [self setLifecycle:TTCSessionLifecycle::kLive];
}

- (void)stopSession {
  [_conversation stop];
  [self setLifecycle:TTCSessionLifecycle::kFinished];
}

- (void)disconnect {
  [self stopSession];
  [_conversation disconnect];
  [self removeBackgroundObserver];
  [_observers removeAllObjects];
}

- (void)setLifecycle:(TTCSessionLifecycle)lifecycle {
  if (_lifecycle == lifecycle || _lifecycle == TTCSessionLifecycle::kFinished) {
    return;
  }
  _lifecycle = lifecycle;
  NSHashTable<id<TTCSessionControllerObserver>>* observers = [_observers copy];
  for (id<TTCSessionControllerObserver> observer in observers) {
    if ([observer
            respondsToSelector:@selector(
                                   sessionController:didChangeLifecycle:)]) {
      [observer sessionController:self didChangeLifecycle:_lifecycle];
    }
  }
}

#pragma mark - TTCConversationDelegate

- (void)conversation:(TTCConversation*)conversation
    didUpdateAudioEnergy:(float)energy {
  [self userAudioLevelDidUpdate:energy];
}

- (void)conversation:(TTCConversation*)conversation
    didEncounterError:(NSError*)error {
  [self failWithError:error];
}

#pragma mark - Audio & Errors

- (void)userAudioLevelDidUpdate:(float)audioLevel {
  if (![NSThread isMainThread]) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self userAudioLevelDidUpdate:audioLevel];
    });
    return;
  }
  if (_lifecycle == TTCSessionLifecycle::kFinished || !_observers.count) {
    return;
  }
  NSHashTable<id<TTCSessionControllerObserver>>* observers = [_observers copy];
  for (id<TTCSessionControllerObserver> observer in observers) {
    if ([observer
            respondsToSelector:@selector(
                                   sessionController:didUpdateAudioLevel:)]) {
      [observer sessionController:self didUpdateAudioLevel:audioLevel];
    }
  }
}

- (void)failWithError:(NSError*)error {
  if (![NSThread isMainThread]) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self failWithError:error];
    });
    return;
  }
  if (_lifecycle == TTCSessionLifecycle::kFinished || !_observers.count) {
    return;
  }
  NSHashTable<id<TTCSessionControllerObserver>>* observers = [_observers copy];
  for (id<TTCSessionControllerObserver> observer in observers) {
    if ([observer
            respondsToSelector:@selector(
                                   sessionController:didFailWithError:)]) {
      [observer sessionController:self didFailWithError:error];
    }
  }
}

#pragma mark - Observers

- (void)addObserver:(id<TTCSessionControllerObserver>)observer {
  if (!observer) {
    return;
  }
  [_observers addObject:observer];
}

- (void)removeObserver:(id<TTCSessionControllerObserver>)observer {
  if (!observer) {
    return;
  }
  [_observers removeObject:observer];
}

#pragma mark - Private

- (void)registerBackgroundObserver {
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(applicationDidEnterBackground)
             name:UIApplicationDidEnterBackgroundNotification
           object:nil];
}

- (void)applicationDidEnterBackground {
  [self stopSession];
}

- (void)removeBackgroundObserver {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidEnterBackgroundNotification
              object:nil];
}

@end
