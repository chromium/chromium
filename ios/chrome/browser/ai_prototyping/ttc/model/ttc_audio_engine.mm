// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_engine.h"

#import <AVFAudio/AVFAudio.h>

#import <cmath>
#import <vector>

#import "base/functional/bind.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_player.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_recorder.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace {

// Test audio tone generation constants.
// Generates a 440Hz sine wave at 24kHz in 20ms chunks (480 samples each)
// for 1.0 second (50 total chunks).
constexpr double kTestToneFrequency = 440.0;
constexpr double kTestToneSampleRate = 24000.0;
constexpr size_t kTestToneChunkSampleCount = 480;
constexpr size_t kTestToneTotalChunks = 50;
constexpr double kTestToneAmplitude = 8000.0;

// Domain for errors originated by TTCAudioEngine.
NSString* const kTTCAudioEngineErrorDomain = @"org.chromium.ttc.audio";

// Error codes for TTCAudioEngine.
constexpr NSInteger kErrorCodeInputNodeUnavailable = -1;
constexpr NSInteger kErrorCodeStartupCancelled = -2;

}  // namespace

@interface TTCAudioEngine () <TTCAudioRecorderDelegate, TTCAudioPlayerDelegate>
@end

@implementation TTCAudioEngine {
  // Audio session category active before TalkToChrome was initialized, restored
  // upon disconnect to preserve the user's prior audio session state.
  AVAudioSessionCategory _previousCategory;

  // Audio engine graph managing hardware input/output nodes.
  AVAudioEngine* _audioEngine;

  // Audio recorder component managing microphone tap, resampling, and RMS.
  TTCAudioRecorder* _recorder;

  // Audio player component managing response scheduling, buffer conversion,
  // and buffer drain.
  TTCAudioPlayer* _player;

  // Flag indicating whether microphone capture is active.
  BOOL _isRecording;

  // Flag tracking whether asynchronous audio session configuration and
  // engine startup are currently pending on base::ThreadPool.
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
}

@synthesize loopbackEnabled = _loopbackEnabled;

- (BOOL)isRecording {
  return _isRecording;
}

- (BOOL)isPlaying {
  return _player.isPlaying;
}

- (instancetype)initWithRecorder:(TTCAudioRecorder*)recorder
                          player:(TTCAudioPlayer*)player {
  self = [super init];
  if (self) {
    _audioEngine = [[AVAudioEngine alloc] init];
    _recorder = recorder;
    _recorder.delegate = self;
    _player = player;
    _player.delegate = self;
    [_player attachToAudioEngine:_audioEngine error:nil];
    _isRecording = NO;
    _isStarting = NO;
    _loopbackEnabled = NO;
    _isStreamingPlaybackActive = NO;
  }
  return self;
}

- (instancetype)initWithRecorder:(TTCAudioRecorder*)recorder {
  return [self initWithRecorder:recorder player:[[TTCAudioPlayer alloc] init]];
}

- (instancetype)init {
  return [self initWithRecorder:[[TTCAudioRecorder alloc] init]
                         player:[[TTCAudioPlayer alloc] init]];
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)requestMicrophonePermissionWithCompletion:
    (void (^)(BOOL granted))completion {
  AVAudioApplication* app = [AVAudioApplication sharedInstance];
  if (app.recordPermission == AVAudioApplicationRecordPermissionGranted) {
    if (completion) {
      completion(YES);
    }
    return;
  }

  [AVAudioApplication
      requestRecordPermissionWithCompletionHandler:^(BOOL granted) {
        if (!completion) {
          return;
        }
        web::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                                   completion(granted);
                                                 }));
      }];
}

- (void)startRecordingWithCompletion:(void (^)(BOOL success,
                                               NSError* error))completion {
  if (_isRecording || _isStarting) {
    if (completion) {
      completion(_isRecording, nil);
    }
    return;
  }

  _isStarting = YES;

  // Cache previous category on the UI thread before hopping to ThreadPool.
  if (!_previousCategory) {
    _previousCategory = [AVAudioSession sharedInstance].category;
  }

  __weak TTCAudioEngine* weakSelf = self;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(^{
        TTCAudioEngine* strongSelf = weakSelf;
        if (!strongSelf) {
          return [NSError errorWithDomain:kTTCAudioEngineErrorDomain
                                     code:kErrorCodeStartupCancelled
                                 userInfo:nil];
        }
        return [strongSelf configureAudioSession];
      }),
      base::BindOnce(^(NSError* sessionError) {
        TTCAudioEngine* strongSelf = weakSelf;
        if (strongSelf) {
          [strongSelf didFinishAudioSessionConfigurationWithError:sessionError
                                                       completion:completion];
        } else if (completion) {
          completion(NO, sessionError);
        }
      }));
}

