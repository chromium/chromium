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
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_in_progress.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_options.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_prefs.h"
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

@interface SceneState () <SignInInProgressAudience>

@end

@implementation SceneState {
  // Cache the connection informations.
  SceneStateOptions _sceneStateOptions;

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

- (instancetype)init {
  if ((self = [super init])) {
    _observers = [SceneStateObserverList
        observersWithProtocol:@protocol(SceneStateObserver)];
    _agents = [[NSMutableArray alloc] init];
    _uiBlockerState = [[SceneUIBlockerState alloc] init];
    _tabGridState = [[TabGridState alloc] init];
    _incognitoState = [[IncognitoState alloc] initWithSceneState:self];
    _layoutState = [[LayoutState alloc] init];
    _lensOverlayStateNotifier = [[LensOverlayStateNotifier alloc] init];
    _prefs = nil;
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

- (void)connectWithOptions:(SceneStateOptions)options {
  _sceneStateOptions = std::move(options);
  ProfileState* profileState = _sceneStateOptions.profile_state;
  [_observers sceneState:self profileStateConnected:profileState];
}

#pragma mark - Setters & Getters.

- (std::string_view)sceneSessionID {
  return _sceneStateOptions.identifier;
}

- (void)setSceneSessionID:(std::string_view)sceneSessionID {
  [self connectWithOptions:{.profile_state = _sceneStateOptions.profile_state,
                            .identifier = std::string(sceneSessionID)}];
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

- (ProfileState*)profileState {
  return _sceneStateOptions.profile_state;
}

- (void)setProfileState:(ProfileState*)profileState {
  [self connectWithOptions:{.profile_state = profileState,
                            .identifier = _sceneStateOptions.identifier}];
}

#pragma mark - UIBlockerTarget

- (BOOL)isUIBlocked {
  return self.uiBlockerState.presentingModalOverlay;
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
    _signinUIBlocker = ScopedUIBlocker::ProfileScoped(self);
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

@end
