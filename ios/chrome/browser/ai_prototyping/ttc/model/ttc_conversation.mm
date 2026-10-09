// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"

#import "base/check.h"
#import "base/sequence_checker.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_engine.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_backend.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_websocket_backend.h"

@interface TTCConversation () <TTCAudioControllerDelegate, TTCBackendDelegate>
@end

@implementation TTCConversation {
  SEQUENCE_CHECKER(_sequenceChecker);

  // Generation counter incremented in `stop` to invalidate pending in-flight
  // async start or permission callbacks.
  uint64_t _sessionGeneration;

  // Guard flag preventing re-entrant stop invocations.
  BOOL _isStopping;
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
    _sessionGeneration = 0;
    _isStopping = NO;
  }
  return self;
}

- (instancetype)init {
  return [self initWithAudioController:[[TTCAudioEngine alloc] init]
                               backend:[[TTCWebSocketBackend alloc] init]];
}

#pragma mark - Public

- (void)start {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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
  if (_isStopping) {
    return;
  }
  _isStopping = YES;

  // Invalidate any pending in-flight start completions.
  _sessionGeneration++;

  [_audioController stopCapture];
  [_audioController stopPlayback];
  [_backend disconnect];

  _isStopping = NO;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stop];
  [_audioController disconnect];
  _audioController.delegate = nil;
  _backend.delegate = nil;
  self.delegate = nil;
}

#pragma mark - TTCAudioControllerDelegate

- (void)audioController:(id<TTCAudioController>)controller
    didCaptureAudioChunk:(NSData*)pcmData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Disable barge-in: suppress microphone capture while response audio is
  // playing.
  if (_audioController.isPlaying) {
    return;
  }

  [_backend sendAudioChunk:pcmData];
}

- (void)audioController:(id<TTCAudioController>)controller
    didUpdateInputEnergy:(float)energy {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.delegate conversation:self didUpdateAudioEnergy:energy];
}

- (void)audioController:(id<TTCAudioController>)controller
      didEncounterError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self handleError:error];
}

- (void)audioControllerDidStopCapture:(id<TTCAudioController>)controller {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stop];
}

#pragma mark - TTCBackendDelegate

- (void)backendDidInitialize:(id<TTCBackend>)backend {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.delegate conversationDidInitialize:self];
}

- (void)backendDidClose:(id<TTCBackend>)backend {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.delegate conversationDidClose:self];
}

- (void)backend:(id<TTCBackend>)backend
    didFailWithError:(ttc::ErrorCode)errorCode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self handleError:CreateTTCError(errorCode)];
}

- (void)backend:(id<TTCBackend>)backend
    didReceiveAudioOutput:(NSData*)audioData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!audioData.length) {
    return;
  }
  [_audioController playStreamingAudioChunk:audioData];
}

- (void)backend:(id<TTCBackend>)backend
    didChangeGenerationStateStarted:(BOOL)started
                          completed:(BOOL)completed
                        interrupted:(BOOL)interrupted {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (interrupted) {
    [_audioController clearPlaybackQueue];
    [_audioController stopPlayback];
  }
}

- (void)backend:(id<TTCBackend>)backend
    didReceiveInputTranscription:(NSString*)inputTranscription {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Transcriptions will be forwarded to observers in a subsequent CL.
}

- (void)backend:(id<TTCBackend>)backend
    didReceiveOutputTranscription:(NSString*)outputTranscription {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Transcriptions will be forwarded to observers in a subsequent CL.
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
      error = CreateTTCError(ttc::ErrorCode::kAudioUnknownError);
    }
    [self handleError:error];
  }
}

- (void)handleError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.delegate conversation:self didEncounterError:error];
}

@end
