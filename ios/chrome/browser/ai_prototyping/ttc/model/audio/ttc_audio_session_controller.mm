// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_controller.h"

#import <AVFAudio/AVFAudio.h>

#import <algorithm>
#import <cmath>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/span.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/sequence_checker.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_engine.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_manager.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_manager_delegate.h"
#import "ios/public/provider/chrome/browser/intelligence/ttc_audio_engine_protocol.h"

NSString* const kTTCAudioSessionControllerErrorDomain =
    @"org.chromium.ttc.audio";

namespace {

// Test audio tone generation constants.
// Generates a 440Hz sine wave at 24kHz in 20ms chunks (480 samples each)
// for 1.0 second (50 total chunks).
constexpr double kTestToneFrequency = 440.0;
constexpr double kTestToneSampleRate = 24000.0;
constexpr size_t kTestToneChunkSampleCount = 480;
constexpr size_t kTestToneTotalChunks = 50;
constexpr double kTestToneAmplitude = 8000.0;

// Upsamples 16kHz 16-bit signed linear PCM audio data to 24kHz 16-bit signed
// linear PCM (3:2 sample ratio) via linear interpolation for local loopback.
NSData* Upsample16kTo24kPCM(NSData* pcm16kData) {
  if (pcm16kData.length < sizeof(int16_t)) {
    return nil;
  }
  auto inputSamples = base::subtle::reinterpret_span<const int16_t>(
      base::apple::NSDataToSpan(pcm16kData));
  size_t inputCount = inputSamples.size();
  if (inputCount == 0) {
    return nil;
  }
  size_t outputCount = (inputCount * 3) / 2;
  if (outputCount == 0) {
    outputCount = 1;
  }
  std::vector<int16_t> outputSamples(outputCount);
  for (size_t i = 0; i < outputCount; ++i) {
    double srcPos = static_cast<double>(i) * 2.0 / 3.0;
    size_t idx0 = std::min(static_cast<size_t>(srcPos), inputCount - 1);
    size_t idx1 = std::min(idx0 + 1, inputCount - 1);
    double frac = srcPos - static_cast<double>(idx0);
    double interpolated =
        (1.0 - frac) * static_cast<double>(inputSamples[idx0]) +
        frac * static_cast<double>(inputSamples[idx1]);
    outputSamples[i] = static_cast<int16_t>(
        std::clamp(std::lround(interpolated), -32768L, 32767L));
  }
  return [NSData dataWithBytes:outputSamples.data()
                        length:outputSamples.size() * sizeof(int16_t)];
}

NSError* CreateControllerError(TTCAudioSessionControllerErrorCode code,
                               NSString* description) {
  return [NSError errorWithDomain:kTTCAudioSessionControllerErrorDomain
                             code:static_cast<NSInteger>(code)
                         userInfo:@{NSLocalizedDescriptionKey : description}];
}

}  // namespace

@interface TTCAudioSessionController () <TTCAudioEngineDelegate,
                                         TTCAudioSessionManagerDelegate>

@property(nonatomic, strong) id<TTCAudioEngineProtocol> audioEngine;
@property(nonatomic, strong) TTCAudioSessionManager* sessionManager;
@property(nonatomic, strong) NSMutableArray<NSData*>* pendingPlaybackChunks;
@property(nonatomic, assign) BOOL isStartingCapture;
@property(nonatomic, assign) BOOL isStartingPlaybackEngine;
@property(nonatomic, assign) BOOL isStreamingPlaybackActive;
@property(nonatomic, assign, getter=isDisconnected) BOOL disconnected;
@property(nonatomic, assign) uint64_t sessionGeneration;

@end

@implementation TTCAudioSessionController {
  SEQUENCE_CHECKER(_sequenceChecker);
}

@synthesize delegate = _delegate;
@synthesize loopbackEnabled = _loopbackEnabled;

#pragma mark - Properties

