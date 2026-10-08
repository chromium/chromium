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
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_player.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_player_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_recorder.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_recorder_delegate.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_manager.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_manager_delegate.h"

// Domain for errors originated by TTCAudioEngine.
NSString* const kTTCAudioEngineErrorDomain = @"org.chromium.ttc.audio";

namespace {

// Test audio tone generation constants.
// Generates a 440Hz sine wave at 24kHz in 20ms chunks (480 samples each)
// for 1.0 second (50 total chunks).
constexpr double kTestToneFrequency = 440.0;
constexpr double kTestToneSampleRate = 24000.0;
constexpr size_t kTestToneChunkSampleCount = 480;
constexpr size_t kTestToneTotalChunks = 50;
constexpr double kTestToneAmplitude = 8000.0;

}  // namespace

@interface TTCAudioEngine () <TTCAudioRecorderDelegate,
                              TTCAudioPlayerDelegate,
                              TTCAudioSessionManagerDelegate>
@end

@implementation TTCAudioEngine {
  // Audio engine graph managing hardware input/output nodes.
  AVAudioEngine* _audioEngine;

  // Audio recorder component managing microphone tap, resampling, and RMS.
  TTCAudioRecorder* _recorder;

  // Audio player component managing response scheduling, buffer conversion,
  // and buffer drain.
  TTCAudioPlayer* _player;

  // Audio session manager handling routing, port selection, and notifications.
  TTCAudioSessionManager* _sessionManager;

  // Task runner for the sequence this engine is bound to.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Flag indicating whether microphone capture is active.
  BOOL _isCapturing;

  // Flag tracking whether asynchronous audio session configuration and
  // engine startup are currently in-flight.
  BOOL _isStarting;

  // Whether microphone input is routed directly to the speaker for local
  // testing.
  BOOL _loopbackEnabled;

  // Whether synthesized streaming playback (e.g. test tone or model voice) is
  // currently active on the player node.
  BOOL _isStreamingPlaybackActive;

  // Allows unit tests running in headless or mock environments without physical
  // audio hardware to simulate that the audio engine is running.
  BOOL _isAudioEngineRunningForTesting;

  // Tracks whether the audio engine has been disconnected.
  BOOL _isDisconnected;

  SEQUENCE_CHECKER(_sequenceChecker);
}

@synthesize delegate = _delegate;
@synthesize loopbackEnabled = _loopbackEnabled;

#pragma mark - Properties

- (BOOL)isCapturing {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _isCapturing;
}

- (BOOL)isPlaying {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _player.isPlaying;
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
  return _sessionManager.outputDestination ==
         TTCAudioOutputDestination::kSpeaker;
}

#pragma mark - Lifecycle

- (instancetype)initWithRecorder:(TTCAudioRecorder*)recorder
                          player:(TTCAudioPlayer*)player
                  sessionManager:(TTCAudioSessionManager*)sessionManager {
  self = [super init];
  if (self) {
    if (base::SequencedTaskRunner::HasCurrentDefault()) {
      _taskRunner = base::SequencedTaskRunner::GetCurrentDefault();
    }
    _audioEngine = [[AVAudioEngine alloc] init];
    _recorder = recorder ?: [[TTCAudioRecorder alloc] init];
    _recorder.delegate = self;
    _player = player ?: [[TTCAudioPlayer alloc] init];
    _player.delegate = self;
    [_player attachToAudioEngine:_audioEngine error:nil];
    _sessionManager = sessionManager ?: [[TTCAudioSessionManager alloc] init];
    _sessionManager.delegate = self;
    _isCapturing = NO;
    _isStarting = NO;
    _loopbackEnabled = NO;
    _isStreamingPlaybackActive = NO;
    _isDisconnected = NO;
    [self registerConfigurationChangeObserver];
  }
  return self;
}

- (instancetype)init {
  return [self initWithRecorder:nil player:nil sessionManager:nil];
}

#pragma mark - Public

