// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_audio_session_manager.h"

#import <AVFAudio/AVFAudio.h>

#import "base/check.h"
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

// Finds the first connected external output port from the session's current
// route.
// @param session The active AVAudioSession instance to inspect.
AVAudioSessionPortDescription* FindConnectedExternalOutputPort(
    AVAudioSession* const session) {
  for (AVAudioSessionPortDescription* output in session.currentRoute.outputs) {
    if (IsExternalPortType(output.portType)) {
      return output;
    }
  }
  return nil;
}

// Returns whether an external audio accessory is currently connected to the
// device, inspecting active outputs, cached descriptors, or available inputs.
// @param session The active AVAudioSession instance to inspect.
// @param cached_external_output_port Previously observed external port.
bool IsExternalAudioConnected(
    AVAudioSession* const session,
    AVAudioSessionPortDescription* const cached_external_output_port) {
  if (FindConnectedExternalOutputPort(session) != nil) {
    return true;
  }

  // If output is overridden to the speaker, currentRoute.outputs only lists the
  // built-in speaker. Check if an external accessory is still present via
  // availableInputs or cached descriptor.
  for (AVAudioSessionPortDescription* input in session.availableInputs) {
    if (IsExternalPortType(input.portType)) {
      return true;
    }
  }
  return cached_external_output_port != nil;
}

// Applies the PlayAndRecord category, destination mode, and options to the
// shared AVAudioSession instance.
// @param mode The AVAudioSessionMode to configure.
// @param options The AVAudioSessionCategoryOptions bitmask to configure.
NSError* ApplyAudioSessionCategoryAndMode(
    AVAudioSessionMode mode,
    AVAudioSessionCategoryOptions options) {
  AVAudioSession* session = [AVAudioSession sharedInstance];
  NSError* error = nil;
  if (![session.category isEqualToString:AVAudioSessionCategoryPlayAndRecord] ||
      ![session.mode isEqualToString:mode] ||
      session.categoryOptions != options) {
    [session setCategory:AVAudioSessionCategoryPlayAndRecord
                    mode:mode
                 options:options
                   error:&error];
  }
  return error;
}

// Applies output audio port override (Speaker or None) on an active session.
// @param destination The target audio output destination.
NSError* ApplyAudioSessionPortOverride(TTCAudioOutputDestination destination) {
  AVAudioSession* session = [AVAudioSession sharedInstance];
  NSError* error = nil;
  AVAudioSessionPortOverride portOverride =
      (destination == TTCAudioOutputDestination::kSpeaker)
          ? AVAudioSessionPortOverrideSpeaker
          : AVAudioSessionPortOverrideNone;
  [session overrideOutputAudioPort:portOverride error:&error];
  return error;
}

// Configures category, mode, options, activates the session, and applies port
// override for `destination`. Returns nil on success or the NSError
// encountered.
NSError* ConfigureAndActivateAudioSession(
    AVAudioSessionMode mode,
    AVAudioSessionCategoryOptions options,
    TTCAudioOutputDestination destination) {
  NSError* error = ApplyAudioSessionCategoryAndMode(mode, options);
  if (error) {
    return error;
  }
  AVAudioSession* session = [AVAudioSession sharedInstance];
  [session setActive:YES error:&error];
  if (error) {
    return error;
  }
  return ApplyAudioSessionPortOverride(destination);
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

// Returns the audio output destination corresponding to `port`.
// @param port The audio session port description to inspect, or nil.
// @return The matching destination, defaulting to `kSpeaker` if `port` is nil
//     or an unrecognized port type.
TTCAudioOutputDestination DestinationForPort(
    AVAudioSessionPortDescription* port) {
  if (!port) {
    return TTCAudioOutputDestination::kSpeaker;
  }
  NSString* portType = port.portType;
  if (IsExternalPortType(portType)) {
    return TTCAudioOutputDestination::kExternal;
  }
  if ([portType isEqualToString:AVAudioSessionPortBuiltInReceiver]) {
    return TTCAudioOutputDestination::kEarpiece;
  }
  return TTCAudioOutputDestination::kSpeaker;
}

}  // namespace