- (BOOL)isCapturing {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return self.audioEngine.isCapturing;
}

- (BOOL)isPlaying {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return self.audioEngine.isPlaying || self.pendingPlaybackChunks.count > 0;
}

- (BOOL)isLoopbackEnabled {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _loopbackEnabled;
}

- (void)setLoopbackEnabled:(BOOL)loopbackEnabled {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _loopbackEnabled = loopbackEnabled;
}

- (BOOL)isOutputRoutedToSpeaker {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return self.sessionManager.outputDestination ==
         TTCAudioOutputDestination::kSpeaker;
}

#pragma mark - Lifecycle

- (instancetype)initWithAudioEngine:(id<TTCAudioEngineProtocol>)audioEngine
                     sessionManager:(TTCAudioSessionManager*)sessionManager {
  self = [super init];
  if (self) {
    _audioEngine = audioEngine ?: [[TTCAudioEngine alloc] init];
    _audioEngine.delegate = self;
    _sessionManager = sessionManager ?: [[TTCAudioSessionManager alloc] init];
    _sessionManager.delegate = self;
    _pendingPlaybackChunks = [[NSMutableArray alloc] init];
    _isStartingCapture = NO;
    _isStartingPlaybackEngine = NO;
    _isStreamingPlaybackActive = NO;
    _loopbackEnabled = NO;
    _disconnected = NO;
    _sessionGeneration = 0;
  }
  return self;
}

- (instancetype)init {
  return [self initWithAudioEngine:nil sessionManager:nil];
}

#pragma mark - TTCAudioController

- (void)startCaptureWithCompletion:(void (^)(BOOL success,
                                             NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    if (completion) {
      completion(NO, CreateControllerError(
                         TTCAudioSessionControllerErrorCode::kStartupCancelled,
                         @"Audio session controller is disconnected."));
    }
    return;
  }

  if (self.audioEngine.isCapturing) {
    if (completion) {
      completion(YES, nil);
    }
    return;
  }

  if (self.isStartingCapture) {
    if (completion) {
      completion(NO, CreateControllerError(
                         TTCAudioSessionControllerErrorCode::kStartupCancelled,
                         @"Audio capture startup is already in flight."));
    }
    return;
  }

  self.isStartingCapture = YES;
  uint64_t generation = ++self.sessionGeneration;

  __weak __typeof(self) weakSelf = self;
  [self requestMicrophonePermissionWithCompletion:^(BOOL granted) {
    [weakSelf didRequestPermissionWithGranted:granted
                                   generation:generation
                                   completion:completion];
  }];
}

- (void)stopCapture {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  BOOL wasCapturing = self.audioEngine.isCapturing;
  BOOL wasStarting = self.isStartingCapture;
  if (!wasCapturing && !wasStarting) {
    return;
  }

  self.isStartingCapture = NO;
  self.sessionGeneration++;

  if (!wasCapturing) {
    return;
  }

  [self.audioEngine stopCapture];

  if (!self.audioEngine.isPlaying && !self.isStartingPlaybackEngine &&
      self.pendingPlaybackChunks.count == 0 && self.audioEngine.isStarted) {
    [self.audioEngine stopWithCompletion:nil];
  }

  if ([self.delegate
          respondsToSelector:@selector(audioControllerDidStopCapture:)]) {
    [self.delegate audioControllerDidStopCapture:self];
  }
}

- (void)playStreamingAudioChunk:(NSData*)pcm24kData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || pcm24kData.length == 0) {
    return;
  }

  if (!self.audioEngine.isStarted) {
    [self.pendingPlaybackChunks addObject:[pcm24kData copy]];
    [self ensureEngineStartedForPlayback];
    return;
  }

  if (!self.isStreamingPlaybackActive) {
    // Flush any queued loopback buffers so synthesized audio begins
    // immediately without delay.
    [self.audioEngine stopPlayback];
    self.isStreamingPlaybackActive = YES;
  }

  [self.audioEngine schedulePlaybackData:pcm24kData];
}