- (void)startCaptureWithCompletion:(void (^)(BOOL success,
                                             NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isCapturing) {
    if (completion) {
      completion(YES, nil);
    }
    return;
  }

  if (_isStarting) {
    if (completion) {
      NSError* inFlightError = [NSError
          errorWithDomain:kTTCAudioEngineErrorDomain
                     code:static_cast<NSInteger>(
                              TTCAudioEngineErrorCode::kStartupCancelled)
                 userInfo:@{
                   NSLocalizedDescriptionKey :
                       @"Audio capture startup is already in flight."
                 }];
      completion(NO, inFlightError);
    }
    return;
  }

  _isStarting = YES;

  __weak TTCAudioEngine* weakSelf = self;
  [self requestMicrophonePermissionWithCompletion:^(BOOL granted) {
    [weakSelf didRequestPermissionWithGranted:granted completion:completion];
  }];
}

- (void)didRequestPermissionWithGranted:(BOOL)granted
                             completion:(void (^)(BOOL success,
                                                  NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_isStarting) {
    if (completion) {
      NSError* cancelError = [NSError
          errorWithDomain:kTTCAudioEngineErrorDomain
                     code:static_cast<NSInteger>(
                              TTCAudioEngineErrorCode::kStartupCancelled)
                 userInfo:@{
                   NSLocalizedDescriptionKey :
                       @"Audio capture startup was cancelled."
                 }];
      completion(NO, cancelError);
    }
    return;
  }

  if (!granted) {
    _isStarting = NO;
    if (completion) {
      NSError* permError = [NSError
          errorWithDomain:kTTCAudioEngineErrorDomain
                     code:static_cast<NSInteger>(
                              TTCAudioEngineErrorCode::kPermissionDenied)
                 userInfo:@{
                   NSLocalizedDescriptionKey : @"Microphone permission denied"
                 }];
      completion(NO, permError);
    }
    return;
  }

  [self proceedWithStartCaptureWithCompletion:completion];
}

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

  // If completion is not nil, then wraps it in base::BindPostTask(...) to
  // ensure it will execute on the current sequence even if CoreAudio call
  // the completion from a background thread. Otherwise create a block that
  // does nothing (no need to perform a thread hop in that case).
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

- (void)proceedWithStartCaptureWithCompletion:
    (void (^)(BOOL success, NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  __weak TTCAudioEngine* weakSelf = self;
  [_sessionManager
      configureAudioSessionWithCompletion:^(NSError* sessionError) {
        [weakSelf didFinishAudioSessionConfigurationWithError:sessionError
                                                   completion:completion];
      }];
}

- (void)stopCapture {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_isCapturing && !_isStarting) {
    return;
  }

  // Cancel any pending startup sequence.
  _isStarting = NO;

  if (!_isCapturing) {
    return;
  }

  AVAudioInputNode* inputNode = nil;
  @try {
    inputNode = _audioEngine.inputNode;
  } @catch (NSException* exception) {
  }
  if (inputNode) {
    [_recorder removeTapFromInputNode:inputNode];
  }

  _isCapturing = NO;

  if (!_player.isPlaying && _audioEngine.isRunning) {
    [_audioEngine stop];
  }

  if ([self.delegate
          respondsToSelector:@selector(audioControllerDidStopCapture:)]) {
    [self.delegate audioControllerDidStopCapture:self];
  }
}

- (void)playStreamingAudioChunk:(NSData*)pcm24kData {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (pcm24kData.length == 0) {
    return;
  }

  NSError* engineError = nil;
  if (![self ensureEngineRunningWithError:&engineError]) {
    if ([self.delegate
            respondsToSelector:@selector(audioController:didEncounterError:)]) {
      [self.delegate audioController:self didEncounterError:engineError];
    }
    return;
  }

  if (!_isStreamingPlaybackActive) {
    // If loopback buffers were queued on the player node, flush them so
    // synthesized audio begins immediately without delay.
    [_player stopPlaybackImmediately];
    _isStreamingPlaybackActive = YES;
  }

  [_player playStreamingAudioChunk:pcm24kData];
}