@interface TTCAudioSessionManager ()
@property(nonatomic, readwrite) TTCAudioOutputDestination outputDestination;
@end

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

  // Monotonically increasing generation counters to ensure rapid destination
  // and preferred input changes on background threads are applied in sequence.
  uint64_t _destinationChangeGeneration;
  uint64_t _preferredInputChangeGeneration;

  // Cached description of the most recently connected external output port,
  // preserved while output is temporarily overridden to the built-in speaker.
  AVAudioSessionPortDescription* _cachedExternalOutputPort;

  // Dedicated sequenced task runner to serialize background audio session
  // operations and prevent data races on [AVAudioSession sharedInstance].
  scoped_refptr<base::SequencedTaskRunner> _audioSessionTaskRunner;

  SEQUENCE_CHECKER(_sequenceChecker);
}

@synthesize outputDestination = _outputDestination;

- (instancetype)init {
  self = [super init];
  if (self) {
    AVAudioSession* session = [AVAudioSession sharedInstance];
    _cachedExternalOutputPort = FindConnectedExternalOutputPort(session);
    _outputDestination =
        IsExternalAudioConnected(session, _cachedExternalOutputPort)
            ? TTCAudioOutputDestination::kExternal
            : TTCAudioOutputDestination::kSpeaker;
    if (base::ThreadPoolInstance::Get()) {
      _audioSessionTaskRunner = base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
    }
    [self registerNotificationObserversWithAudioEngine:nil];
  }
  return self;
}

- (void)disconnect {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _isDisconnected = YES;
  _delegate = nil;
  [self clearCachedExternalOutputPort];
  _outputDestination = TTCAudioOutputDestination::kSpeaker;
  [self unregisterNotificationObservers];
  [self restoreAudioSessionCategoryInternal];
}

#pragma mark - Properties

- (TTCAudioOutputDestination)outputDestination {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return _outputDestination;
}

- (void)setOutputDestination:(TTCAudioOutputDestination)outputDestination {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _outputDestination = outputDestination;
}

- (BOOL)hasHardwareAEC {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioSessionPortDescription* inputPort =
      [AVAudioSession sharedInstance].currentRoute.inputs.firstObject;
  return inputPort.hasHardwareVoiceCallProcessing;
}

- (BOOL)isExternalOutputConnected {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return [self connectedExternalOutputPort] != nil;
}

- (NSString*)externalOutputDeviceName {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioSessionPortDescription* port = [self connectedExternalOutputPort];
  return port ? port.portName : nil;
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

- (NSArray<AVAudioSessionPortDescription*>*)availableInputs {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return [AVAudioSession sharedInstance].availableInputs ?: @[];
}

- (AVAudioSessionPortDescription*)preferredInput {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  return [AVAudioSession sharedInstance].preferredInput;
}

#pragma mark - Public

- (void)registerNotificationObserversWithAudioEngine:(AVAudioEngine*)engine {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    return;
  }
  [self unregisterNotificationObservers];

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(handleRouteChangeNotification:)
                 name:AVAudioSessionRouteChangeNotification
               object:nil];
  [center addObserver:self
             selector:@selector(handleInterruptionNotification:)
                 name:AVAudioSessionInterruptionNotification
               object:nil];
  if (engine) {
    [center addObserver:self
               selector:@selector(handleEngineConfigurationChangeNotification:)
                   name:AVAudioEngineConfigurationChangeNotification
                 object:engine];
  }
}

- (void)unregisterNotificationObservers {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self
                    name:AVAudioSessionRouteChangeNotification
                  object:nil];
  [center removeObserver:self
                    name:AVAudioSessionInterruptionNotification
                  object:nil];
  [center removeObserver:self
                    name:AVAudioEngineConfigurationChangeNotification
                  object:nil];
}

