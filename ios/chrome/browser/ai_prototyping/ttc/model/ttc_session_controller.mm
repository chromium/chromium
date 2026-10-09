// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"

#import <UIKit/UIKit.h>

#import <memory>
#import <utility>

#import "base/check.h"
#import "components/ttc/app/public/error_codes.h"
#import "components/ttc/app/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_engine.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"

namespace {

// Bridge forwarding C++ `TtcConversation::Delegate` callbacks to an Objective-C
// `TTCSessionController`.
class TtcConversationDelegateBridge : public TtcConversation::Delegate {
 public:
  explicit TtcConversationDelegateBridge(TTCSessionController* controller)
      : controller_(controller) {}
  ~TtcConversationDelegateBridge() override = default;

  // `TtcConversation::Delegate` implementation:
  void OnConversationInitialized() override {
    [controller_ onSessionInitialized];
  }

  void OnConversationClosed() override { [controller_ stopSession]; }

  void OnAudioEnergyUpdated(float energy) override {
    [controller_ userAudioLevelDidUpdate:energy];
  }

  void OnConversationError(ttc::ErrorCode error) override {
    [controller_ failWithError:CreateTTCError(error)];
  }

 private:
  __weak TTCSessionController* controller_ = nil;
};

}  // namespace

@implementation TTCSessionController {
  // Declared before `_conversation` so `_conversation` is destroyed first
  // during deallocation while the delegate bridge is still alive.
  std::unique_ptr<TtcConversationDelegateBridge> _conversationDelegateBridge;
  std::unique_ptr<TtcConversation> _conversation;
  NSHashTable<id<TTCSessionControllerObserver>>* _observers;
  BOOL _fatalErrorReported;
}

- (instancetype)initWithConversation:
    (std::unique_ptr<TtcConversation>)conversation {
  CHECK(conversation);
  self = [super init];
  if (self) {
    _conversationDelegateBridge =
        std::make_unique<TtcConversationDelegateBridge>(self);
    _conversation = std::move(conversation);
    _conversation->set_delegate(_conversationDelegateBridge.get());
    _lifecycle = TTCSessionLifecycle::kInitializing;
    _observers = [NSHashTable weakObjectsHashTable];
    _fatalErrorReported = NO;
    [self registerBackgroundObserver];
  }
  return self;
}

- (instancetype)initWithBackend:(std::unique_ptr<ttc::TtcBackend>)backend {
  return [self initWithConversation:std::make_unique<TtcConversation>(
                                        [[TTCAudioEngine alloc] init],
                                        std::move(backend))];
}

- (instancetype)init {
  return [self initWithConversation:std::make_unique<TtcConversation>()];
}

- (TtcConversation*)conversation {
  return _conversation.get();
}

#pragma mark - Session Lifecycle

- (void)startSession {
  if (_lifecycle == TTCSessionLifecycle::kFinished) {
    return;
  }
  _conversation->Start();
}

- (void)onSessionInitialized {
  if (_lifecycle != TTCSessionLifecycle::kInitializing) {
    return;
  }
  [self setLifecycle:TTCSessionLifecycle::kLive];
}

- (void)stopSession {
  _conversation->Stop();
  [self setLifecycle:TTCSessionLifecycle::kFinished];
}

- (void)disconnect {
  [self stopSession];
  _conversation->Disconnect();
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
  if (_lifecycle == TTCSessionLifecycle::kFinished || _fatalErrorReported) {
    return;
  }
  const bool isFatal = IsFatalTTCError(error);
  if (isFatal) {
    _fatalErrorReported = YES;
  }
  NSHashTable<id<TTCSessionControllerObserver>>* observers = [_observers copy];
  for (id<TTCSessionControllerObserver> observer in observers) {
    if ([observer
            respondsToSelector:@selector(
                                   sessionController:didFailWithError:)]) {
      [observer sessionController:self didFailWithError:error];
    }
  }
  if (isFatal) {
    [self stopSession];
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
