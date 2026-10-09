// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_engine.h"

#import <AVFAudio/AVFAudio.h>

#import <algorithm>
#import <cmath>
#import <vector>

#import "base/check.h"
#import "base/compiler_specific.h"
#import "base/containers/span.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/sequence_checker.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_player.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_player_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_recorder.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_recorder_delegate.h"

// Domain for errors originated by TTCAudioEngine.
NSString* const kTTCAudioEngineErrorDomain = @"org.chromium.ttc.audio";

namespace {

// Delay before restarting the audio engine after a hardware configuration
// change so route and sample-rate updates finish propagating across all audio
// nodes.
constexpr base::TimeDelta kConfigurationChangeRestartDelay =
    base::Milliseconds(200);

// Returns whether `format` has a valid non-zero sample rate and channel count.
bool IsAudioFormatValid(AVAudioFormat* format) {
  return format != nil && format.sampleRate > 0.0 && format.channelCount > 0;
}

}  // namespace

@interface TTCAudioEngine () <TTCAudioRecorderDelegate, TTCAudioPlayerDelegate>

@property(nonatomic, strong) AVAudioEngine* audioEngine;
@property(nonatomic, strong) TTCAudioRecorder* recorder;
@property(nonatomic, strong) TTCAudioPlayer* player;
@property(nonatomic, assign, readwrite, getter=isStarted) BOOL started;
@property(nonatomic, assign, readwrite, getter=isCapturing) BOOL capturing;
@property(nonatomic, assign, readwrite) float inputAudioLevel;
@property(nonatomic, assign, getter=isEngineConfigured) BOOL engineConfigured;
@property(nonatomic, assign) BOOL hasCustomAudioEngine;
@property(nonatomic, assign) BOOL isAudioEngineRunningForTesting;
@property(nonatomic, assign, getter=isDisconnected) BOOL disconnected;

@end

@implementation TTCAudioEngine {
  // Task runner for the sequence this engine is bound to.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  SEQUENCE_CHECKER(_sequenceChecker);
}

@synthesize delegate = _delegate;
@synthesize started = _started;
@synthesize aecMode = _aecMode;
@synthesize capturing = _capturing;
@synthesize inputAudioLevel = _inputAudioLevel;

#pragma mark - Properties

- (BOOL)isStarted {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _started;
}

- (void)setStarted:(BOOL)started {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _started = started;
}

- (TTCAudioAECMode)aecMode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _aecMode;
}

- (void)setAecMode:(TTCAudioAECMode)aecMode {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_aecMode == aecMode) {
    return;
  }
  _aecMode = aecMode;
  self.engineConfigured = NO;
}

- (BOOL)isCapturing {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _capturing;
}

- (void)setCapturing:(BOOL)capturing {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _capturing = capturing;
}

- (float)inputAudioLevel {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _inputAudioLevel;
}

- (void)setInputAudioLevel:(float)inputAudioLevel {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _inputAudioLevel = inputAudioLevel;
}

- (BOOL)isPlaying {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return self.player.isPlaying;
}

#pragma mark - Lifecycle

- (instancetype)initWithAudioEngine:(AVAudioEngine*)audioEngine
                           recorder:(TTCAudioRecorder*)recorder
                             player:(TTCAudioPlayer*)player {
  self = [super init];
  if (self) {
    if (base::SequencedTaskRunner::HasCurrentDefault()) {
      _taskRunner = base::SequencedTaskRunner::GetCurrentDefault();
    }
    _hasCustomAudioEngine = (audioEngine != nil);
    _audioEngine = audioEngine ?: [[AVAudioEngine alloc] init];
    _recorder = recorder ?: [[TTCAudioRecorder alloc] init];
    _recorder.delegate = self;
    _player = player ?: [[TTCAudioPlayer alloc] init];
    _player.delegate = self;
    [_player attachToAudioEngine:_audioEngine error:nil];
    _engineConfigured = YES;
    _started = NO;
    _aecMode = TTCAudioAECMode::kUnknown;
    _capturing = NO;
    _inputAudioLevel = 0.0f;
    _disconnected = NO;
    [self registerConfigurationChangeObserver];
  }
  return self;
}

- (instancetype)init {
  return [self initWithAudioEngine:nil recorder:nil player:nil];
}

#pragma mark - TTCAudioEngineProtocol

