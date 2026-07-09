// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

#import "base/apple/foundation_util.h"
#import "base/check_deref.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/ios/ios_util.h"
#import "base/logging.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_in_progress.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_prefs.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/lens_overlay_state_notifier.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_ui_blocker_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_window.h"

@interface SceneStateObserverList : CRBProtocolObservers <SceneStateObserver>
@end

@implementation SceneStateObserverList
@end

#pragma mark - SceneState

@interface SceneState () <SignInInProgressAudience, ProfileStateObserver>

@end

@implementation SceneState {
  // Cache the session identifier.
  std::string _sceneSessionID;

  // The AppState passed to the initializer.
  AppState* _appState;

  // Container for this object's observers.
  SceneStateObserverList* _observers;

  // Agents attached to this scene.
  NSMutableArray<id<SceneAgent>>* _agents;

  // The current value of -activationLevel.
  SceneActivationLevel _activationLevel;

  // A UIBlocker that blocks other scenes if and only if a sign in is in
  // progress.
  std::unique_ptr<ScopedUIBlocker> _signinUIBlocker;

  // The number of sign-in in progress. This include both the authentication
  // flow and the sign-in prompt UI.
  // In normal usage, this number can be greater than one because a signin
  // coordinator may open another signin coordinator. It also occurs that two
  // signin coordinator are started simultaneously from different screen, for
  // example due to simultaneous tap on a IPH signin promo and on the NTP’s
  // identity disc.
  NSInteger _numberOfSigninInProgress;
}

- (instancetype)initWithAppState:(AppState*)appState {
  self = [super init];
  if (self) {
    _appState = appState;
    _observers = [SceneStateObserverList
        observersWithProtocol:@protocol(SceneStateObserver)];
    _agents = [[NSMutableArray alloc] init];
    _uiBlockerState = [[SceneUIBlockerState alloc] init];
    _tabGridState = [[TabGridState alloc] init];
    _incognitoState = [[IncognitoState alloc] initWithSceneState:self];
    _layoutState = [[LayoutState alloc] init];
    _lensOverlayStateNotifier = [[LensOverlayStateNotifier alloc] init];
    _prefs = nil;

    // AppState might be nil in tests.
    if (appState) {
      [self addObserver:appState];
    }
  }
  return self;
}

#pragma mark - public

- (void)addObserver:(id<SceneStateObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<SceneStateObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)addAgent:(id<SceneAgent>)agent {
  DCHECK(agent);
  [_agents addObject:agent];
  [agent setSceneState:self];
}

- (NSArray*)connectedAgents {
  return _agents;
}

- (std::unique_ptr<SigninInProgress>)createSigninInProgress {
  return std::make_unique<SigninInProgress>(self);
}

#pragma mark - Setters & Getters.

- (UIWindow*)window {
  UIWindow* mainWindow = nil;
  for (UIWindow* window in self.scene.windows) {
    if ([window isKindOfClass:[ChromeOverlayWindow class]]) {
      mainWindow = window;
    }
  }
  return mainWindow;
}

- (const std::string&)sceneSessionID {
  return _sceneSessionID;
}

- (void)setScene:(UIWindowScene*)scene {
  _scene = scene;
  if (_scene) {
    _sceneSessionID = SessionIdentifierForScene(_scene);
    [self createPrefsIfPossible];
  } else {
    _sceneSessionID.clear();
    _prefs = nil;
  }
}

- (void)setActivationLevel:(SceneActivationLevel)newLevel {
  if (_activationLevel == newLevel) {
    return;
  }
  _activationLevel = newLevel;

  [_observers sceneState:self transitionedToActivationLevel:newLevel];
}

- (void)setUIEnabled:(BOOL)UIEnabled {
  if (_UIEnabled == UIEnabled) {
    return;
  }

  _UIEnabled = UIEnabled;
  if (UIEnabled) {
    [_observers sceneStateDidEnableUI:self];
  } else {
    [_observers sceneStateDidDisableUI:self];
  }
}

- (id<BrowserProviderInterface>)browserProviderInterface {
  return self.controller.browserProviderInterface;
}


- (void)setURLContextsToOpen:(NSSet<UIOpenURLContext*>*)URLContextsToOpen {
  if (_URLContextsToOpen == nil || URLContextsToOpen == nil) {
    _URLContextsToOpen = URLContextsToOpen;
  } else {
    _URLContextsToOpen =
        [_URLContextsToOpen setByAddingObjectsFromSet:URLContextsToOpen];
  }
  if (_URLContextsToOpen) {
    [_observers sceneState:self hasPendingURLs:_URLContextsToOpen];
  }
}

