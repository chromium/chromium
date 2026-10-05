// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"

#import "base/check.h"
#import "base/sequence_checker.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_engine.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"

NSString* const kTTCConversationErrorDomain = @"TTCConversationErrorDomain";

@interface TTCConversation () <TTCAudioControllerDelegate, TTCBackendDelegate>

@property(nonatomic, assign, readwrite) TTCConversationState state;

@end

@implementation TTCConversation {
  SEQUENCE_CHECKER(_sequenceChecker);

  // Generation counter incremented in `stop` to invalidate pending in-flight
  // async start or permission callbacks.
  uint64_t _sessionGeneration;
}

- (instancetype)initWithAudioController:(id<TTCAudioController>)audioController
                                backend:(id<TTCBackend>)backend {
  CHECK(audioController);
  self = [super init];
  if (self) {
    _audioController = audioController;
    _audioController.delegate = self;
    _backend = backend;
    _backend.delegate = self;
    _state = TTCConversationState::kStopped;
    _sessionGeneration = 0;
  }
  return self;
}

- (instancetype)init {
  return [self initWithAudioController:[[TTCAudioEngine alloc] init]
                               backend:nil];
}

#pragma mark - Public

- (void)start {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state != TTCConversationState::kStopped) {
    return;
  }

  _lastError = nil;
  const uint64_t currentGeneration = ++_sessionGeneration;

  [_backend connect];

  __weak __typeof(self) weakSelf = self;
  [_audioController startCaptureWithCompletion:^(BOOL success, NSError* error) {
    [weakSelf handleStartCaptureSuccess:success
                                  error:error
                             generation:currentGeneration];
  }];
}

- (void)stop {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // Invalidate any pending in-flight start completions.
  _sessionGeneration++;

  [_backend disconnect];

  if (_state == TTCConversationState::kStopped) {
    return;
  }

  [_audioController stopCapture];
  [_audioController stopPlayback];
  [self updateState:TTCConversationState::kStopped];
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stop];
  [_audioController disconnect];
  _audioController.delegate = nil;
  _backend.delegate = nil;
  self.delegate = nil;
}

- (void)playResponseAudio:(NSData*)pcm24kData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state == TTCConversationState::kStopped || !pcm24kData.length) {
    return;
  }

  if (_state != TTCConversationState::kTalking) {
    [self updateState:TTCConversationState::kTalking];
  }

  [_audioController playStreamingAudioChunk:pcm24kData];
}

- (void)finishTurn {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state == TTCConversationState::kTalking) {
    [self updateState:TTCConversationState::kListening];
  }
}

#pragma mark - TTCAudioControllerDelegate

- (void)audioController:(id<TTCAudioController>)controller
    didCaptureAudioChunk:(NSData*)pcmData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state == TTCConversationState::kStopped) {
    return;
  }

  [_backend sendAudioChunk:pcmData];

  if ([self.delegate
          respondsToSelector:@selector(conversation:didCaptureAudioChunk:)]) {
    [self.delegate conversation:self didCaptureAudioChunk:pcmData];
  }
}

- (void)audioController:(id<TTCAudioController>)controller
    didUpdateInputEnergy:(float)energy {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state == TTCConversationState::kStopped) {
    return;
  }

  if ([self.delegate
          respondsToSelector:@selector(conversation:didUpdateAudioEnergy:)]) {
    [self.delegate conversation:self didUpdateAudioEnergy:energy];
  }
}

- (void)audioControllerDidStopPlayback:(id<TTCAudioController>)controller {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // When response audio finishes playing through the speaker, transition
  // back to listening.
  if (_state == TTCConversationState::kTalking) {
    [self finishTurn];
  }
}

- (void)audioController:(id<TTCAudioController>)controller
      didEncounterError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self handleError:error];
}

- (void)audioControllerDidStopCapture:(id<TTCAudioController>)controller {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state != TTCConversationState::kStopped) {
    [self stop];
  }
}

#pragma mark - TTCBackendDelegate

- (void)backendDidInitialize:(id<TTCBackend>)backend {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ([self.delegate
          respondsToSelector:@selector(conversationDidInitialize:)]) {
    [self.delegate conversationDidInitialize:self];
  }
}

- (void)backendDidClose:(id<TTCBackend>)backend {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state != TTCConversationState::kStopped) {
    [self stop];
  }
}

- (void)backend:(id<TTCBackend>)backend
    didFailWithError:(TTCErrorCode)errorCode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSError* error = [NSError errorWithDomain:kTTCErrorDomain
                                       code:static_cast<NSInteger>(errorCode)
                                   userInfo:nil];
  [self handleError:error];
}

- (void)backend:(id<TTCBackend>)backend
    didReceiveAudioOutput:(NSData*)audioData
           sequenceNumber:(int64_t)sequenceNumber {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self playResponseAudio:audioData];
}

- (void)backend:(id<TTCBackend>)backend
    didChangeGenerationStateStarted:(BOOL)started
                          completed:(BOOL)completed
                        interrupted:(BOOL)interrupted {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (interrupted) {
    [_audioController clearPlaybackQueue];
    [_audioController stopPlayback];
    if (_state == TTCConversationState::kTalking) {
      [self updateState:TTCConversationState::kListening];
    }
  } else if (completed) {
    [self finishTurn];
  }
}

#pragma mark - Private

- (void)handleStartCaptureSuccess:(BOOL)success
                            error:(NSError*)error
                       generation:(uint64_t)generation {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_sessionGeneration != generation) {
    return;
  }

  if (!success) {
    if (!error) {
      error = [NSError
          errorWithDomain:kTTCConversationErrorDomain
                     code:static_cast<NSInteger>(
                              TTCConversationErrorCode::kAudioCaptureFailure)
                 userInfo:@{
                   NSLocalizedDescriptionKey :
                       @"Audio controller failed to start capture."
                 }];
    }
    [self handleError:error];
    return;
  }

  [self updateState:TTCConversationState::kListening];
}

- (void)updateState:(TTCConversationState)newState {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_state == newState) {
    return;
  }

  _state = newState;
  if ([self.delegate
          respondsToSelector:@selector(conversation:didChangeState:)]) {
    [self.delegate conversation:self didChangeState:_state];
  }
}

- (void)handleError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _lastError = error;
  [self stop];

  if ([self.delegate
          respondsToSelector:@selector(conversation:didEncounterError:)]) {
    [self.delegate conversation:self didEncounterError:error];
  }
}

@end
