// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_session_manager.h"

#import <AVFAudio/AVFAudio.h>

#import "base/functional/bind.h"
#import "base/sequence_checker.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_session_manager_delegate.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

NSString* const kTTCAudioSessionManagerErrorDomain =
    @"org.chromium.ttc.audio_session";

namespace {

// Default category options for TalkToChrome audio sessions, ordered
// alphabetically: routing audio to speaker while allowing external Bluetooth
// and AirPlay accessories.
constexpr AVAudioSessionCategoryOptions kDefaultCategoryOptions =
    AVAudioSessionCategoryOptionAllowAirPlay |
    AVAudioSessionCategoryOptionAllowBluetoothA2DP |
    AVAudioSessionCategoryOptionAllowBluetoothHFP |
    AVAudioSessionCategoryOptionDefaultToSpeaker;

// Applies the PlayAndRecord category, VoiceChat mode, and default options to
// the shared AVAudioSession instance to enable Voice Processing (AEC/AGC), and
// activates the session. Returns nil on success, or the NSError encountered.
NSError* ConfigureAndActivateAudioSession() {
  AVAudioSession* session = [AVAudioSession sharedInstance];
  NSError* error = nil;
  if (![session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord] ||
      ![session.mode isEqualToString:AVAudioSessionModeVoiceChat] ||
      session.categoryOptions != kDefaultCategoryOptions) {
    [session setCategory:AVAudioSessionCategoryPlayAndRecord
                    mode:AVAudioSessionModeVoiceChat
                 options:kDefaultCategoryOptions
                   error:&error];
    if (error) {
      return error;
    }
  }

  [session setActive:YES error:&error];
  return error;
}

// Returns a standardized NSError indicating the configuration was cancelled.
NSError* CreateCancelledError() {
  return
      [NSError errorWithDomain:kTTCAudioSessionManagerErrorDomain
                          code:static_cast<NSInteger>(
                                   TTCAudioSessionManagerErrorCode::kCancelled)
                      userInfo:@{
                        NSLocalizedDescriptionKey :
                            @"Audio session configuration was cancelled."
                      }];
}

}  // namespace

@implementation TTCAudioSessionManager {
  // Audio session state active before TalkToChrome configured the session,
  // fully restored upon teardown to preserve the user's prior audio session
  // state (category, mode, and categoryOptions).
  AVAudioSessionCategory _previousCategory;
  AVAudioSessionMode _previousMode;
  AVAudioSessionCategoryOptions _previousOptions;
  BOOL _hasPreviousState;

  // Tracks whether the session manager has been disconnected or torn down.
  BOOL _isDisconnected;

  // Dedicated sequenced task runner to serialize background audio session
  // operations and prevent data races on [AVAudioSession sharedInstance].
  scoped_refptr<base::SequencedTaskRunner> _audioSessionTaskRunner;

  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)init {
  self = [super init];
  if (self) {
    if (base::ThreadPoolInstance::Get()) {
      _audioSessionTaskRunner = base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    }
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(handleInterruptionNotification:)
               name:AVAudioSessionInterruptionNotification
             object:[AVAudioSession sharedInstance]];
  }
  return self;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isDisconnected = YES;
  _delegate = nil;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self restoreAudioSessionCategoryInternal];
}

#pragma mark - Properties

- (BOOL)hasHardwareAEC {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioSessionPortDescription* inputPort =
      [AVAudioSession sharedInstance].currentRoute.inputs.firstObject;
  return inputPort.hasHardwareVoiceCallProcessing;
}

- (NSString*)activeInputRouteName {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioSessionPortDescription* inputPort =
      [AVAudioSession sharedInstance].currentRoute.inputs.firstObject;
  return inputPort.portName;
}

- (NSString*)activeOutputRouteName {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioSessionPortDescription* outputPort =
      [AVAudioSession sharedInstance].currentRoute.outputs.firstObject;
  return outputPort.portName;
}

#pragma mark - Public

- (NSError*)configureAudioSession {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    return CreateCancelledError();
  }

  [self recordPreviousAudioSessionStateIfNeeded];

  NSError* error = ConfigureAndActivateAudioSession();
  if (error) {
    [self restoreAudioSessionCategoryInternal];
  }
  return error;
}

- (void)configureAudioSessionWithCompletion:
    (void (^)(NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    if (completion) {
      completion(CreateCancelledError());
    }
    return;
  }

  [self recordPreviousAudioSessionStateIfNeeded];

  if (!_audioSessionTaskRunner) {
    NSError* error = [self configureAudioSession];
    if (completion) {
      completion(error);
    }
    return;
  }

  __weak TTCAudioSessionManager* weakSelf = self;
  auto configureBlock = ^{
    return ConfigureAndActivateAudioSession();
  };

  _audioSessionTaskRunner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(configureBlock),
      base::BindOnce(^(NSError* error) {
        if (!weakSelf) {
          if (completion) {
            completion(CreateCancelledError());
          }
          return;
        }
        [weakSelf handleConfigureAudioSessionResult:error
                                         completion:completion];
      }));
}