- (void)clearPlaybackQueue {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stopPlaybackImmediately];
}

- (void)stopPlayback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stopPlaybackImmediately];
}

- (void)playTestTone {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  for (size_t chunkIndex = 0; chunkIndex < kTestToneTotalChunks; ++chunkIndex) {
    std::vector<int16_t> samples(kTestToneChunkSampleCount);
    for (size_t i = 0; i < kTestToneChunkSampleCount; ++i) {
      size_t globalSampleIndex = chunkIndex * kTestToneChunkSampleCount + i;
      double t = static_cast<double>(globalSampleIndex) / kTestToneSampleRate;
      double sineValue = std::sin(2.0 * M_PI * kTestToneFrequency * t);
      samples[i] = static_cast<int16_t>(sineValue * kTestToneAmplitude);
    }
    NSData* chunkData = [NSData dataWithBytes:samples.data()
                                       length:samples.size() * sizeof(int16_t)];
    [self playStreamingAudioChunk:chunkData];
  }
}

- (void)stopTestTone {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stopPlaybackImmediately];
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    return;
  }
  self.disconnected = YES;
  self.delegate = nil;

  self.isStartingCapture = NO;
  self.isStartingPlaybackEngine = NO;
  self.isStreamingPlaybackActive = NO;
  self.sessionGeneration++;
  [self.pendingPlaybackChunks removeAllObjects];

  self.audioEngine.delegate = nil;
  [self.audioEngine disconnect];

  self.sessionManager.delegate = nil;
  [self.sessionManager disconnect];
}

#pragma mark - TTCAudioEngineDelegate

- (void)audioEngine:(id<TTCAudioEngineProtocol>)engine
    didCaptureAudioData:(NSData*)audioData
             inputLevel:(float)inputLevel {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || !self.audioEngine.isCapturing) {
    return;
  }

  if ([self.delegate
          respondsToSelector:@selector(
                                 audioController:didUpdateInputEnergy:)]) {
    [self.delegate audioController:self didUpdateInputEnergy:inputLevel];
  }

  if (self.isLoopbackEnabled && !self.isStreamingPlaybackActive &&
      audioData.length > 0) {
    NSData* upsampled24k = Upsample16kTo24kPCM(audioData);
    if (upsampled24k.length > 0) {
      [self.audioEngine schedulePlaybackData:upsampled24k];
    }
  }

  if (audioData.length > 0 &&
      [self.delegate
          respondsToSelector:@selector(
                                 audioController:didCaptureAudioChunk:)]) {
    [self.delegate audioController:self didCaptureAudioChunk:audioData];
  }
}

- (void)audioEngineDidStartPlayback:(id<TTCAudioEngineProtocol>)engine {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    return;
  }
  if ([self.delegate
          respondsToSelector:@selector(audioControllerDidStartPlayback:)]) {
    [self.delegate audioControllerDidStartPlayback:self];
  }
}

- (void)audioEngineDidStopPlayback:(id<TTCAudioEngineProtocol>)engine {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    return;
  }
  self.isStreamingPlaybackActive = NO;
  if (!self.audioEngine.isCapturing && !self.isStartingCapture &&
      !self.isStartingPlaybackEngine && self.pendingPlaybackChunks.count == 0 &&
      self.audioEngine.isStarted) {
    [self.audioEngine stopWithCompletion:nil];
  }
  if ([self.delegate
          respondsToSelector:@selector(audioControllerDidStopPlayback:)]) {
    [self.delegate audioControllerDidStopPlayback:self];
  }
}

- (void)audioEngine:(id<TTCAudioEngineProtocol>)engine
    didEncounterError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    return;
  }
  if ([self.delegate
          respondsToSelector:@selector(audioController:didEncounterError:)]) {
    [self.delegate audioController:self didEncounterError:error];
  }
}

