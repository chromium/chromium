// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_voice_plate_mediator.h"

#import <memory>

#import "base/callback_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/to_string.h"
#import "components/ttc/app/public/error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer_bridge.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_consumer.h"

namespace {

// Error message displayed when the TTC service is unavailable.
NSString* const kServiceUnavailableError = @"TTC service is unavailable";

// Default error message displayed when a session error occurs without a
// specific description.
NSString* const kDefaultSessionError = @"Session error occurred";

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

@interface TTCVoicePlateMediator () <TTCSessionControllerObserver>
@end

@implementation TTCVoicePlateMediator {
  // Current voice session lifecycle state (Idle, Connecting, Listening, Error).
  TTCSessionUIState _currentState;

  // Whether microphone capture is currently muted.
  BOOL _isMicMuted;

  // TTC keyed service for the profile.
  raw_ptr<TTCKeyedService> _ttcService;

  // Subscription observing state changes in TTCKeyedService.
  base::CallbackListSubscription _stateChangeSubscription;

  // Bridge observing `TtcSessionController`.
  std::unique_ptr<TtcSessionControllerObserverBridge>
      _sessionControllerObserverBridge;

  // Weak reference to the currently observed session controller.
  base::WeakPtr<TtcSessionController> _observedSessionController;
}

#pragma mark - Initialization

- (instancetype)initWithTTCService:(TTCKeyedService*)ttcService {
  self = [super init];
  if (self) {
    _currentState = TTCSessionUIState::kIdle;
    _ttcService = ttcService;
    _sessionControllerObserverBridge =
        std::make_unique<TtcSessionControllerObserverBridge>(self);
    if (_ttcService) {
      __weak TTCVoicePlateMediator* weakSelf = self;
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

- (void)setConsumer:(id<TTCVoicePlateConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self hydrateConsumer];
  }
}

#pragma mark - TTCVoicePlateMutator

- (void)startSession {
  if (!_ttcService || !_ttcService->IsEnabled()) {
    _currentState = TTCSessionUIState::kError;
    _isMicMuted = NO;
    [self.consumer setSessionState:_currentState];
    [self.consumer setMicMuted:NO];
    [self.consumer showErrorMessage:kServiceUnavailableError];
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

- (void)toggleMicMuted {
  if (_observedSessionController &&
      _observedSessionController->lifecycle() == TTCSessionLifecycle::kLive) {
    _observedSessionController->SetMicMuted(
        !_observedSessionController->IsMicMuted());
  }
}

#pragma mark - TTCSessionControllerObserver

- (void)sessionController:(TtcSessionController*)controller
       didChangeLifecycle:(TTCSessionLifecycle)lifecycle {
  if (lifecycle == TTCSessionLifecycle::kFinished) {
    [self stopObservingSessionControllerIfNeeded];
    if (_currentState == TTCSessionUIState::kError) {
      [self.consumer setMicEnergyLevel:0.0f];
      return;
    }
  }
  [self updateConsumerForLifecycle:lifecycle];
}

- (void)sessionController:(TtcSessionController*)controller
        didChangeMicMuted:(BOOL)micMuted {
  if (_isMicMuted == micMuted) {
    return;
  }
  _isMicMuted = micMuted;
  [self.consumer setMicMuted:micMuted];
}

- (void)sessionController:(TtcSessionController*)controller
      didUpdateAudioLevel:(float)audioLevel {
  if (_currentState != TTCSessionUIState::kListening || _isMicMuted) {
    return;
  }
  [self.consumer setMicEnergyLevel:audioLevel];
}

- (void)sessionController:(TtcSessionController*)controller
         didFailWithError:(ttc::ErrorCode)error {
  _currentState = TTCSessionUIState::kError;
  _isMicMuted = NO;
  [self.consumer setSessionState:_currentState];
  [self.consumer setMicMuted:NO];
  NSString* errorMessage = error == ttc::ErrorCode::kUnknown
                               ? kDefaultSessionError
                               : base::SysUTF8ToNSString(base::ToString(error));
  [self.consumer showErrorMessage:errorMessage];
}

#pragma mark - Public

- (void)disconnect {
  [self stopObservingSessionControllerIfNeeded];
  _sessionControllerObserverBridge.reset();
  _stateChangeSubscription = {};
  _ttcService = nullptr;
  self.consumer = nil;
}

#pragma mark - Private

// Starts observing the active session controller if not already observing.
- (void)startObservingSessionControllerIfNeeded {
  if (_observedSessionController || !_ttcService ||
      !_sessionControllerObserverBridge) {
    return;
  }
  TtcSessionController* sessionController = _ttcService->session_controller();
  if (!sessionController) {
    return;
  }
  _observedSessionController = sessionController->GetWeakPtr();
  sessionController->AddObserver(_sessionControllerObserverBridge.get());
  [self updateConsumerForLifecycle:sessionController->lifecycle()];
}

// Stops observing the active session controller if currently observing.
- (void)stopObservingSessionControllerIfNeeded {
  if (_observedSessionController && _sessionControllerObserverBridge) {
    _observedSessionController->RemoveObserver(
        _sessionControllerObserverBridge.get());
  }
  _observedSessionController.reset();
}

// Updates `_currentState` and notifies the consumer based on the session
// controller lifecycle.
- (void)updateConsumerForLifecycle:(TTCSessionLifecycle)lifecycle {
  _currentState = SessionUIStateForLifecycle(lifecycle);
  _isMicMuted =
      _observedSessionController && _observedSessionController->IsMicMuted();
  id<TTCVoicePlateConsumer> consumer = self.consumer;
  [consumer setSessionState:_currentState];
  [consumer setMicMuted:_isMicMuted];
  if (_currentState == TTCSessionUIState::kIdle || _isMicMuted) {
    [consumer setMicEnergyLevel:0.0f];
  }
}

// Handles service state changes dispatched by `TTCKeyedService`.
- (void)handleServiceStateChanged:(TTCServiceState)state {
  if (state == TTCServiceState::kSessionActive) {
    [self startObservingSessionControllerIfNeeded];
  } else if (state == TTCServiceState::kSessionInactive) {
    [self stopObservingSessionControllerIfNeeded];
    _isMicMuted = NO;
    if (_currentState != TTCSessionUIState::kError) {
      _currentState = TTCSessionUIState::kIdle;
      [self.consumer setSessionState:TTCSessionUIState::kIdle];
      [self.consumer setMicMuted:NO];
      [self.consumer setMicEnergyLevel:0.0f];
    }
  }
}

// Hydrates the consumer with current state and resets audio energy.
- (void)hydrateConsumer {
  [self.consumer setSessionState:_currentState];
  [self.consumer setMicMuted:_isMicMuted];
  [self.consumer setMicEnergyLevel:0.0f];
}

@end
