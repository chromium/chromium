// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"

#import "base/callback_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"

namespace {

// Error message displayed when the TTC service is unavailable.
NSString* const kServiceUnavailableError = @"TTC service is unavailable";

// Default error message displayed when a session error occurs without a
// specific description.
NSString* const kDefaultSessionError = @"Voice session encountered an error";

// Converts a `TTCSessionLifecycle` to the corresponding `TTCSessionUIState`.
TTCSessionUIState SessionUIStateForLifecycle(TTCSessionLifecycle lifecycle) {
  switch (lifecycle) {
    case TTCSessionLifecycle::kInitializing:
      return TTCSessionUIState::kConnecting;
    case TTCSessionLifecycle::kLive:
      return TTCSessionUIState::kListening;
    case TTCSessionLifecycle::kFinished:
      return TTCSessionUIState::kIdle;
  }
}

}  // namespace

@interface TTCMediator () <TTCSessionControllerObserver>
@end

@implementation TTCMediator {
  // Current voice session lifecycle state (Idle, Connecting, Listening, Error).
  TTCSessionUIState _currentState;

  // TTC keyed service for the profile.
  raw_ptr<TTCKeyedService> _ttcService;

  // Subscription observing state changes in TTCKeyedService.
  base::CallbackListSubscription _stateChangeSubscription;

  // Weak reference to the currently observed session controller.
  __weak TTCSessionController* _observedSessionController;
}

#pragma mark - Initialization

- (instancetype)initWithTTCService:(TTCKeyedService*)ttcService {
  self = [super init];
  if (self) {
    _currentState = TTCSessionUIState::kIdle;
    _ttcService = ttcService;
    if (_ttcService) {
      __weak TTCMediator* weakSelf = self;
      _stateChangeSubscription = _ttcService->RegisterStateChangedCallback(
          base::BindRepeating(^(TTCServiceState state) {
            [weakSelf handleServiceStateChanged:state];
          }));
      [self startObservingSessionControllerIfNeeded];
    }
  }
  return self;
}

#pragma mark - Setters

- (void)setConsumer:(id<TTCConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self hydrateConsumer];
  }
}

#pragma mark - TTCMutator

- (void)startSession {
  if (!_ttcService || !_ttcService->IsEnabled()) {
    _currentState = TTCSessionUIState::kError;
    [self.consumer setSessionState:_currentState];
    [self.consumer didEncounterError:kServiceUnavailableError];
    return;
  }

  if (_observedSessionController || _ttcService->is_session_active()) {
    return;
  }

  _ttcService->StartSession();
  [self startObservingSessionControllerIfNeeded];
}

- (void)stopSession {
  if (_ttcService && _ttcService->is_session_active()) {
    _ttcService->EndSession();
  }
}

- (void)setLoopbackEnabled:(BOOL)enabled {
  [self.consumer setLoopbackEnabled:enabled];
}

- (void)playTestAudio {
  [self.consumer setTestAudioPlaying:NO];
}

- (void)stopTestAudio {
  [self.consumer setTestAudioPlaying:NO];
}

- (void)viewWillAppear {
  [self hydrateConsumer];
}

#pragma mark - TTCSessionControllerObserver

- (void)sessionController:(TTCSessionController*)controller
       didChangeLifecycle:(TTCSessionLifecycle)lifecycle {
  [self updateConsumerForLifecycle:lifecycle];
}

- (void)sessionController:(TTCSessionController*)controller
      didUpdateAudioLevel:(float)audioLevel {
  if (_currentState != TTCSessionUIState::kListening) {
    return;
  }
  [self.consumer setMicEnergyLevel:audioLevel];
}

- (void)sessionController:(TTCSessionController*)controller
         didFailWithError:(NSError*)error {
  _currentState = TTCSessionUIState::kError;
  [self.consumer setSessionState:_currentState];
  [self.consumer
      didEncounterError:error.localizedDescription ?: kDefaultSessionError];
}

#pragma mark - Public

- (void)disconnect {
  [self stopObservingSessionControllerIfNeeded];
  _stateChangeSubscription = {};
  _ttcService = nullptr;
  _consumer = nil;
}

#pragma mark - Private

// Starts observing the active session controller if not already observing.
- (void)startObservingSessionControllerIfNeeded {
  if (_observedSessionController || !_ttcService ||
      !_ttcService->session_controller()) {
    return;
  }
  _observedSessionController = _ttcService->session_controller();
  [_observedSessionController addObserver:self];
  [self updateConsumerForLifecycle:_observedSessionController.lifecycle];
}

// Stops observing the active session controller if currently observing.
- (void)stopObservingSessionControllerIfNeeded {
  if (!_observedSessionController) {
    return;
  }
  [_observedSessionController removeObserver:self];
  _observedSessionController = nil;
}

// Updates `_currentState` and notifies the consumer based on the session
// controller lifecycle.
- (void)updateConsumerForLifecycle:(TTCSessionLifecycle)lifecycle {
  _currentState = SessionUIStateForLifecycle(lifecycle);
  [self.consumer setSessionState:_currentState];
  if (_currentState == TTCSessionUIState::kIdle) {
    [self.consumer setMicEnergyLevel:0.0f];
  }
}

// Handles service state changes dispatched by `TTCKeyedService`.
- (void)handleServiceStateChanged:(TTCServiceState)state {
  if (state == TTCServiceState::kSessionActive) {
    [self startObservingSessionControllerIfNeeded];
  } else if (state == TTCServiceState::kSessionInactive) {
    [self stopObservingSessionControllerIfNeeded];
    if (_currentState != TTCSessionUIState::kError) {
      _currentState = TTCSessionUIState::kIdle;
      [self.consumer setSessionState:TTCSessionUIState::kIdle];
      [self.consumer setMicEnergyLevel:0.0f];
    }
  }
}

// Hydrates the consumer with current state and resets audio energy.
- (void)hydrateConsumer {
  if (!_consumer) {
    return;
  }
  [_consumer setSessionState:_currentState];
  [_consumer setMicEnergyLevel:0.0f];
  [_consumer setLoopbackEnabled:NO];
  [_consumer setTestAudioPlaying:NO];
}

@end