- (void)startWithCompletion:(void (^)(BOOL success, NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    if (completion) {
      NSError* cancelError = [NSError
          errorWithDomain:kTTCAudioEngineErrorDomain
                     code:static_cast<NSInteger>(
                              TTCAudioEngineErrorCode::kStartupCancelled)
                 userInfo:@{
                   NSLocalizedDescriptionKey : @"Audio engine is disconnected."
                 }];
      completion(NO, cancelError);
    }
    return;
  }

  if (self.isStarted) {
    if (completion) {
      completion(YES, nil);
    }
    return;
  }

  if (self.isAudioEngineRunningForTesting) {
    if (!self.isEngineConfigured) {
      [self.player attachToAudioEngine:self.audioEngine error:nil];
      self.engineConfigured = YES;
    }
    self.started = YES;
    if (completion) {
      completion(YES, nil);
    }
    return;
  }

  NSError* startError = nil;
  BOOL started = [self startAudioEngineWithRecoveryAndError:&startError];
  self.started = started;
  if (completion) {
    completion(started, started ? nil : startError);
  }
}

- (void)stopWithCompletion:(void (^)(BOOL success, NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self stopCapture];
  [self stopPlayback];
  [self removeRecorderTapIfNeeded];
  if (self.audioEngine.isRunning) {
    [self.audioEngine stop];
  }
  self.started = NO;
  self.inputAudioLevel = 0.0f;
  if (completion) {
    completion(YES, nil);
  }
}

- (BOOL)startCapture {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || !self.isStarted) {
    return NO;
  }
  if (self.isCapturing) {
    return YES;
  }

  NSError* tapError = nil;
  if (![self startEngineAndInstallTapWithError:&tapError]) {
    if (tapError &&
        [self.delegate
            respondsToSelector:@selector(audioEngine:didEncounterError:)]) {
      [self.delegate audioEngine:self didEncounterError:tapError];
    }
    return NO;
  }
  return YES;
}

- (void)stopCapture {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!self.isCapturing) {
    return;
  }

  self.capturing = NO;
  self.inputAudioLevel = 0.0f;
}

- (void)schedulePlaybackData:(NSData*)pcm24kData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || !self.isStarted || pcm24kData.length == 0) {
    return;
  }
  [self.player playStreamingAudioChunk:pcm24kData];
}

- (void)notifyEndOfPlaybackData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
}

- (void)stopPlayback {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self.player stopPlaybackImmediately];
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected) {
    return;
  }
  self.disconnected = YES;

  // Clear external delegate immediately to prevent dispatching callbacks with
  // a deallocating or disconnecting instance.
  self.delegate = nil;

  [self unregisterConfigurationChangeObserver];
  [self stopCapture];
  [self stopPlayback];
  [self removeRecorderTapIfNeeded];
  if (self.audioEngine.isRunning) {
    [self.audioEngine stop];
  }
  self.started = NO;
  self.engineConfigured = NO;
  self.recorder.delegate = nil;
  [self.recorder reset];
  self.player.delegate = nil;
  [self.player detachFromAudioEngine:self.audioEngine];
  [self.player reset];
}

#pragma mark - Testing

- (void)setIsCapturingForTesting:(BOOL)isCapturing {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  self.capturing = isCapturing;
}

- (void)setIsAudioEngineRunningForTesting:(BOOL)isRunning {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isAudioEngineRunningForTesting = isRunning;
  if (isRunning && !self.isEngineConfigured) {
    [self.player attachToAudioEngine:self.audioEngine error:nil];
    self.engineConfigured = YES;
  }
  self.started = isRunning;
}

- (AVAudioEngine*)audioEngineForTesting {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return self.audioEngine;
}

#pragma mark - TTCAudioRecorderDelegate

- (void)audioRecorder:(TTCAudioRecorder*)recorder
    didUpdateInputEnergy:(float)energy {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!self.isCapturing) {
    return;
  }
  self.inputAudioLevel =
      std::isfinite(energy) ? std::clamp(energy, 0.0f, 1.0f) : 0.0f;
}

- (void)audioRecorder:(TTCAudioRecorder*)recorder
     didCaptureBuffer:(AVAudioPCMBuffer*)buffer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!self.isCapturing) {
    return;
  }

  if ([self.delegate
          respondsToSelector:
              @selector(audioEngine:didCaptureAudioData:inputLevel:)]) {
    float* const* channelData = buffer.floatChannelData;
    if (channelData && buffer.format.channelCount > 0 && channelData[0] &&
        buffer.frameLength > 0) {
      AVAudioFrameCount frameCount = buffer.frameLength;
      // SAFETY: `channelData[0]` has length `frameCount` guaranteed by
      // `buffer.frameLength` from AVFoundation.
      auto samples = UNSAFE_BUFFERS(base::span(channelData[0], frameCount));
      std::vector<int16_t> pcmBuffer(frameCount);
      for (size_t i = 0; i < frameCount; ++i) {
        float rawSample = samples[i];
        float sample = std::isfinite(rawSample)
                           ? std::clamp(rawSample, -1.0f, 1.0f)
                           : 0.0f;
        pcmBuffer[i] = static_cast<int16_t>(std::lroundf(sample * 32767.0f));
      }
      NSData* pcmData =
          [NSData dataWithBytes:pcmBuffer.data()
                         length:pcmBuffer.size() * sizeof(int16_t)];
      [self.delegate audioEngine:self
             didCaptureAudioData:pcmData
                      inputLevel:self.inputAudioLevel];
    }
  }
}

