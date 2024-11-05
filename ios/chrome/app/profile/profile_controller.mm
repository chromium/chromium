// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_controller.h"

#import <memory>

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/certificate_policy_profile_agent.h"
#import "ios/chrome/app/profile/docking_promo_profile_agent.h"
#import "ios/chrome/app/profile/first_run_profile_agent.h"
#import "ios/chrome/app/profile/identity_confirmation_profile_agent.h"
#import "ios/chrome/app/profile/post_restore_profile_agent.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/profile/search_engine_choice_profile_agent.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_profile_agent.h"
#import "ios/chrome/browser/profile_metrics/model/profile_activity_profile_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/device_orientation/scoped_force_portrait_orientation.h"

@interface ProfileController () <ProfileStateObserver>
@end

@implementation ProfileController {
  // Used to force the device orientation in portrait mode on iPhone.
  std::unique_ptr<ScopedForcePortraitOrientation> _scopedForceOrientation;
}

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _state = [[ProfileState alloc] initWithAppState:appState];
    _scopedForceOrientation = ForcePortraitOrientationOnIphone(appState);
    [_state addObserver:self];
  }
  return self;
}

- (void)shutdown {
  // Under the UIScene API, -sceneDidDisconnect: notification is not sent to
  // the UISceneDelegate on app termination. Mark all connected scene states
  // as disconnected in order to allow the services to properly unregister
  // their observers and tear down the remaining UI.
  for (SceneState* sceneState in _state.connectedScenes) {
    sceneState.activationLevel = SceneActivationLevelDisconnected;
  }
}

#pragma mark ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  switch (nextInitStage) {
    case ProfileInitStage::kStart:
      break;

    case ProfileInitStage::kLoadProfile:
      break;

    case ProfileInitStage::kProfileLoaded:
      [self attachProfileAgents];
      break;

    case ProfileInitStage::kPrepareUI:
      break;

    case ProfileInitStage::kUIReady:
      // SceneController uses this stage to create the normal UI if needed.
      // There is no specific agent (other than SceneController) handling
      // this stage.
      // TODO(crbug.com/353683675): once AppInitStage and ProfileInitStage
      // are fully decoupled, remove the check that current ProfileStage is
      // the main ProfileStage.
      if (profileState.appState.mainProfile == profileState) {
        [profileState queueTransitionToNextInitStage];
      }
      break;

    case ProfileInitStage::kFirstRun:
      break;

    case ProfileInitStage::kChoiceScreen:
      break;

    case ProfileInitStage::kNormalUI:
      // Stop forcing the portrait orientation once the normal UI is presented.
      _scopedForceOrientation.reset();
      break;

    case ProfileInitStage::kFinal:
      break;
  }
}

#pragma mark Private methods

- (void)attachProfileAgents {
  // TODO(crbug.com/355142171): Remove the DiscoverFeedProfileAgent?
  [_state addAgent:[[DiscoverFeedProfileAgent alloc] init]];

  [_state addAgent:[[CertificatePolicyProfileAgent alloc] init]];
  [_state addAgent:[[FirstRunProfileAgent alloc] init]];
  [_state addAgent:[[IdentityConfirmationProfileAgent alloc] init]];
  [_state addAgent:[[ProfileActivityProfileAgent alloc] init]];
  [_state addAgent:[[PostRestoreProfileAgent alloc] init]];
  [_state addAgent:[[SearchEngineChoiceProfileAgent alloc] init]];

  if (IsDockingPromoEnabled()) {
    switch (DockingPromoExperimentTypeEnabled()) {
      case DockingPromoDisplayTriggerArm::kDuringFRE:
        break;
      case DockingPromoDisplayTriggerArm::kAfterFRE:
      case DockingPromoDisplayTriggerArm::kAppLaunch:
        [_state addAgent:[[DockingPromoProfileAgent alloc] init]];
        break;
    }
  }
}

@end