- (void)stopPlaybackImmediately {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isStreamingPlaybackActive = NO;
  [_player stopPlaybackImmediately];
  if (!_isCapturing && !_isStarting && _audioEngine.isRunning) {
    [_audioEngine stop];
  }
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
  if (_isDisconnected) {
    return;
  }
  _isDisconnected = YES;

  // Clear external delegate immediately to prevent dispatching callbacks with
  // a deallocating or disconnecting instance.
  self.delegate = nil;

  [self unregisterConfigurationChangeObserver];
  _isStarting = NO;
  _isStreamingPlaybackActive = NO;
  [self stopCapture];
  [self stopPlaybackImmediately];
  if (_audioEngine.isRunning) {
    [_audioEngine stop];
  }
  _sessionManager.delegate = nil;
  [_sessionManager disconnect];
  _recorder.delegate = nil;
  [_recorder reset];
  _player.delegate = nil;
  [_player detachFromAudioEngine:_audioEngine];
  [_player reset];
}

#pragma mark - Testing

- (void)setIsCapturingForTesting:(BOOL)isCapturing {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isCapturing = isCapturing;
}

- (void)setIsAudioEngineRunningForTesting:(BOOL)isRunning {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isAudioEngineRunningForTesting = isRunning;
}

- (AVAudioEngine*)audioEngineForTesting {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _audioEngine;
}

#pragma mark - TTCAudioRecorderDelegate

- (void)audioRecorder:(TTCAudioRecorder*)recorder
    didUpdateInputEnergy:(float)energy {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_isCapturing) {
    return;
  }
  if ([self.delegate
          respondsToSelector:@selector(
                                 audioController:didUpdateInputEnergy:)]) {
    [self.delegate audioController:self didUpdateInputEnergy:energy];
  }
}

- (void)audioRecorder:(TTCAudioRecorder*)recorder
     didCaptureBuffer:(AVAudioPCMBuffer*)buffer {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_isCapturing) {
    return;
  }
  if (_loopbackEnabled && !_isStreamingPlaybackActive &&
      buffer.frameLength > 0) {
    [_player playPCMBuffer:buffer];
  }

  if ([self.delegate
          respondsToSelector:@selector(
                                 audioController:didCaptureAudioChunk:)]) {
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
      [self.delegate audioController:self didCaptureAudioChunk:pcmData];
    }
  }
}

#pragma mark - TTCAudioPlayerDelegate

- (void)audioPlayerDidStartPlayback:(TTCAudioPlayer*)player {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if ([self.delegate
          respondsToSelector:@selector(audioControllerDidStartPlayback:)]) {
    [self.delegate audioControllerDidStartPlayback:self];
  }
}

- (void)audioPlayerDidStopPlayback:(TTCAudioPlayer*)player {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isStreamingPlaybackActive = NO;
  if (!_isCapturing && !_isStarting && _audioEngine.isRunning) {
    [_audioEngine stop];
  }
  if ([self.delegate
          respondsToSelector:@selector(audioControllerDidStopPlayback:)]) {
    [self.delegate audioControllerDidStopPlayback:self];
  }
}

- (void)audioPlayer:(TTCAudioPlayer*)player didEncounterError:(NSError*)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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
  if ([self.delegate
          respondsToSelector:@selector(audioControllerDidChangeRoute:)]) {
    [self.delegate audioControllerDidChangeRoute:self];
  }
}

#pragma mark - Private

// Registers `self` as an observer for
// `AVAudioEngineConfigurationChangeNotification` scoped to `_audioEngine`.
- (void)registerConfigurationChangeObserver {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected || !_audioEngine) {
    return;
  }
  [self unregisterConfigurationChangeObserver];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleEngineConfigurationChange:)
             name:AVAudioEngineConfigurationChangeNotification
           object:_audioEngine];
}

// Unregisters `self` from `AVAudioEngineConfigurationChangeNotification`.
- (void)unregisterConfigurationChangeObserver {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:AVAudioEngineConfigurationChangeNotification
              object:_audioEngine];
}