#pragma mark - TTCAudioSessionManagerDelegate

- (void)audioSessionManager:(TTCAudioSessionManager*)manager
    didChangeRouteDescription:(NSString*)routeDescription
               hasHardwareAEC:(BOOL)hasAEC {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    return;
  }
  self.audioEngine.aecMode =
      hasAEC ? TTCAudioAECMode::kHardware : TTCAudioAECMode::kAdaptiveSoftware;
  if ([self.delegate
          respondsToSelector:@selector(audioControllerDidChangeRoute:)]) {
    [self.delegate audioControllerDidChangeRoute:self];
  }
}

- (void)audioSessionManagerDidRequireEngineReconfiguration:
    (TTCAudioSessionManager*)manager {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    return;
  }
  [self updateEngineAECMode];
  if (!self.audioEngine.isStarted || self.isStartingCapture ||
      self.isStartingPlaybackEngine) {
    return;
  }

  BOOL wasCapturing = self.audioEngine.isCapturing;
  uint64_t generation = ++self.sessionGeneration;
  __weak __typeof(self) weakSelf = self;
  [self.audioEngine stopWithCompletion:^(BOOL stopped, NSError* stopError) {
    [weakSelf didStopEngineForReconfigurationWithWasCapturing:wasCapturing
                                                   generation:generation];
  }];
}

- (void)audioSessionManagerDidBeginInterruption:
    (TTCAudioSessionManager*)manager {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    return;
  }
  [self stopPlaybackImmediately];
  [self stopCapture];
}

#pragma mark - Private

// Updates `self.audioEngine.aecMode` based on whether the active audio route
// provides hardware acoustic echo cancellation.
- (void)updateEngineAECMode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  self.audioEngine.aecMode = self.sessionManager.hasHardwareAEC
                                 ? TTCAudioAECMode::kHardware
                                 : TTCAudioAECMode::kAdaptiveSoftware;
}

// Restarts the audio engine after stopping for a route reconfiguration.
- (void)didStopEngineForReconfigurationWithWasCapturing:(BOOL)wasCapturing
                                             generation:(uint64_t)generation {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || self.sessionGeneration != generation) {
    return;
  }
  [self updateEngineAECMode];
  __weak __typeof(self) weakSelf = self;
  [self.audioEngine startWithCompletion:^(BOOL started, NSError* startError) {
    [weakSelf didRestartEngineAfterReconfiguration:started
                                             error:startError
                                      wasCapturing:wasCapturing
                                        generation:generation];
  }];
}

// Restores capture and flushes queued playback after an engine reconfiguration
// restart completes.
- (void)didRestartEngineAfterReconfiguration:(BOOL)started
                                       error:(NSError*)startError
                                wasCapturing:(BOOL)wasCapturing
                                  generation:(uint64_t)generation {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || self.sessionGeneration != generation) {
    return;
  }
  if (!started) {
    if (wasCapturing &&
        [self.delegate
            respondsToSelector:@selector(audioControllerDidStopCapture:)]) {
      [self.delegate audioControllerDidStopCapture:self];
    }
    if (startError &&
        [self.delegate
            respondsToSelector:@selector(audioController:didEncounterError:)]) {
      [self.delegate audioController:self didEncounterError:startError];
    }
    return;
  }

  if (wasCapturing) {
    if (![self.audioEngine startCapture]) {
      if ([self.delegate
              respondsToSelector:@selector(audioControllerDidStopCapture:)]) {
        [self.delegate audioControllerDidStopCapture:self];
      }
    }
  }
  [self flushPendingPlaybackChunks];
}

// Immediately stops active playback, clears queued chunks, and stops the
// underlying audio engine if capture is not active or starting.
- (void)stopPlaybackImmediately {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  self.isStreamingPlaybackActive = NO;
  [self.pendingPlaybackChunks removeAllObjects];
  self.isStartingPlaybackEngine = NO;
  [self.audioEngine stopPlayback];
  if (!self.audioEngine.isCapturing && !self.isStartingCapture &&
      self.audioEngine.isStarted) {
    [self.audioEngine stopWithCompletion:nil];
  }
}