- (void)restoreAudioSessionCategory {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self restoreAudioSessionCategoryInternal];
}

#pragma mark - Private

// Records the current audio session category, mode, and categoryOptions if
// previous state has not already been saved, allowing it to be restored on
// teardown.
- (void)recordPreviousAudioSessionStateIfNeeded {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_hasPreviousState) {
    AVAudioSession* session = [AVAudioSession sharedInstance];
    _previousCategory = session.category;
    _previousMode = session.mode;
    _previousOptions = session.categoryOptions;
    _hasPreviousState = YES;
  }
}

// Handles the result of an asynchronous audio session configuration task.
// Validates disconnection state on the caller sequence, rolls back previous
// state if an error was encountered, and invokes the completion handler.
// @param error The NSError returned by the background configuration task, or
// nil on success.
// @param completion Block invoked with the resulting error or cancellation.
- (void)handleConfigureAudioSessionResult:(NSError*)error
                               completion:(void (^)(NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    if (completion) {
      completion(CreateCancelledError());
    }
    return;
  }

  if (error) {
    [self restoreAudioSessionCategoryInternal];
  }

  if (completion) {
    completion(error);
  }
}

// Restores the previous audio session category, mode, and categoryOptions that
// were recorded before TalkToChrome configuration, deactivates the session, and
// clears cached state. Dispatches the restoration onto
// `_audioSessionTaskRunner` to avoid blocking the main thread.
- (void)restoreAudioSessionCategoryInternal {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_hasPreviousState) {
    return;
  }

  AVAudioSessionCategory previousCategory = _previousCategory;
  AVAudioSessionMode previousMode = _previousMode;
  AVAudioSessionCategoryOptions previousOptions = _previousOptions;
  _previousCategory = nil;
  _previousMode = nil;
  _previousOptions = 0;
  _hasPreviousState = NO;

  auto restoreBlock = ^{
    AVAudioSession* session = [AVAudioSession sharedInstance];
    NSError* error = nil;
    [session setCategory:previousCategory
                    mode:previousMode
                 options:previousOptions
                   error:&error];
    [session setActive:NO
           withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation
                 error:&error];
  };

  if (_audioSessionTaskRunner) {
    _audioSessionTaskRunner->PostTask(FROM_HERE, base::BindOnce(restoreBlock));
  } else if (base::ThreadPoolInstance::Get()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce(restoreBlock));
  } else {
    restoreBlock();
  }
}

// Handles audio session interruptions on the UI thread and notifies the
// delegate.
// @param type The type of interruption (Began or Ended).
// @param shouldResume Whether audio processing should resume if Ended.
- (void)handleInterruptionWithType:(AVAudioSessionInterruptionType)type
                      shouldResume:(BOOL)shouldResume {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    return;
  }

  id<TTCAudioSessionManagerDelegate> delegate = self.delegate;
  if (type == AVAudioSessionInterruptionTypeBegan) {
    if ([delegate
            respondsToSelector:@selector(
                                   audioSessionManagerDidBeginInterruption:)]) {
      [delegate audioSessionManagerDidBeginInterruption:self];
    }
  } else if (type == AVAudioSessionInterruptionTypeEnded) {
    if ([delegate
            respondsToSelector:
                @selector(
                    audioSessionManager:didEndInterruptionWithShouldResume:)]) {
      [delegate audioSessionManager:self
          didEndInterruptionWithShouldResume:shouldResume];
    }
  }
}

#pragma mark - Notifications

// Handles AVAudioSessionInterruptionNotification received from AVFoundation on
// arbitrary CoreAudio notification threads, validating the payload and
// dispatching to the UI thread.
// @param notification The interruption notification posted by AVFoundation.
- (void)handleInterruptionNotification:(NSNotification*)notification {
  if (!web::WebThread::IsThreadInitialized(web::WebThread::UI)) {
    return;
  }

  NSDictionary* userInfo = notification.userInfo;
  NSNumber* typeValue = userInfo[AVAudioSessionInterruptionTypeKey];
  if (!typeValue) {
    return;
  }
  AVAudioSessionInterruptionType type =
      static_cast<AVAudioSessionInterruptionType>(
          [typeValue unsignedIntegerValue]);

  NSNumber* optionValue = userInfo[AVAudioSessionInterruptionOptionKey];
  AVAudioSessionInterruptionOptions options =
      static_cast<AVAudioSessionInterruptionOptions>(
          [optionValue unsignedIntegerValue]);
  BOOL shouldResume =
      (options & AVAudioSessionInterruptionOptionShouldResume) != 0;

  __weak TTCAudioSessionManager* weakSelf = self;
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf handleInterruptionWithType:type shouldResume:shouldResume];
      }));
}

@end
