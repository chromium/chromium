// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/app_state.h"

#import <utility>

#import "base/apple/foundation_util.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/strings/sys_string_conversions.h"
#import "base/types/cxx23_to_underlying.h"
#import "ios/chrome/app/application_delegate/app_state+Testing.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/deferred_initialization_queue.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/deferred_initialization_task_names.h"
#import "ios/chrome/browser/crash_report/model/crash_keys_helper.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"

namespace {

// Returns YES if the UIApplication is currently in the background, regardless
// of where it is in the lifecycle.
BOOL ApplicationIsInBackground() {
  return [[UIApplication sharedApplication] applicationState] ==
         UIApplicationStateBackground;
}
}  // namespace

#pragma mark - AppStateObserverList

@interface AppStateObserverList : CRBProtocolObservers <AppStateObserver>
@end

@implementation AppStateObserverList

+ (instancetype)list {
  return [self observersWithProtocol:@protocol(AppStateObserver)];
}

@end

#pragma mark - UIBlockerManagerObserverList

@interface UIBlockerManagerObserverList
    : CRBProtocolObservers <UIBlockerManagerObserver>
@end

@implementation UIBlockerManagerObserverList

+ (instancetype)list {
  return [self observersWithProtocol:@protocol(UIBlockerManagerObserver)];
}

@end

#pragma mark - AppState

@implementation AppState {
  // Container for observers.
  AppStateObserverList* _observers;

  // Container for observers.
  UIBlockerManagerObserverList* _uiBlockerManagerObservers;

  // List of connected profileStates.
  NSMutableArray<ProfileState*>* _profileStates;

  // Agents attached to this app state.
  NSMutableArray<id<AppStateAgent>>* _agents;

  // The current blocker target if any.
  __weak id<UIBlockerTarget> _uiBlockerTarget;

  // Counter of number of object that want to force the device in the
  // portrait orientation (orientation is locked if non-zero).
  NSUInteger _forcePortraitOrientationCounter;

  // Counter of currently shown blocking UIs.
  NSUInteger _blockingUICounter;

  // Whether the application is currently in the background.
  // This is a workaround for rdar://22392526 where
  // -applicationDidEnterBackground: can be called twice.
  // TODO(crbug.com/41211311): Remove this once rdar://22392526 is fixed.
  BOOL _applicationInBackground;

  // A flag that tracks if the init stage is currently being incremented. Used
  // to prevent reentrant calls to queueTransitionToNextInitStage originating
  // from stage change notifications.
  BOOL _isIncrementingInitStage;

  // A flag that tracks if another increment of init stage needs to happen after
  // this one is complete. Will be set if queueTransitionToNextInitStage is
  // called while queueTransitionToNextInitStage is already on the call stack.
  BOOL _needsIncrementInitStage;
}

- (instancetype)initWithStartupInformation:
    (id<StartupInformation>)startupInformation {
  self = [super init];
  if (self) {
    _observers = [AppStateObserverList list];
    _uiBlockerManagerObservers = [UIBlockerManagerObserverList list];
    _profileStates = [[NSMutableArray alloc] init];
    _agents = [[NSMutableArray alloc] init];
    _startupInformation = startupInformation;
    _appCommandDispatcher = [[CommandDispatcher alloc] init];
    _deferredRunner = [[DeferredInitializationRunner alloc]
        initWithQueue:[DeferredInitializationQueue sharedInstance]];

    // Subscribe to scene connection notifications.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(sceneWillConnect:)
               name:UISceneWillConnectNotification
             object:nil];

    // Observe the status of VoiceOver for crash logging.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(voiceOverStatusDidChange:)
               name:UIAccessibilityVoiceOverStatusDidChangeNotification
             object:nil];
    crash_keys::SetVoiceOverRunning(UIAccessibilityIsVoiceOverRunning());
  }
  return self;
}

#pragma mark - Properties implementation

- (void)setUiBlockerTarget:(id<UIBlockerTarget>)uiBlockerTarget {
  _uiBlockerTarget = uiBlockerTarget;
  for (SceneState* scene in self.connectedScenes) {
    // When there's a scene with blocking UI, all other scenes should show the
    // overlay.
    BOOL shouldPresentOverlay =
        (uiBlockerTarget != nil) && (scene != uiBlockerTarget);
    scene.presentingModalOverlay = shouldPresentOverlay;
  }
}