// Handles `AVAudioEngineConfigurationChangeNotification` when the audio
// engine's hardware configuration changes. Reinstalls the capture tap if
// capturing, restarts the engine, and resumes active playback.
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
  if (_isDisconnected) {
    return;
  }
  if (!_isCapturing && !_player.isPlaying) {
    return;
  }

  if (_isCapturing && !_isAudioEngineRunningForTesting) {
    AVAudioInputNode* inputNode = nil;
    @try {
      inputNode = _audioEngine.inputNode;
    } @catch (NSException* exception) {
    }
    if (inputNode) {
      [_recorder removeTapFromInputNode:inputNode];
    }
    NSError* tapError = nil;
    if (![self startEngineAndInstallTapWithError:&tapError]) {
      _isCapturing = NO;
      if ([self.delegate
              respondsToSelector:@selector(audioControllerDidStopCapture:)]) {
        [self.delegate audioControllerDidStopCapture:self];
      }
      if (tapError &&
          [self.delegate
              respondsToSelector:@selector(
                                     audioController:didEncounterError:)]) {
        [self.delegate audioController:self didEncounterError:tapError];
      }
      if (!_player.isPlaying) {
        return;
      }
    }
  }

  if (_player.isPlaying) {
    if (!_audioEngine.isRunning && !_isAudioEngineRunningForTesting) {
      [_audioEngine prepare];
      NSError* startError = nil;
      if (![_audioEngine startAndReturnError:&startError]) {
        [_player stopPlaybackImmediately];
        if (startError &&
            [self.delegate
                respondsToSelector:@selector(
                                       audioController:didEncounterError:)]) {
          [self.delegate audioController:self didEncounterError:startError];
        }
        return;
      }
    }
    [_player resumePlaybackAfterEngineRestart];
  }
}

// Ensures the audio session is configured and the AVAudioEngine graph is
// running before scheduling playback buffers.
- (BOOL)ensureEngineRunningWithError:(NSError**)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_audioEngine.isRunning || _isAudioEngineRunningForTesting) {
    return YES;
  }

  NSError* sessionError = [_sessionManager configureAudioSession];
  if (sessionError) {
    if (error) {
      *error = sessionError;
    }
    return NO;
  }

  [_audioEngine prepare];
  BOOL started = [_audioEngine startAndReturnError:error];
  if (!started) {
    [_sessionManager restoreAudioSessionCategory];
  }
  return started;
}

// Handles completion of background audio session configuration on the main
// thread, starting the audio engine and invoking `completion`.
- (void)didFinishAudioSessionConfigurationWithError:(NSError*)error
                                         completion:(void (^)(BOOL, NSError*))
                                                        completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // If startup was cancelled while the background task was in flight, abort
  // and restore the audio session category.
  if (!_isStarting) {
    [_sessionManager restoreAudioSessionCategory];
    if (completion) {
      NSError* cancelledError = [NSError
          errorWithDomain:kTTCAudioEngineErrorDomain
                     code:static_cast<NSInteger>(
                              TTCAudioEngineErrorCode::kStartupCancelled)
                 userInfo:@{
                   NSLocalizedDescriptionKey :
                       @"Audio capture startup was cancelled."
                 }];
      completion(NO, cancelledError);
    }
    return;
  }
  _isStarting = NO;

  if (error) {
    [_sessionManager restoreAudioSessionCategory];
    if (completion) {
      completion(NO, error);
    }
    return;
  }

  NSError* startError = nil;
  BOOL startSuccess = [self startEngineAndInstallTapWithError:&startError];
  if (!startSuccess) {
    [_sessionManager restoreAudioSessionCategory];
  } else if ([self.delegate
                 respondsToSelector:@selector(
                                        audioControllerDidStartCapture:)]) {
    [self.delegate audioControllerDidStartCapture:self];
  }
  if (completion) {
    completion(startSuccess, startError);
  }
}

// Verifies the hardware input node is accessible, installs the audio recorder
// tap, and starts the AVAudioEngine audio processing graph.
- (BOOL)startEngineAndInstallTapWithError:(NSError**)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isAudioEngineRunningForTesting) {
    _isCapturing = YES;
    return YES;
  }
  AVAudioInputNode* inputNode = nil;
  @try {
    inputNode = _audioEngine.inputNode;
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

  if (![_recorder installTapOnInputNode:inputNode error:error]) {
    return NO;
  }

  if (!_audioEngine.isRunning) {
    NSError* engineError = nil;
    if (![_audioEngine startAndReturnError:&engineError]) {
      [_recorder removeTapFromInputNode:inputNode];
      if (error) {
        *error = engineError;
      }
      return NO;
    }
  }

  _isCapturing = YES;
  return YES;
}

@end