- (NSError*)configureAudioSession {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    return CreateCancelledError();
  }

  [self recordPreviousAudioSessionStateIfNeeded];

  TTCAudioOutputDestination destination = self.outputDestination;
  AVAudioSessionMode mode = [self modeForDestination:destination];
  AVAudioSessionCategoryOptions options =
      [self categoryOptionsForDestination:destination];

  NSError* error = ConfigureAndActivateAudioSession(mode, options, destination);
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

  [self recordPreviousAudioSessionStateIfNeeded];

  TTCAudioOutputDestination destination = self.outputDestination;
  AVAudioSessionMode mode = [self modeForDestination:destination];
  AVAudioSessionCategoryOptions options =
      [self categoryOptionsForDestination:destination];

  if (!_audioSessionTaskRunner) {
    NSError* error = [self configureAudioSession];
    if (completion) {
      completion(error);
    }
    return;
  }

  __weak TTCAudioSessionManager* weakSelf = self;
  auto configureBlock = ^{
    return ConfigureAndActivateAudioSession(mode, options, destination);
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

- (BOOL)setOutputDestination:(TTCAudioOutputDestination)destination
                       error:(NSError**)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    if (error) {
      *error = CreateCancelledError();
    }
    return NO;
  }

  [self recordPreviousAudioSessionStateIfNeeded];
  _destinationChangeGeneration++;

  AVAudioSessionMode mode = [self modeForDestination:destination];
  AVAudioSessionCategoryOptions options =
      [self categoryOptionsForDestination:destination];

  NSError* localError = ApplyAudioSessionCategoryAndMode(mode, options);
  if (!localError) {
    localError = ApplyAudioSessionPortOverride(destination);
  }

  if (localError) {
    if (error) {
      *error = localError;
    }
    return NO;
  }

  self.outputDestination = destination;
  [self notifyRouteChanged];
  [self notifyEngineReconfigurationRequested];
  return YES;
}

- (void)setOutputDestination:(TTCAudioOutputDestination)destination
                  completion:
                      (void (^)(BOOL success, NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    if (completion) {
      completion(NO, CreateCancelledError());
    }
    return;
  }

  [self recordPreviousAudioSessionStateIfNeeded];
  uint64_t generation = ++_destinationChangeGeneration;

  AVAudioSessionMode mode = [self modeForDestination:destination];
  AVAudioSessionCategoryOptions options =
      [self categoryOptionsForDestination:destination];

  if (!_audioSessionTaskRunner) {
    NSError* sessionError = ApplyAudioSessionCategoryAndMode(mode, options);
    if (!sessionError) {
      sessionError = ApplyAudioSessionPortOverride(destination);
    }
    if (sessionError) {
      if (completion) {
        completion(NO, sessionError);
      }
      return;
    }

    self.outputDestination = destination;
    [self notifyRouteChanged];
    [self notifyEngineReconfigurationRequested];
    if (completion) {
      completion(YES, nil);
    }
    return;
  }

  __weak TTCAudioSessionManager* weakSelf = self;
  auto taskBlock = ^{
    NSError* sessionError = ApplyAudioSessionCategoryAndMode(mode, options);
    if (!sessionError) {
      sessionError = ApplyAudioSessionPortOverride(destination);
    }
    return sessionError;
  };

  _audioSessionTaskRunner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(taskBlock),
      base::BindOnce(^(NSError* sessionError) {
        TTCAudioSessionManager* strongSelf = weakSelf;
        if (!strongSelf) {
          if (completion) {
            completion(NO, CreateCancelledError());
          }
          return;
        }
        [strongSelf handleDestinationChangeResult:sessionError
                                       generation:generation
                                   newDestination:destination
                                       completion:completion];
      }));
}

- (BOOL)setOutputOverriddenToSpeaker:(BOOL)forceSpeaker error:(NSError**)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  TTCAudioOutputDestination dest =
      [self destinationForSpeakerOverride:forceSpeaker];
  return [self setOutputDestination:dest error:error];
}