// Do not use this setter directly, instead use -queueTransitionToInitStage:
// that provides reentry guards.
- (void)setInitStage:(AppInitStage)newInitStage {
  DCHECK_GE(newInitStage, AppInitStage::kStart);
  DCHECK_LE(newInitStage, AppInitStage::kFinal);
  // As of writing this, it seems reasonable for init stages to be strictly
  // incremented by one only: if a stage needs to be skipped, it can just be a
  // no-op, but the observers will get a chance to react to it normally. If in
  // the future these need to be skipped, or go backwards:
  // 1. Check that all observers will support this change
  // 2. Keep the previous init stage and modify addObserver: code to send the
  // previous init stage instead.
  if (newInitStage == AppInitStage::kStart) {
    DCHECK_EQ(_initStage, AppInitStage::kStart);
  } else {
    DCHECK_EQ(base::to_underlying(newInitStage),
              base::to_underlying(_initStage) + 1);
  }

  AppInitStage previousInitStage = _initStage;
  [_observers appState:self willTransitionToInitStage:newInitStage];
  [self updateInitStage:newInitStage];
  [_observers appState:self didTransitionFromInitStage:previousInitStage];
}

// Side-effect free setter, exposed in the +Testing category. Outside of tests,
// this should only be called from the -setInitStage: implementation above.
- (void)updateInitStage:(AppInitStage)initStage {
  _initStage = initStage;
}

- (BOOL)portraitOnly {
  return _forcePortraitOrientationCounter > 0;
}

- (NSArray<id<AppStateAgent>>*)connectedAgents {
  return [_agents copy];
}

#pragma mark - Public methods.

- (void)addObserver:(id<AppStateObserver>)observer {
  [_observers addObserver:observer];

  if ([observer respondsToSelector:@selector(appState:
                                       didTransitionFromInitStage:)] &&
      _initStage > AppInitStage::kStart) {
    AppInitStage previousInitStage =
        static_cast<AppInitStage>(base::to_underlying(_initStage) - 1);
    // Trigger an update on the newly added agent.
    [observer appState:self didTransitionFromInitStage:previousInitStage];
  }
}

- (void)removeObserver:(id<AppStateObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)profileStateCreated:(ProfileState*)profileState {
  [_profileStates addObject:profileState];
  [_observers appState:self profileStateConnected:profileState];
}

- (void)profileStateDestroyed:(ProfileState*)profileState {
  [_profileStates removeObject:profileState];
  [_observers appState:self profileStateDisconnected:profileState];
}

- (void)addAgent:(id<AppStateAgent>)agent {
  DCHECK(agent);
  [_agents addObject:agent];
  [agent setAppState:self];
}

- (void)removeAgent:(id<AppStateAgent>)agent {
  DCHECK(agent);
  DCHECK([_agents containsObject:agent]);
  [_agents removeObject:agent];
}

- (void)queueTransitionToNextInitStage {
  DCHECK_LT(_initStage, AppInitStage::kFinal);
  AppInitStage nextInitStage =
      static_cast<AppInitStage>(base::to_underlying(_initStage) + 1);
  [self queueTransitionToInitStage:nextInitStage];
}

- (void)startInitialization {
  [self queueTransitionToInitStage:AppInitStage::kStart];
}

#pragma mark - Multiwindow-related

- (SceneState*)foregroundActiveScene {
  for (SceneState* sceneState in self.connectedScenes) {
    if (sceneState.activationLevel == SceneActivationLevelForegroundActive) {
      return sceneState;
    }
  }

  return nil;
}

- (NSArray<SceneState*>*)connectedScenes {
  NSMutableArray* sceneStates = [[NSMutableArray alloc] init];
  NSSet* connectedScenes = [UIApplication sharedApplication].connectedScenes;
  for (UIWindowScene* scene in connectedScenes) {
    if (![scene.delegate isKindOfClass:[SceneDelegate class]]) {
      // This might happen in tests.
      // TODO(crbug.com/40710078): This shouldn't be needed. (It might also
      // be the cause of crbug.com/1142782).
      [sceneStates addObject:[[SceneState alloc] initWithAppState:self]];
      continue;
    }

    SceneDelegate* sceneDelegate =
        base::apple::ObjCCastStrict<SceneDelegate>(scene.delegate);
    [sceneStates addObject:sceneDelegate.sceneState];
  }
  return sceneStates;
}

- (NSArray<SceneState*>*)foregroundScenes {
  return [self.connectedScenes
      filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(
                                                   SceneState* scene,
                                                   NSDictionary* bindings) {
        return scene.activationLevel >= SceneActivationLevelForegroundInactive;
      }]];
}

- (NSArray<ProfileState*>*)profileStates {
  return [_profileStates copy];
}

#pragma mark - Internal methods.

