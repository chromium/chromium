// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/enterprise_app_agent.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/policy_namespace.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/enterprise_loading_screen_view_controller.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/chrome_browser_cloud_management_controller_ios.h"
#import "ios/chrome/browser/policy/model/chrome_browser_cloud_management_controller_observer_bridge.h"
#import "ios/chrome/browser/policy/model/cloud_policy_client_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace {

constexpr CGFloat kTimeout = 30;

}  // namespace

@interface EnterpriseAppAgent () <
    ChromeBrowserCloudManagementControllerObserver,
    CloudPolicyClientObserver,
    SceneStateObserver> {
  std::unique_ptr<ChromeBrowserCloudManagementControllerObserverBridge>
      _cloudManagementControllerObserver;
  std::unique_ptr<CloudPolicyClientObserverBridge> _cloudPolicyClientObserver;

  raw_ptr<BrowserPolicyConnectorIOS> _policyConnector;
}

// The app state for the app.
@property(nonatomic, weak, readonly) AppState* appState;

// Browser policy connector for iOS.
@property(nonatomic, assign) raw_ptr<BrowserPolicyConnectorIOS> policyConnector;

// YES if enterprise launch screen has been dismissed.
@property(nonatomic, assign) BOOL launchScreenDismissed;

@end

@implementation EnterpriseAppAgent

- (void)dealloc {
  for (SceneState* scene in _appState.connectedScenes) {
    [scene removeObserver:self];
  }
  [_appState removeObserver:self];
}

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];

  for (SceneState* scene in appState.connectedScenes) {
    [scene addObserver:self];
  }
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (appState.initStage == AppInitStage::kEnterprise) {
    if ([self shouldShowEnterpriseLoadScreen]) {
      _cloudManagementControllerObserver = std::make_unique<
          ChromeBrowserCloudManagementControllerObserverBridge>(
          self,
          self.policyConnector->chrome_browser_cloud_management_controller());

      policy::CloudPolicyClient* client =
          self.policyConnector->machine_level_user_cloud_policy_manager()
              ->core()
              ->client();
      _cloudPolicyClientObserver =
          std::make_unique<CloudPolicyClientObserverBridge>(self, client);

      self.launchScreenDismissed = NO;
      for (SceneState* scene in appState.connectedScenes) {
        if (scene.activationLevel > SceneActivationLevelBackground) {
          [self showUIInScene:scene];
        }
      }

      // Ensure to never stay stuck on enterprise launch screen.
      __weak EnterpriseAppAgent* weakSelf = self;
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kTimeout * NSEC_PER_SEC)),
          dispatch_get_main_queue(), ^{
            if (!weakSelf.launchScreenDismissed) {
              [weakSelf cloudPolicyDidError:nullptr];
            }
          });
    } else {
      [self.appState queueTransitionToNextInitStage];
    }
  }

  if (previousInitStage == AppInitStage::kEnterprise) {
    // Nothing left to do; clean up.
    _cloudManagementControllerObserver = nullptr;
    _cloudPolicyClientObserver = nullptr;

    // Let the following line at the end of the block.
    [self.appState removeAgent:self];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (self.appState.initStage == AppInitStage::kEnterprise &&
      level > SceneActivationLevelBackground) {
    [self showUIInScene:sceneState];
  }
}

#pragma mark - ChromeBrowserCloudManagementControllerObserverBridge

- (void)policyRegistrationDidCompleteSuccessfuly:(BOOL)succeeded {
  if (!succeeded && !self.launchScreenDismissed) {
    self.launchScreenDismissed = YES;
    [self.appState queueTransitionToNextInitStage];
  }
}

#pragma mark - CloudPolicyClientObserverBridge

- (void)cloudPolicyWasFetched:(policy::CloudPolicyClient*)client {
  if (!self.launchScreenDismissed) {
    self.launchScreenDismissed = YES;
    [self.appState queueTransitionToNextInitStage];
  }
}

- (void)cloudPolicyDidError:(policy::CloudPolicyClient*)client {
  if (!self.launchScreenDismissed) {
    self.launchScreenDismissed = YES;
    [self.appState queueTransitionToNextInitStage];
  }
}

- (void)cloudPolicyRegistrationChanged:(policy::CloudPolicyClient*)client {
  // Ignored as for initialization, only registration and fetch completion
  // results are needed.
}

#pragma mark - private

- (void)showUIInScene:(SceneState*)sceneState {
  if ([sceneState.window.rootViewController
          isKindOfClass:[EnterpriseLoadScreenViewController class]]) {
    return;
  }

  sceneState.window.rootViewController =
      [[EnterpriseLoadScreenViewController alloc] init];
  [sceneState.window makeKeyAndVisible];
}

- (BOOL)shouldShowEnterpriseLoadScreen {
  self.policyConnector = GetApplicationContext()->GetBrowserPolicyConnector();
  // `policyConnector` is nullptr if policy is not enabled.
  if (!self.policyConnector) {
    return NO;
  }

  // `machineLevelUserCloudPolicyManager` is nullptr if the DM token needed
  // for fetch is explicitly invalid or if enrollment tokens and DM token are
  // empty.
  policy::MachineLevelUserCloudPolicyManager*
      machineLevelUserCloudPolicyManager =
          self.policyConnector->machine_level_user_cloud_policy_manager();

  return self.appState.startupInformation.isFirstRun &&
         self.policyConnector->chrome_browser_cloud_management_controller()
             ->IsEnabled() &&
         machineLevelUserCloudPolicyManager &&
         !machineLevelUserCloudPolicyManager->IsFirstPolicyLoadComplete(
             policy::POLICY_DOMAIN_CHROME);
}

@end