#pragma mark - TTCAudioPlayerDelegate

- (void)audioPlayerDidStartPlayback:(TTCAudioPlayer*)player {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ([self.delegate
          respondsToSelector:@selector(audioEngineDidStartPlayback:)]) {
    [self.delegate audioEngineDidStartPlayback:self];
  }
}

- (void)audioPlayerDidStopPlayback:(TTCAudioPlayer*)player {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ([self.delegate
          respondsToSelector:@selector(audioEngineDidStopPlayback:)]) {
    [self.delegate audioEngineDidStopPlayback:self];
  }
}

- (void)audioPlayer:(TTCAudioPlayer*)player didEncounterError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ([self.delegate
          respondsToSelector:@selector(audioEngine:didEncounterError:)]) {
    [self.delegate audioEngine:self didEncounterError:error];
  }
}

#pragma mark - Private

// Registers `self` as an observer for
// `AVAudioEngineConfigurationChangeNotification` scoped to `self.audioEngine`.
- (void)registerConfigurationChangeObserver {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || !self.audioEngine) {
    return;
  }
  [self unregisterConfigurationChangeObserver];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleEngineConfigurationChange:)
             name:AVAudioEngineConfigurationChangeNotification
           object:self.audioEngine];
}

// Unregisters `self` from `AVAudioEngineConfigurationChangeNotification`.
- (void)unregisterConfigurationChangeObserver {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:AVAudioEngineConfigurationChangeNotification
              object:self.audioEngine];
}

// Removes the microphone tap from `self.audioEngine.inputNode` if installed.
- (void)removeRecorderTapIfNeeded {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isAudioEngineRunningForTesting) {
    return;
  }
  AVAudioInputNode* inputNode = nil;
  @try {
    inputNode = self.audioEngine.inputNode;
  } @catch (NSException* exception) {
  }
  [self.recorder removeTapFromInputNode:inputNode];
}

// Recreates `self.audioEngine` to recover from a transient CoreAudio graph or
// hardware format failure during startup.
- (void)recreateAudioEngine {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self unregisterConfigurationChangeObserver];
  [self removeRecorderTapIfNeeded];
  if (self.audioEngine.isRunning) {
    [self.audioEngine stop];
  }
  [self.player detachFromAudioEngine:self.audioEngine];
  if (!self.hasCustomAudioEngine) {
    self.audioEngine = [[AVAudioEngine alloc] init];
  }
  self.engineConfigured =
      [self.player attachToAudioEngine:self.audioEngine error:nil];
  [self registerConfigurationChangeObserver];
}

// Starts the audio engine and installs the input tap, recreating the engine
// and retrying once if the initial startup attempt fails.
- (BOOL)startAudioEngineWithRecoveryAndError:(NSError**)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ([self internalStartWithError:error]) {
    return YES;
  }
  [self recreateAudioEngine];
  return [self internalStartWithError:error];
}