- (void)queueTransitionToInitStage:(AppInitStage)initStage {
  if (_isIncrementingInitStage) {
    // It is an error to queue more than one transition at once.
    DCHECK(!_needsIncrementInitStage);

    // Set a flag to increment after the observers are notified of the current
    // change.
    _needsIncrementInitStage = YES;
    return;
  }

  _isIncrementingInitStage = YES;
  [self setInitStage:initStage];
  _isIncrementingInitStage = NO;

  if (_needsIncrementInitStage) {
    _needsIncrementInitStage = NO;
    [self queueTransitionToNextInitStage];
  }
}

#pragma mark - BackgroundRefreshAudience

- (void)backgroundRefreshDidStart {
  // If  refresh is starting, and the app is in the background, then let the
  // application state know so it can enable the clean exit beacon while work
  // is underway.
  if (ApplicationIsInBackground()) {
    // Background refresh events can be triggered at odd times in the startup/
    // shutdown cycle, so always ensure that the app context exists.
    ApplicationContext* applicationContext = GetApplicationContext();
    if (applicationContext) {
      applicationContext->OnAppStartedBackgroundProcessing();
    }
  }
}

- (void)backgroundRefreshDidEnd {
  // If  refresh has completed, and the app is in the background, then let the
  // application state know so it can disable the clean exit beacon. If iOS
  // kills the app in the background at this point it should not be a crash for
  // the purposes of metrics or experiments.
  if (ApplicationIsInBackground()) {
    // Background refresh events can be triggered at odd times in the startup/
    // shutdown cycle, so always ensure that the app context exists.
    ApplicationContext* applicationContext = GetApplicationContext();
    if (applicationContext) {
      applicationContext->OnAppFinishedBackgroundProcessing();
    }
  }
}

#pragma mark - PortraitOrientationManager

- (void)incrementForcePortraitOrientationCounter {
  if (!_forcePortraitOrientationCounter) {
    for (SceneState* sceneState in self.connectedScenes) {
      [sceneState.browserProviderInterface.currentBrowserProvider
              .viewController setNeedsUpdateOfSupportedInterfaceOrientations];
    }
  }
  ++_forcePortraitOrientationCounter;
}

- (void)decrementForcePortraitOrientationCounter {
  CHECK_GT(_forcePortraitOrientationCounter, 0ul);
  --_forcePortraitOrientationCounter;
  if (!_forcePortraitOrientationCounter) {
    for (SceneState* sceneState in self.connectedScenes) {
      [sceneState.browserProviderInterface.currentBrowserProvider
              .viewController setNeedsUpdateOfSupportedInterfaceOrientations];
    }
  }
}

#pragma mark - UIBlockerManager

- (void)incrementBlockingUICounterForTarget:(id<UIBlockerTarget>)target {
  CHECK(_uiBlockerTarget == nil || target == _uiBlockerTarget)
      << "Another scene is already showing a blocking UI!";
  _blockingUICounter++;
  [self setUiBlockerTarget:target];
}

- (void)decrementBlockingUICounterForTarget:(id<UIBlockerTarget>)target {
  CHECK_GT(_blockingUICounter, 0u);
  CHECK_EQ(_uiBlockerTarget, target);
  if (--_blockingUICounter == 0) {
    [self setUiBlockerTarget:nil];
    [_uiBlockerManagerObservers currentUIBlockerRemoved];
  }
}

- (id<UIBlockerTarget>)currentUIBlocker {
  return _uiBlockerTarget;
}

- (void)addUIBlockerManagerObserver:(id<UIBlockerManagerObserver>)observer {
  [_uiBlockerManagerObservers addObserver:observer];
}

- (void)removeUIBlockerManagerObserver:(id<UIBlockerManagerObserver>)observer {
  [_uiBlockerManagerObservers removeObserver:observer];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  crash_keys::SetForegroundScenesCount([self foregroundScenes].count);
}

#pragma mark - Scenes lifecycle

- (void)sceneWillConnect:(NSNotification*)notification {
  UIWindowScene* scene =
      base::apple::ObjCCastStrict<UIWindowScene>(notification.object);
  SceneDelegate* sceneDelegate =
      base::apple::ObjCCastStrict<SceneDelegate>(scene.delegate);

  // Under some iOS 15 betas, Chrome gets scene connection events for some
  // system scene connections. To handle this, early return if the connecting
  // scene doesn't have a valid delegate. (See crbug.com/1217461)
  if (!sceneDelegate) {
    return;
  }

  SceneState* sceneState = sceneDelegate.sceneState;
  DCHECK(sceneState);

  [_observers appState:self sceneConnected:sceneState];
  crash_keys::SetConnectedScenesCount([self connectedScenes].count);
}

#pragma mark - Voice Over lifecycle

- (void)voiceOverStatusDidChange:(NSNotification*)notification {
  crash_keys::SetVoiceOverRunning(UIAccessibilityIsVoiceOverRunning());
}

@end
