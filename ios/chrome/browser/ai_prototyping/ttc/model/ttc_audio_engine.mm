// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_engine.h"

#import <AVFAudio/AVFAudio.h>

#import "base/functional/bind.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_recorder.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

namespace {

// Domain for errors originated by TTCAudioEngine.
NSString* const kTTCAudioEngineErrorDomain = @"org.chromium.ttc.audio";

// Error codes for TTCAudioEngine.
constexpr NSInteger kErrorCodeInputNodeUnavailable = -1;
constexpr NSInteger kErrorCodeStartupCancelled = -2;

}  // namespace

@interface TTCAudioEngine () <TTCAudioRecorderDelegate>
@end

@implementation TTCAudioEngine {
  // Audio session category active before TalkToChrome was initialized, restored
  // upon disconnect to preserve the user's prior audio session state.
  AVAudioSessionCategory _previousCategory;

  // Audio engine graph managing hardware input/output nodes.
  AVAudioEngine* _audioEngine;

  // Audio recorder component managing microphone tap, resampling, and RMS.
  TTCAudioRecorder* _recorder;

  // Flag indicating whether microphone capture is active.
  BOOL _isRecording;

  // Flag tracking whether asynchronous audio session configuration and
  // engine startup are currently pending on base::ThreadPool.
  BOOL _isStarting;
}

- (BOOL)isRecording {
  return _isRecording;
}

- (instancetype)initWithRecorder:(TTCAudioRecorder*)recorder {
  self = [super init];
  if (self) {
    _audioEngine = [[AVAudioEngine alloc] init];
    _recorder = recorder;
    _recorder.delegate = self;
    _isRecording = NO;
    _isStarting = NO;
  }
  return self;
}

- (instancetype)init {
  return [self initWithRecorder:[[TTCAudioRecorder alloc] init]];
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

  if (_audioEngine.isRunning) {
    [_audioEngine stop];
  }

  if ([self.delegate
          respondsToSelector:@selector(audioEngineDidStopRecording:)]) {
    [self.delegate audioEngineDidStopRecording:self];
  }
}

- (void)setIsRecordingForTesting:(BOOL)isRecording {
  _isRecording = isRecording;
}

- (void)disconnect {
  _isStarting = NO;
  [self stopRecording];
  _recorder.delegate = nil;
  [_recorder reset];
  [self restoreAudioSessionCategory];
}

#pragma mark - TTCAudioRecorderDelegate

- (void)audioRecorder:(TTCAudioRecorder*)recorder
    didUpdateInputEnergy:(float)rms {
  if ([self.delegate
          respondsToSelector:@selector(audioEngine:didUpdateInputEnergy:)]) {
    [self.delegate audioEngine:self didUpdateInputEnergy:rms];
  }
}

#pragma mark - Private

// Configures the AVAudioSession for simultaneous recording and playback,
// defaulting to speaker and enabling Bluetooth routes. Returns an error if
// configuration or session activation fails.
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

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(^{
        AVAudioSession* session = [AVAudioSession sharedInstance];
        NSError* error = nil;
        [session setCategory:previousCategory error:&error];
        [session
              setActive:NO
            withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                  error:&error];
      }));
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