- (void)setOutputOverriddenToSpeaker:(BOOL)forceSpeaker
                          completion:(void (^)(BOOL success,
                                               NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  TTCAudioOutputDestination dest =
      [self destinationForSpeakerOverride:forceSpeaker];
  [self setOutputDestination:dest completion:completion];
}

- (BOOL)setPreferredInput:(AVAudioSessionPortDescription*)port
                    error:(NSError**)error {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    if (error) {
      *error = CreateCancelledError();
    }
    return NO;
  }

  _preferredInputChangeGeneration++;
  AVAudioSession* session = [AVAudioSession sharedInstance];
  BOOL success = [session setPreferredInput:port error:error];
  if (success) {
    [self notifyRouteChanged];
    [self notifyEngineReconfigurationRequested];
  }
  return success;
}

- (void)setPreferredInput:(AVAudioSessionPortDescription*)port
               completion:(void (^)(BOOL success, NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    if (completion) {
      completion(NO, CreateCancelledError());
    }
    return;
  }

  uint64_t generation = ++_preferredInputChangeGeneration;

  if (!_audioSessionTaskRunner) {
    NSError* sessionError = nil;
    BOOL success = [self setPreferredInput:port error:&sessionError];
    if (completion) {
      completion(success, sessionError);
    }
    return;
  }

  __weak TTCAudioSessionManager* weakSelf = self;
  auto taskBlock = ^{
    AVAudioSession* session = [AVAudioSession sharedInstance];
    NSError* sessionError = nil;
    [session setPreferredInput:port error:&sessionError];
    return sessionError;
  };

  _audioSessionTaskRunner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(taskBlock),
      base::BindOnce(^(NSError* sessionError) {
        TTCAudioSessionManager* strongSelf = weakSelf;
        if (!strongSelf) {
          if (completion) {
            completion(NO, CreateCancelledError());
          }
          return;
        }
        [strongSelf handlePreferredInputResult:sessionError
                                    generation:generation
                                    completion:completion];
      }));
}

#pragma mark - Private

// Invalidates the cached external output descriptor when an accessory is
// disconnected or during teardown to prevent routing to stale hardware ports.
- (void)clearCachedExternalOutputPort {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _cachedExternalOutputPort = nil;
}