// Requests microphone record permission via `AVAudioApplication`, hopping back
// to the creation sequence before invoking `completion`.
- (void)requestMicrophonePermissionWithCompletion:
    (void (^)(BOOL granted))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioApplication* app = [AVAudioApplication sharedInstance];
  if (app.recordPermission == AVAudioApplicationRecordPermissionGranted) {
    if (completion) {
      completion(YES);
    }
    return;
  }
  if (app.recordPermission == AVAudioApplicationRecordPermissionDenied) {
    if (completion) {
      completion(NO);
    }
    return;
  }

  if (completion) {
    completion = base::CallbackToBlock(
        base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                           base::BindOnce(completion)));
  } else {
    completion = ^(BOOL) {
    };
  }

  DCHECK(completion);
  [AVAudioApplication requestRecordPermissionWithCompletionHandler:completion];
}

// Handles the result of the microphone permission prompt.
- (void)didRequestPermissionWithGranted:(BOOL)granted
                             generation:(uint64_t)generation
                             completion:(void (^)(BOOL success,
                                                  NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || !self.isStartingCapture ||
      self.sessionGeneration != generation) {
    if (completion) {
      completion(NO, CreateControllerError(
                         TTCAudioSessionControllerErrorCode::kStartupCancelled,
                         @"Audio capture startup was cancelled."));
    }
    return;
  }

  if (!granted) {
    self.isStartingCapture = NO;
    if (completion) {
      completion(NO, CreateControllerError(
                         TTCAudioSessionControllerErrorCode::kPermissionDenied,
                         @"Microphone permission denied"));
    }
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self.sessionManager
      configureAudioSessionWithCompletion:^(NSError* sessionError) {
        [weakSelf didFinishAudioSessionConfigurationWithError:sessionError
                                                   generation:generation
                                                   completion:completion];
      }];
}

// Handles completion of background `AVAudioSession` configuration, starting the
// audio engine if needed and beginning microphone capture.
- (void)didFinishAudioSessionConfigurationWithError:(NSError*)error
                                         generation:(uint64_t)generation
                                         completion:(void (^)(BOOL, NSError*))
                                                        completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || !self.isStartingCapture ||
      self.sessionGeneration != generation) {
    [self.sessionManager restoreAudioSessionCategory];
    if (completion) {
      completion(NO, CreateControllerError(
                         TTCAudioSessionControllerErrorCode::kStartupCancelled,
                         @"Audio capture startup was cancelled."));
    }
    return;
  }

  if (error) {
    self.isStartingCapture = NO;
    [self.sessionManager restoreAudioSessionCategory];
    if (completion) {
      completion(NO, error);
    }
    return;
  }

  [self updateEngineAECMode];

  if (self.audioEngine.isStarted) {
    [self finishStartCaptureAfterEngineStart:YES
                                       error:nil
                                  generation:generation
                                  completion:completion];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self.audioEngine startWithCompletion:^(BOOL started, NSError* startError) {
    [weakSelf finishStartCaptureAfterEngineStart:started
                                           error:startError
                                      generation:generation
                                      completion:completion];
  }];
}

