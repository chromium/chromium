// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_engine.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_consumer.h"

namespace {

// Error messages displayed when microphone permissions or recording fails.
NSString* const kMicrophonePermissionDeniedError =
    @"Microphone permission denied";
NSString* const kFailedToStartRecordingError = @"Failed to start recording";

}  // namespace

@interface TTCMediator () <TTCAudioEngineDelegate>
@end

@implementation TTCMediator {
  // Current voice session lifecycle state (Idle, Connecting, Listening, Error).
  TTCSessionState _currentState;

  // Audio engine managing CoreAudio microphone capture and RMS computation.
  TTCAudioEngine* _audioEngine;
}

- (instancetype)initWithAudioEngine:(TTCAudioEngine*)audioEngine {
  self = [super init];
  if (self) {
    _currentState = TTCSessionState::kIdle;
    _audioEngine = audioEngine;
    _audioEngine.delegate = self;
  }
  return self;
}

- (instancetype)init {
  return [self initWithAudioEngine:[[TTCAudioEngine alloc] init]];
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
  _currentState = TTCSessionState::kConnecting;
  [self.consumer setSessionState:_currentState];

  __weak TTCMediator* weakSelf = self;
  [_audioEngine requestMicrophonePermissionWithCompletion:^(BOOL granted) {
    [weakSelf didRequestMicrophonePermissionWithGranted:granted];
  }];
}

- (void)stopSession {
  [_audioEngine stopRecording];
  _currentState = TTCSessionState::kIdle;
  [self.consumer setSessionState:TTCSessionState::kIdle];
  [self.consumer setMicEnergyLevel:0.0f];
}

- (void)viewWillAppear {
  [self hydrateConsumer];
}

#pragma mark - TTCAudioEngineDelegate

- (void)audioEngine:(TTCAudioEngine*)engine didUpdateInputEnergy:(float)rms {
  if (_currentState != TTCSessionState::kListening) {
    return;
  }
  [_consumer setMicEnergyLevel:rms];
}

- (void)audioEngine:(TTCAudioEngine*)engine didEncounterError:(NSError*)error {
  _currentState = TTCSessionState::kError;
  [_consumer setSessionState:_currentState];
  [_consumer didEncounterError:error.localizedDescription
                                   ?: kFailedToStartRecordingError];
}

#pragma mark - Public

- (void)disconnect {
  [_audioEngine disconnect];
  _audioEngine.delegate = nil;
  _audioEngine = nil;
  _currentState = TTCSessionState::kIdle;
  _consumer = nil;
}

#pragma mark - Private

// Handles the result of the microphone permission request. If granted, begins
// audio capture; otherwise transitions the session to error state.
- (void)didRequestMicrophonePermissionWithGranted:(BOOL)granted {
  // Discard callback if the session was stopped or disconnected while prompt
  // was visible.
  if (_currentState != TTCSessionState::kConnecting) {
    return;
  }

  if (!granted) {
    _currentState = TTCSessionState::kError;
    [self.consumer setSessionState:_currentState];
    [self.consumer didEncounterError:kMicrophonePermissionDeniedError];
    return;
  }

  __weak TTCMediator* weakSelf = self;
  [_audioEngine startRecordingWithCompletion:^(BOOL success, NSError* error) {
    [weakSelf didStartRecordingWithSuccess:success error:error];
  }];
}

// Handles the result of starting audio recording. If successful, transitions
// the session to listening state; otherwise transitions to error state.
- (void)didStartRecordingWithSuccess:(BOOL)success error:(NSError*)error {
  // Discard callback if the session was stopped or disconnected while startup
  // was pending.
  if (_currentState != TTCSessionState::kConnecting) {
    if (success) {
      [_audioEngine stopRecording];
    }
    return;
  }

  if (success) {
    _currentState = TTCSessionState::kListening;
    [self.consumer setSessionState:_currentState];
  } else {
    _currentState = TTCSessionState::kError;
    [self.consumer setSessionState:_currentState];
    [self.consumer didEncounterError:error.localizedDescription
                                         ?: kFailedToStartRecordingError];
  }
}

// Hydrates the consumer with current state and resets audio energy.
- (void)hydrateConsumer {
  if (!_consumer) {
    return;
  }
  [_consumer setSessionState:_currentState];
  [_consumer setMicEnergyLevel:0.0f];
}

@end