// Returns destination based on speaker override state and connected devices.
// @param forceSpeaker YES if speaker output is explicitly requested.
- (TTCAudioOutputDestination)destinationForSpeakerOverride:(BOOL)forceSpeaker {
  return forceSpeaker ? TTCAudioOutputDestination::kSpeaker
                      : (self.isExternalOutputConnected
                             ? TTCAudioOutputDestination::kExternal
                             : TTCAudioOutputDestination::kEarpiece);
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

// Returns the AVAudioSessionMode corresponding to `destination`.
// Speaker uses VideoChat for AEC over loudspeaker; earpiece and external
// accessories use VoiceChat for VoIP AEC.
// @param destination Target audio output destination.
- (AVAudioSessionMode)modeForDestination:
    (TTCAudioOutputDestination)destination {
  switch (destination) {
    case TTCAudioOutputDestination::kSpeaker:
      return AVAudioSessionModeVideoChat;
    case TTCAudioOutputDestination::kEarpiece:
    case TTCAudioOutputDestination::kExternal:
      return AVAudioSessionModeVoiceChat;
  }
}

// Returns the AVAudioSessionCategoryOptions corresponding to `destination`.
// @param destination Target audio output destination.
- (AVAudioSessionCategoryOptions)categoryOptionsForDestination:
    (TTCAudioOutputDestination)destination {
  switch (destination) {
    case TTCAudioOutputDestination::kSpeaker:
      return AVAudioSessionCategoryOptionAllowAirPlay |
             AVAudioSessionCategoryOptionAllowBluetoothA2DP |
             AVAudioSessionCategoryOptionAllowBluetoothHFP |
             AVAudioSessionCategoryOptionDefaultToSpeaker;
    case TTCAudioOutputDestination::kExternal:
      return AVAudioSessionCategoryOptionAllowAirPlay |
             AVAudioSessionCategoryOptionAllowBluetoothA2DP |
             AVAudioSessionCategoryOptionAllowBluetoothHFP;
    case TTCAudioOutputDestination::kEarpiece:
      // Omit DefaultToSpeaker and Bluetooth options so audio routes to
      // receiver.
      return 0;
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
    NSString* inputName = self.activeInputRouteName ?: @"No Input";
    NSString* outputName = self.activeOutputRouteName ?: @"No Output";
    NSString* routeDescription =
        [NSString stringWithFormat:@"In: %@ | Out: %@", inputName, outputName];
    [delegate audioSessionManager:self
        didChangeRouteDescription:routeDescription
                   hasHardwareAEC:self.hasHardwareAEC];
  }
}

// Finds and caches the first connected external output port, checking both
// active outputs and available inputs when output is overridden to the speaker.
- (AVAudioSessionPortDescription*)connectedExternalOutputPort {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AVAudioSession* session = [AVAudioSession sharedInstance];
  AVAudioSessionPortDescription* activeOutput =
      FindConnectedExternalOutputPort(session);
  if (activeOutput) {
    _cachedExternalOutputPort = activeOutput;
    return activeOutput;
  }

  // If output is overridden to the speaker, currentRoute.outputs only lists the
  // built-in speaker. Check if an external accessory is still connected via
  // availableInputs.
  for (AVAudioSessionPortDescription* input in session.availableInputs) {
    if (IsExternalPortType(input.portType)) {
      return _cachedExternalOutputPort ?: input;
    }
  }

  return _cachedExternalOutputPort;
}

// Handles the result of an asynchronous destination change task.
// Validates generation counter and disconnection state, updates destination
// upon success, and dispatches completion.
// @param sessionError The NSError returned from the background configuration.
// @param generation Generation counter when the task was dispatched.
// @param newDestination Destination to apply upon success.
// @param completion Block invoked on the UI thread with the result.
- (void)handleDestinationChangeResult:(NSError*)sessionError
                           generation:(uint64_t)generation
                       newDestination:(TTCAudioOutputDestination)newDestination
                           completion:(void (^)(BOOL success,
                                                NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    if (completion) {
      completion(NO, CreateCancelledError());
    }
    return;
  }

  if (_destinationChangeGeneration != generation) {
    if (completion) {
      completion(NO, CreateCancelledError());
    }
    return;
  }

  if (sessionError) {
    if (completion) {
      completion(NO, sessionError);
    }
    return;
  }

  self.outputDestination = newDestination;
  [self notifyRouteChanged];
  [self notifyEngineReconfigurationRequested];
  if (completion) {
    completion(YES, nil);
  }
}

// Handles the result of an asynchronous setPreferredInput task.
// Validates generation counter and disconnection state, notifies delegate on
// success, and dispatches completion.
// @param sessionError The NSError returned from the background task, if any.
// @param generation Generation counter when the task was dispatched.
// @param completion Block invoked on the UI thread with the result.
- (void)handlePreferredInputResult:(NSError*)sessionError
                        generation:(uint64_t)generation
                        completion:
                            (void (^)(BOOL success, NSError* error))completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    if (completion) {
      completion(NO, CreateCancelledError());
    }
    return;
  }

  if (_preferredInputChangeGeneration != generation) {
    if (completion) {
      completion(NO, CreateCancelledError());
    }
    return;
  }

  if (sessionError) {
    if (completion) {
      completion(NO, sessionError);
    }
    return;
  }

  [self notifyRouteChanged];
  [self notifyEngineReconfigurationRequested];
  if (completion) {
    completion(YES, nil);
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
  } else {
    [self notifyRouteChanged];
  }

  if (completion) {
    completion(error);
  }
}

// Restores the previous audio session category, mode, and categoryOptions that
// were recorded before TalkToChrome configuration, clears port overrides,
// deactivates the session, and clears cached state.
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
    [session overrideOutputAudioPort:AVAudioSessionPortOverrideNone
                               error:&error];
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