// Connects `self.player` to `self.audioEngine` if needed, installs the
// microphone tap on `inputNode`, and starts the `AVAudioEngine` processing
// graph.
- (BOOL)internalStartWithError:(NSError**)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioInputNode* inputNode = nil;
  @try {
    inputNode = self.audioEngine.inputNode;
  } @catch (NSException* exception) {
    if (error) {
      *error = [NSError
          errorWithDomain:kTTCAudioEngineErrorDomain
                     code:static_cast<NSInteger>(
                              TTCAudioEngineErrorCode::kInputNodeUnavailable)
                 userInfo:@{
                   NSLocalizedDescriptionKey : exception.reason
                       ?: @"Audio input node is unavailable."
                 }];
    }
    return NO;
  }

  if (!inputNode) {
    if (error) {
      *error = [NSError
          errorWithDomain:kTTCAudioEngineErrorDomain
                     code:static_cast<NSInteger>(
                              TTCAudioEngineErrorCode::kInputNodeUnavailable)
                 userInfo:@{
                   NSLocalizedDescriptionKey :
                       @"Audio input node is unavailable."
                 }];
    }
    return NO;
  }

  AVAudioFormat* outputFormat =
      [self.audioEngine.outputNode outputFormatForBus:0];
  if (!IsAudioFormatValid(outputFormat)) {
    if (error) {
      *error = [NSError
          errorWithDomain:kTTCAudioEngineErrorDomain
                     code:static_cast<NSInteger>(
                              TTCAudioEngineErrorCode::kEngineStartFailed)
                 userInfo:@{
                   NSLocalizedDescriptionKey :
                       @"Audio output hardware is unavailable."
                 }];
    }
    return NO;
  }

  if (!self.isEngineConfigured) {
    if (![self.player attachToAudioEngine:self.audioEngine error:error]) {
      return NO;
    }
    self.engineConfigured = YES;
  }

  if (![self.recorder installTapOnInputNode:inputNode error:error]) {
    return NO;
  }

  if (!self.audioEngine.isRunning) {
    [self.audioEngine prepare];
    NSError* startError = nil;
    if (![self.audioEngine startAndReturnError:&startError]) {
      [self.recorder removeTapFromInputNode:inputNode];
      if (error) {
        *error = startError;
      }
      return NO;
    }
  }

  return YES;
}

// Handles `AVAudioEngineConfigurationChangeNotification` when the audio
// engine's hardware configuration changes. Waits 200ms on real hardware before
// reconfiguring and restarting the engine so route changes finish propagating
// across all audio nodes.
- (void)handleEngineConfigurationChange:(NSNotification*)notification {
  if (_taskRunner && !_taskRunner->RunsTasksInCurrentSequence()) {
    __weak __typeof(self) weakSelf = self;
    _taskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                            [weakSelf
                                handleEngineConfigurationChange:notification];
                          }));
    return;
  }
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || !self.isStarted) {
    return;
  }

  if (self.isAudioEngineRunningForTesting) {
    [self applyEngineConfigurationChange];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf applyEngineConfigurationChange];
      }),
      kConfigurationChangeRestartDelay);
}

// Reinstalls the capture tap, restarts the audio engine, and resumes active
// playback after an `AVAudioEngineConfigurationChangeNotification`.
- (void)applyEngineConfigurationChange {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isDisconnected || !self.isStarted) {
    return;
  }

  if (!self.isAudioEngineRunningForTesting) {
    self.engineConfigured = NO;
    [self removeRecorderTapIfNeeded];
    NSError* startError = nil;
    if (![self startAudioEngineWithRecoveryAndError:&startError]) {
      self.started = NO;
      self.capturing = NO;
      self.inputAudioLevel = 0.0f;
      [self.player stopPlaybackImmediately];
      if (startError &&
          [self.delegate
              respondsToSelector:@selector(audioEngine:didEncounterError:)]) {
        [self.delegate audioEngine:self didEncounterError:startError];
      }
      return;
    }
  }

  if (self.player.isPlaying) {
    [self.player resumePlaybackAfterEngineRestart];
  }
}

// Verifies the hardware input node is accessible, ensures the audio recorder
// tap is installed, and starts the AVAudioEngine audio processing graph if
// needed.
- (BOOL)startEngineAndInstallTapWithError:(NSError**)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (self.isAudioEngineRunningForTesting) {
    self.capturing = YES;
    return YES;
  }

  if (!self.audioEngine.isRunning) {
    if (![self startAudioEngineWithRecoveryAndError:error]) {
      return NO;
    }
  } else {
    AVAudioInputNode* inputNode = nil;
    @try {
      inputNode = self.audioEngine.inputNode;
    } @catch (NSException* exception) {
      if (error) {
        *error = [NSError
            errorWithDomain:kTTCAudioEngineErrorDomain
                       code:static_cast<NSInteger>(
                                TTCAudioEngineErrorCode::kInputNodeUnavailable)
                   userInfo:@{
                     NSLocalizedDescriptionKey : exception.reason
                         ?: @"Audio input node is unavailable."
                   }];
      }
      return NO;
    }

    if (!inputNode) {
      if (error) {
        *error = [NSError
            errorWithDomain:kTTCAudioEngineErrorDomain
                       code:static_cast<NSInteger>(
                                TTCAudioEngineErrorCode::kInputNodeUnavailable)
                   userInfo:@{
                     NSLocalizedDescriptionKey :
                         @"Audio input node is unavailable."
                   }];
      }
      return NO;
    }

    if (![self.recorder installTapOnInputNode:inputNode error:error]) {
      return NO;
    }
  }

  self.capturing = YES;
  return YES;
}

@end