- (void)setPendingUserActivity:(NSUserActivity*)pendingUserActivity {
  _pendingUserActivity = pendingUserActivity;
  [_observers sceneState:self receivedUserActivity:pendingUserActivity];
}

- (BOOL)signinInProgress {
  return _numberOfSigninInProgress > 0;
}

- (void)setProfileState:(ProfileState*)profileState {
  if (_profileState) {
    [_profileState removeObserver:self];
  }
  _profileState = profileState;
  [_observers sceneState:self profileStateConnected:_profileState];
  if (_profileState) {
    [_profileState addObserver:self];
  }
}

#pragma mark - UIBlockerTarget

- (BOOL)isUIBlocked {
  return self.uiBlockerState.presentingModalOverlay;
}

- (id<UIBlockerManager>)uiBlockerManagerForExtent:(UIBlockerExtent)extent {
  switch (extent) {
    case UIBlockerExtent::kProfile:
      return _profileState;
    case UIBlockerExtent::kApplication:
      return _appState;
  }
}

- (void)bringBlockerToFront:(UIScene*)requestingScene {
  if (!base::ios::IsMultipleScenesSupported()) {
    return;
  }
  UISceneActivationRequestOptions* options =
      [[UISceneActivationRequestOptions alloc] init];
  options.requestingScene = requestingScene;

  [[UIApplication sharedApplication]
      requestSceneSessionActivation:self.scene.session
                       userActivity:nil
                            options:options
                       errorHandler:^(NSError* error) {
                         LOG(ERROR) << base::SysNSStringToUTF8(
                             error.localizedDescription);
                         NOTREACHED();
                       }];
}

#pragma mark - debug

- (NSString*)description {
  NSString* activityString = nil;
  switch (_activationLevel) {
    case SceneActivationLevelUnattached: {
      activityString = @"Unattached";
      break;
    }

    case SceneActivationLevelDisconnected: {
      activityString = @"Disconnected";
      break;
    }

    case SceneActivationLevelBackground: {
      activityString = @"Background";
      break;
    }
    case SceneActivationLevelForegroundInactive: {
      activityString = @"Foreground, Inactive";
      break;
    }
    case SceneActivationLevelForegroundActive: {
      activityString = @"Active";
      break;
    }
  }

  return
      [NSString stringWithFormat:@"SceneState %p (%@)", self, activityString];
}

#pragma mark - SignInInProgressAudience

- (void)signInStarted {
  if (_numberOfSigninInProgress == 0) {
    [_observers signinDidStart:self];
    CHECK(!_signinUIBlocker, base::NotFatalUntil::M146);
    _signinUIBlocker = std::make_unique<ScopedUIBlocker>(self);
  } else {
    CHECK(_signinUIBlocker, base::NotFatalUntil::M146);
  }
  _numberOfSigninInProgress++;
}

- (void)signinFinished {
  _numberOfSigninInProgress--;
  CHECK_GE(_numberOfSigninInProgress, 0, base::NotFatalUntil::M146);
  if (_numberOfSigninInProgress < 0) {
    _numberOfSigninInProgress = 0;
  }
  if (_numberOfSigninInProgress > 0) {
    return;
  }
  _signinUIBlocker.reset();
  [_observers signinDidEnd:self];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage >= ProfileInitStage::kProfileLoaded) {
    [self createPrefsIfPossible];
  }
}

#pragma mark - Private methods

// Will create the SceneStatePrefs if the object is ready. Can be called
// when any condition controlling the creation of the object has changed.
- (void)createPrefsIfPossible {
  if (!_scene || _sceneSessionID.empty() ||
      _profileState.initStage < ProfileInitStage::kProfileLoaded) {
    return;
  }

  // During unit tests, the profile or the profile manager may not be
  // initialized. Avoid crashing by returning early.
  ProfileIOS* profile = _profileState.profile;
  ProfileManagerIOS* manager = GetApplicationContext()->GetProfileManager();
  if (!profile || !manager) {
    return;
  }

  [_profileState removeObserver:self];
  const std::string& profile_name = profile->GetProfileName();
  _prefs = [[SceneStatePrefs alloc] initWithProfileManager:manager
                                               profileName:profile_name
                                         sessionIdentifier:_sceneSessionID
                                              sceneSession:_scene.session];
  [_incognitoState preferencesDidLoad];
}

@end