// Handles AVAudioSessionRouteChangeNotification received from AVFoundation on
// arbitrary CoreAudio notification threads, validating the payload and
// dispatching to the UI thread.
// @param notification The route change notification posted by AVFoundation.
- (void)handleRouteChangeNotification:(NSNotification*)notification {
  if (!web::WebThread::IsThreadInitialized(web::WebThread::UI)) {
    return;
  }

  NSDictionary* userInfo = notification.userInfo;
  NSNumber* reasonValue = userInfo[AVAudioSessionRouteChangeReasonKey];
  if (!reasonValue) {
    return;
  }
  AVAudioSessionRouteChangeReason reason =
      static_cast<AVAudioSessionRouteChangeReason>(
          [reasonValue unsignedIntegerValue]);

  __weak TTCAudioSessionManager* weakSelf = self;
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf handleRouteChangeWithReason:reason];
      }));
}

// Handles an audio route change on the UI thread for the specified reason.
// Adapts destination for new/removed devices, and notifies the delegate.
// @param reason The route change reason reported by AVFoundation.
- (void)handleRouteChangeWithReason:(AVAudioSessionRouteChangeReason)reason {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    return;
  }

  if (reason == AVAudioSessionRouteChangeReasonNewDeviceAvailable) {
    AVAudioSessionPortDescription* externalOutput =
        [self connectedExternalOutputPort];
    if (externalOutput) {
      _cachedExternalOutputPort = externalOutput;
      [self setOutputDestination:TTCAudioOutputDestination::kExternal
                      completion:nil];
    } else {
      [self notifyRouteChanged];
    }
  } else if (reason == AVAudioSessionRouteChangeReasonOldDeviceUnavailable) {
    if (!self.isExternalOutputConnected) {
      [self clearCachedExternalOutputPort];
      [self setOutputDestination:TTCAudioOutputDestination::kSpeaker
                      completion:nil];
    } else {
      [self notifyRouteChanged];
    }
  } else {
    // Port overrides and audio category updates should not trigger
    // engine reconfigurations, which cause audio dropouts and spurious
    // port override resets.
    if (reason == AVAudioSessionRouteChangeReasonOverride ||
        reason == AVAudioSessionRouteChangeReasonCategoryChange) {
      [self notifyRouteChanged];
      return;
    }

    AVAudioSession* session = [AVAudioSession sharedInstance];
    AVAudioSessionPortDescription* activeOutput =
        session.currentRoute.outputs.firstObject;
    TTCAudioOutputDestination activeDestination =
        DestinationForPort(activeOutput);

    if (activeDestination != self.outputDestination) {
      if (activeDestination == TTCAudioOutputDestination::kExternal) {
        _cachedExternalOutputPort = activeOutput;
      }
      [self setOutputDestination:activeDestination completion:nil];
      return;
    }

    if (activeDestination == TTCAudioOutputDestination::kExternal &&
        activeOutput) {
      _cachedExternalOutputPort = activeOutput;
    }

    [self notifyRouteChanged];
    // A route change with the same logical destination may indicate a hardware
    // parameter change (e.g. sample rate or channel layout) requiring audio
    // engine reconfiguration.
    [self notifyEngineReconfigurationRequested];
  }
}

// Handles an audio engine configuration change on the UI thread.
// Notifies the delegate that the engine's audio graph requires reconfiguration.
- (void)handleEngineConfigurationChange {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (_isDisconnected) {
    return;
  }
  [self notifyEngineReconfigurationRequested];
}

// Handles AVAudioEngineConfigurationChangeNotification received from
// AVFoundation when the audio engine's hardware configuration changes.
// Stops the engine's audio graph and requires rebuilding or restarting taps.
// @param notification The configuration change notification posted by
// AVAudioEngine.
- (void)handleEngineConfigurationChangeNotification:
    (NSNotification*)notification {
  if (!web::WebThread::IsThreadInitialized(web::WebThread::UI)) {
    return;
  }

  __weak TTCAudioSessionManager* weakSelf = self;
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf handleEngineConfigurationChange];
      }));
}

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