- (void)stopRecording {
  if (!_isRecording && !_isStarting) {
    return;
  }

  // Cancel any pending startup sequence.
  _isStarting = NO;

  if (!_isRecording) {
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

  _isRecording = NO;

  if (!_player.isPlaying && _audioEngine.isRunning) {
    [_audioEngine stop];
  }

  if ([self.delegate
          respondsToSelector:@selector(audioEngineDidStopRecording:)]) {
    [self.delegate audioEngineDidStopRecording:self];
  }
}

- (void)playStreamingAudioChunk:(NSData*)pcm24kData {
  if (pcm24kData.length == 0) {
    return;
  }

  NSError* engineError = nil;
  if (![self ensureEngineRunningWithError:&engineError]) {
    if ([self.delegate
            respondsToSelector:@selector(audioEngine:didEncounterError:)]) {
      [self.delegate audioEngine:self didEncounterError:engineError];
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
  _isStreamingPlaybackActive = NO;
  [_player stopPlaybackImmediately];
  if (!_isRecording && !_isStarting && _audioEngine.isRunning) {
    [_audioEngine stop];
  }
}

- (void)playTestTone {
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
  [self stopPlaybackImmediately];
}

- (void)setIsRecordingForTesting:(BOOL)isRecording {
  _isRecording = isRecording;
}

- (void)setIsAudioEngineRunningForTesting:(BOOL)isRunning {
  _isAudioEngineRunningForTesting = isRunning;
}

- (void)disconnect {
  _isStarting = NO;
  _isStreamingPlaybackActive = NO;
  [self stopRecording];
  [self stopPlaybackImmediately];
  if (_audioEngine.isRunning) {
    [_audioEngine stop];
  }
  _recorder.delegate = nil;
  [_recorder reset];
  _player.delegate = nil;
  [_player detachFromAudioEngine:_audioEngine];
  [_player reset];
  [self restoreAudioSessionCategory];
}

#pragma mark - TTCAudioRecorderDelegate

- (void)audioRecorder:(TTCAudioRecorder*)recorder
    didUpdateInputEnergy:(float)energy {
  if ([self.delegate
          respondsToSelector:@selector(audioEngine:didUpdateInputEnergy:)]) {
    [self.delegate audioEngine:self didUpdateInputEnergy:energy];
  }
}

- (void)audioRecorder:(TTCAudioRecorder*)recorder
     didCaptureBuffer:(AVAudioPCMBuffer*)buffer {
  if (_loopbackEnabled && !_isStreamingPlaybackActive &&
      buffer.frameLength > 0) {
    [_player playPCMBuffer:buffer];
  }
}

#pragma mark - TTCAudioPlayerDelegate

- (void)audioPlayerDidStartPlayback:(TTCAudioPlayer*)player {
  if ([self.delegate
          respondsToSelector:@selector(audioEngineDidStartPlayback:)]) {
    [self.delegate audioEngineDidStartPlayback:self];
  }
}

- (void)audioPlayerDidStopPlayback:(TTCAudioPlayer*)player {
  _isStreamingPlaybackActive = NO;
  if (!_isRecording && !_isStarting && _audioEngine.isRunning) {
    [_audioEngine stop];
  }
  if ([self.delegate
          respondsToSelector:@selector(audioEngineDidStopPlayback:)]) {
    [self.delegate audioEngineDidStopPlayback:self];
  }
}

- (void)audioPlayer:(TTCAudioPlayer*)player didEncounterError:(NSError*)error {
  if ([self.delegate
          respondsToSelector:@selector(audioEngine:didEncounterError:)]) {
    [self.delegate audioEngine:self didEncounterError:error];
  }
}

#pragma mark - Private

// Configures the AVAudioSession for simultaneous recording and playback,
// defaulting to speaker and enabling Bluetooth routes. Returns an error if
// configuration or session activation fails.
// NOTE: When mic loopback is enabled on a physical device with the built-in
// speaker, acoustic coupling between speaker and microphone can cause feedback.
// Headphones or AirPods are strongly recommended for local loopback testing.
- (NSError*)configureAudioSession {
  NSError* error = nil;
  AVAudioSession* session = [AVAudioSession sharedInstance];

  AVAudioSessionCategoryOptions options =
      AVAudioSessionCategoryOptionDefaultToSpeaker |
      AVAudioSessionCategoryOptionAllowBluetoothHFP |
      AVAudioSessionCategoryOptionAllowBluetoothA2DP;

  if (session.category != AVAudioSessionCategoryPlayAndRecord ||
      session.categoryOptions != options) {
    [session setCategory:AVAudioSessionCategoryPlayAndRecord
                    mode:AVAudioSessionModeDefault
                 options:options
                   error:&error];
  }

  if (!error) {
    [session setActive:YES error:&error];
  }

  return error;
}

// Ensures the audio session is configured and the AVAudioEngine graph is
// running before scheduling playback buffers.
- (BOOL)ensureEngineRunningWithError:(NSError**)error {
  if (_audioEngine.isRunning || _isAudioEngineRunningForTesting) {
    return YES;
  }

  if (!_previousCategory) {
    _previousCategory = [AVAudioSession sharedInstance].category;
  }

  NSError* sessionError = [self configureAudioSession];
  if (sessionError) {
    if (error) {
      *error = sessionError;
    }
    return NO;
  }

  [_audioEngine prepare];
  return [_audioEngine startAndReturnError:error];
}

// Handles completion of background audio session configuration on the main
// thread, starting the audio engine and invoking `completion`.
- (void)didFinishAudioSessionConfigurationWithError:(NSError*)error
                                         completion:(void (^)(BOOL, NSError*))
                                                        completion {
  // If startup was cancelled while the background task was in flight, abort
  // and restore the audio session category.
  if (!_isStarting) {
    [self restoreAudioSessionCategory];
    if (completion) {
      NSError* cancelledError =
          [NSError errorWithDomain:kTTCAudioEngineErrorDomain
                              code:kErrorCodeStartupCancelled
                          userInfo:@{
                            NSLocalizedDescriptionKey :
                                @"Audio recording startup was cancelled."
                          }];
      completion(NO, cancelledError);
    }
    return;
  }
  _isStarting = NO;

  if (error) {
    [self restoreAudioSessionCategory];
    if (completion) {
      completion(NO, error);
    }
    return;
  }

  NSError* startError = nil;
  BOOL startSuccess = [self startEngineAndInstallTapWithError:&startError];
  if (!startSuccess) {
    [self restoreAudioSessionCategory];
  } else if ([self.delegate
                 respondsToSelector:@selector(audioEngineDidStartRecording:)]) {
    [self.delegate audioEngineDidStartRecording:self];
  }
  if (completion) {
    completion(startSuccess, startError);
  }
}

// Restores the previous AVAudioSession category on a background thread via
// base::ThreadPool when the audio engine disconnects.
- (void)restoreAudioSessionCategory {
  AVAudioSessionCategory previousCategory = _previousCategory;
  _previousCategory = nil;
  if (!previousCategory) {
    return;
  }

  auto restoreBlock = ^{
    AVAudioSession* session = [AVAudioSession sharedInstance];
    NSError* error = nil;
    [session setCategory:previousCategory error:&error];
    [session setActive:NO
           withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                 error:&error];
  };

  if (base::ThreadPoolInstance::Get()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(restoreBlock));
  } else {
    restoreBlock();
  }
}

// Verifies the hardware input node is accessible, installs the audio recorder
// tap, and starts the AVAudioEngine audio processing graph.
- (BOOL)startEngineAndInstallTapWithError:(NSError**)error {
  AVAudioInputNode* inputNode = nil;
  @try {
    inputNode = _audioEngine.inputNode;
  } @catch (NSException* exception) {
    if (error) {
      *error = [NSError errorWithDomain:kTTCAudioEngineErrorDomain
                                   code:kErrorCodeInputNodeUnavailable
                               userInfo:@{
                                 NSLocalizedDescriptionKey : exception.reason
                                     ?: @"Audio input node is unavailable."
                               }];
    }
    return NO;
  }

  if (!inputNode) {
    if (error) {
      *error = [NSError errorWithDomain:kTTCAudioEngineErrorDomain
                                   code:kErrorCodeInputNodeUnavailable
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

  _isRecording = YES;
  return YES;
}

@end
