// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_manager.h"

#import <AVFAudio/AVFAudio.h>

#import <utility>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/sequence_checker.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/task/thread_pool/thread_pool_instance.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/audio/ttc_audio_session_manager_delegate.h"

NSString* const kTTCAudioSessionManagerErrorDomain =
    @"org.chromium.ttc.audio_session";

namespace {

// Returns whether `port_type` corresponds to an external accessory (Bluetooth,
// wired headphones, USB-C audio, or AirPlay), checked in alphabetical order.
bool IsExternalPortType(NSString* const port_type) {
  return [port_type isEqualToString:AVAudioSessionPortAirPlay] ||
         [port_type isEqualToString:AVAudioSessionPortBluetoothA2DP] ||
         [port_type isEqualToString:AVAudioSessionPortBluetoothHFP] ||
         [port_type isEqualToString:AVAudioSessionPortBluetoothLE] ||
         [port_type isEqualToString:AVAudioSessionPortCarAudio] ||
         [port_type isEqualToString:AVAudioSessionPortHeadphones] ||
         [port_type isEqualToString:AVAudioSessionPortHeadsetMic] ||
         [port_type isEqualToString:AVAudioSessionPortUSBAudio];
}

// Configures PlayAndRecord category, VideoChat mode, DefaultToSpeaker +
// Bluetooth/AirPlay options, and activates the shared AVAudioSession instance.
NSError* ConfigureAndActivateAudioSession() {
  AVAudioSession* session = [AVAudioSession sharedInstance];
  AVAudioSessionMode mode = AVAudioSessionModeVideoChat;
  AVAudioSessionCategoryOptions options =
      AVAudioSessionCategoryOptionAllowAirPlay |
      AVAudioSessionCategoryOptionAllowBluetoothA2DP |
      AVAudioSessionCategoryOptionAllowBluetoothHFP |
      AVAudioSessionCategoryOptionDefaultToSpeaker;
  NSError* error = nil;
  if (![session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord] ||
      ![session.mode isEqualToString:mode] ||
      session.categoryOptions != options) {
    [session setCategory:AVAudioSessionCategoryPlayAndRecord
                    mode:mode
                 options:options
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
  // Audio session state active before TTC configured the session,
  // fully restored upon teardown to preserve the user's prior audio session
  // state (category, mode, and categoryOptions).
  AVAudioSessionCategory _previousCategory;
  AVAudioSessionMode _previousMode;
  AVAudioSessionCategoryOptions _previousOptions;
  BOOL _hasPreviousState;

  // Tracks whether the session manager has been disconnected or torn down.
  BOOL _isDisconnected;

  // Used to store the handle for registered notification with the
  // NSNotificationCenter.
  NSMutableArray* _notificationHandles;

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
    [self registerNotificationObservers];
  }
  return self;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isDisconnected = YES;
  _delegate = nil;
  [self unregisterNotificationObservers];
  [self restoreAudioSessionCategoryInternal];
}

#pragma mark - Properties

- (BOOL)hasHardwareAEC {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioSessionPortDescription* inputPort =
      [AVAudioSession sharedInstance].currentRoute.inputs.firstObject;
  return inputPort.hasHardwareVoiceCallProcessing;
}

- (BOOL)isOutputRoutedToSpeaker {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioSessionPortDescription* outputPort =
      [AVAudioSession sharedInstance].currentRoute.outputs.firstObject;
  if (!outputPort) {
    return YES;
  }
  NSString* portType = outputPort.portType;
  return !IsExternalPortType(portType) &&
         ![portType isEqualToString:AVAudioSessionPortBuiltInReceiver];
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
    return error;
  }

  [self notifyRouteChanged];
  return nil;
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

  if (!_audioSessionTaskRunner) {
    NSError* error = [self configureAudioSession];
    if (completion) {
      completion(error);
    }
    return;
  }

  [self recordPreviousAudioSessionStateIfNeeded];

  __weak TTCAudioSessionManager* weakSelf = self;
  auto configureBlock = ^{
    return ConfigureAndActivateAudioSession();
  };

  _audioSessionTaskRunner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(configureBlock),
      base::BindOnce(^(NSError* error) {
        TTCAudioSessionManager* strongSelf = weakSelf;
        if (!strongSelf) {
          if (completion) {
            completion(CreateCancelledError());
          }
          return;
        }
        [strongSelf handleConfigureAudioSessionResult:error
                                           completion:completion];
      }));
}

- (void)restoreAudioSessionCategory {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [self restoreAudioSessionCategoryInternal];
}

#pragma mark - Private