// Completes capture startup after the audio engine has started.
- (void)finishStartCaptureAfterEngineStart:(BOOL)started
                                     error:(NSError*)startError
                                generation:(uint64_t)generation
                                completion:
                                    (void (^)(BOOL, NSError*))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || !self.isStartingCapture ||
      self.sessionGeneration != generation) {
    if (!self.isStartingPlaybackEngine &&
        self.pendingPlaybackChunks.count == 0) {
      [self.audioEngine stopWithCompletion:nil];
      [self.sessionManager restoreAudioSessionCategory];
    }
    if (completion) {
      completion(NO, CreateControllerError(
                         TTCAudioSessionControllerErrorCode::kStartupCancelled,
                         @"Audio capture startup was cancelled."));
    }
    return;
  }

  self.isStartingCapture = NO;

  if (!started) {
    [self.sessionManager restoreAudioSessionCategory];
    if (completion) {
      completion(
          NO,
          startError
              ?: CreateControllerError(
                     TTCAudioSessionControllerErrorCode::kCaptureStartFailed,
                     @"Failed to start audio engine."));
    }
    return;
  }

  if (![self.audioEngine startCapture]) {
    if (!self.audioEngine.isPlaying && self.pendingPlaybackChunks.count == 0) {
      [self.audioEngine stopWithCompletion:nil];
      [self.sessionManager restoreAudioSessionCategory];
    }
    if (completion) {
      completion(NO,
                 CreateControllerError(
                     TTCAudioSessionControllerErrorCode::kCaptureStartFailed,
                     @"Failed to start microphone capture."));
    }
    return;
  }

  [self flushPendingPlaybackChunks];

  if ([self.delegate
          respondsToSelector:@selector(audioControllerDidStartCapture:)]) {
    [self.delegate audioControllerDidStartCapture:self];
  }
  if (completion) {
    completion(YES, nil);
  }
}

// Configures the audio session and starts the audio engine on demand when
// playback chunks arrive while the engine is not yet started.
- (void)ensureEngineStartedForPlayback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isStartingPlaybackEngine || self.isStartingCapture) {
    return;
  }

  NSError* sessionError = [self.sessionManager configureAudioSession];
  if (sessionError) {
    [self.pendingPlaybackChunks removeAllObjects];
    if ([self.delegate
            respondsToSelector:@selector(audioController:didEncounterError:)]) {
      [self.delegate audioController:self didEncounterError:sessionError];
    }
    if ([self.delegate
            respondsToSelector:@selector(audioControllerDidStopPlayback:)]) {
      [self.delegate audioControllerDidStopPlayback:self];
    }
    return;
  }

  [self updateEngineAECMode];
  self.isStartingPlaybackEngine = YES;
  __weak __typeof(self) weakSelf = self;
  [self.audioEngine startWithCompletion:^(BOOL started, NSError* startError) {
    [weakSelf didFinishStartingEngineForPlayback:started error:startError];
  }];
}

// Handles completion of on-demand playback engine startup and flushes any
// queued playback chunks.
- (void)didFinishStartingEngineForPlayback:(BOOL)started
                                     error:(NSError*)startError {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    return;
  }
  self.isStartingPlaybackEngine = NO;

  if (!started) {
    [self.pendingPlaybackChunks removeAllObjects];
    if (!self.isStartingCapture && !self.audioEngine.isCapturing) {
      [self.sessionManager restoreAudioSessionCategory];
    }
    if (startError &&
        [self.delegate
            respondsToSelector:@selector(audioController:didEncounterError:)]) {
      [self.delegate audioController:self didEncounterError:startError];
    }
    if ([self.delegate
            respondsToSelector:@selector(audioControllerDidStopPlayback:)]) {
      [self.delegate audioControllerDidStopPlayback:self];
    }
    return;
  }

  [self flushPendingPlaybackChunks];
}

// Schedules all queued playback chunks onto the running audio engine.
- (void)flushPendingPlaybackChunks {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!self.audioEngine.isStarted || self.pendingPlaybackChunks.count == 0) {
    return;
  }
  if (!self.isStreamingPlaybackActive) {
    [self.audioEngine stopPlayback];
    self.isStreamingPlaybackActive = YES;
  }
  NSArray<NSData*>* chunks = [self.pendingPlaybackChunks copy];
  [self.pendingPlaybackChunks removeAllObjects];
  for (NSData* chunk in chunks) {
    [self.audioEngine schedulePlaybackData:chunk];
  }
}

@end