- (void)registerNotificationObservers {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    return;
  }
  [self unregisterNotificationObservers];

  [self registerSelector:@selector(handleRouteChangeNotification:)
         forNotification:AVAudioSessionRouteChangeNotification
                  object:nil];
  [self registerSelector:@selector(handleInterruptionNotification:)
         forNotification:AVAudioSessionInterruptionNotification
                  object:nil];
}

- (void)unregisterNotificationObservers {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_notificationHandles) {
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    for (id handle in std::exchange(_notificationHandles, nil)) {
      [center removeObserver:handle];
    }
  }
}

// Registers `selector` on `self` to be invoked when notification `name`
// is posted. This wrapper around -addObserver:selector:name:object:
// ensures that the selector is called on the correct sequence (because
// AVAudioSession may post the notification on a background thread).
- (void)registerSelector:(SEL)selector
         forNotification:(NSNotificationName)name
                  object:(id)object {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  CHECK([self respondsToSelector:selector]);

  __weak __typeof(self) weakSelf = self;
  base::RepeatingCallback<void(NSNotification*)> callback = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(
          [](id target, SEL selector, NSNotification* notification) {
            if (target) {
              if (IMP method = [target methodForSelector:selector]) {
                using Function = void (*)(id, SEL, NSNotification*);
                Function function = reinterpret_cast<Function>(method);
                function(target, selector, notification);
              }
            }
          },
          weakSelf, selector));

  if (id handle = [[NSNotificationCenter defaultCenter]
          addObserverForName:name
                      object:object
                       queue:nil
                  usingBlock:base::CallbackToBlock(std::move(callback))]) {
    if (!_notificationHandles) {
      _notificationHandles = [[NSMutableArray alloc] init];
    }
    [_notificationHandles addObject:handle];
  }
}

// Captures audio session state prior to modification if not already captured.
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

// Notifies the delegate of the current route description and AEC status.
- (void)notifyRouteChanged {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  id<TTCAudioSessionManagerDelegate> delegate = self.delegate;
  if ([delegate
          respondsToSelector:@selector(
                                 audioSessionManager:didChangeRouteDescription:
                                 hasHardwareAEC:)]) {
    AVAudioSessionRouteDescription* route =
        [AVAudioSession sharedInstance].currentRoute;
    NSString* inputName = route.inputs.firstObject.portName ?: @"No Input";
    NSString* outputName = route.outputs.firstObject.portName ?: @"No Output";
    NSString* routeDescription =
        [NSString stringWithFormat:@"In: %@ | Out: %@", inputName, outputName];
    [delegate audioSessionManager:self
        didChangeRouteDescription:routeDescription
                   hasHardwareAEC:self.hasHardwareAEC];
  }
}

// Notifies the delegate that an audio engine reconfiguration is required.
- (void)notifyEngineReconfigurationRequested {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  id<TTCAudioSessionManagerDelegate> delegate = self.delegate;
  if ([delegate
          respondsToSelector:
              @selector(audioSessionManagerDidRequireEngineReconfiguration:)]) {
    [delegate audioSessionManagerDidRequireEngineReconfiguration:self];
  }
}

// Handles the result of an asynchronous audio session configuration task.
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
  } else {
    [self notifyRouteChanged];
  }

  if (completion) {
    completion(error);
  }
}

// Restores the previous audio session category, mode, and categoryOptions that
// were recorded before TTC configuration, deactivates the session, and clears
// cached state.
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
  } else {
    restoreBlock();
  }
}

// Handles audio session interruptions on the UI thread and notifies the
// delegate.
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

- (void)handleRouteChangeNotification:(NSNotification*)notification {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  NSDictionary* userInfo = notification.userInfo;
  NSNumber* reasonValue = userInfo[AVAudioSessionRouteChangeReasonKey];
  if (!reasonValue) {
    return;
  }
  AVAudioSessionRouteChangeReason reason =
      static_cast<AVAudioSessionRouteChangeReason>(
          [reasonValue unsignedIntegerValue]);
  [self handleRouteChangeWithReason:reason];
}

// Handles an audio route change on the UI thread for the specified reason.
- (void)handleRouteChangeWithReason:(AVAudioSessionRouteChangeReason)reason {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    return;
  }

  [self notifyRouteChanged];

  // Only request engine reconfiguration when a hardware device is connected or
  // disconnected. Category changes, port overrides, and internal route
  // configuration updates (which CoreAudio posts during session activation and
  // engine startup) should not trigger engine restarts.
  if (reason == AVAudioSessionRouteChangeReasonNewDeviceAvailable ||
      reason == AVAudioSessionRouteChangeReasonOldDeviceUnavailable) {
    [self notifyEngineReconfigurationRequested];
  }
}

- (void)handleInterruptionNotification:(NSNotification*)notification {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
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

  [self handleInterruptionWithType:type shouldResume:shouldResume];
}

@end
