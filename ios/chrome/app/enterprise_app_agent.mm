// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/enterprise_app_agent.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/policy/core/common/cloud/cloud_policy_client.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/policy_namespace.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/enterprise_loading_screen_view_controller.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/chrome_browser_cloud_management_controller_ios.h"
#import "ios/chrome/browser/policy/model/chrome_browser_cloud_management_controller_observer_bridge.h"
#import "ios/chrome/browser/policy/model/cloud_policy_client_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace {

constexpr base::TimeDelta kTimeout = base::Seconds(30);

}  // namespace

@interface EnterpriseAppAgent () <
    ChromeBrowserCloudManagementControllerObserver,
    CloudPolicyClientObserver>

@end

@implementation EnterpriseAppAgent {
  // Bridge to observe the ChromeBrowserCloudManagementController.
  std::unique_ptr<ChromeBrowserCloudManagementControllerObserverBridge>
      _cloudManagementControllerObserver;

  // Bridge to observe the CloudPolicyClient.
  std::unique_ptr<CloudPolicyClientObserverBridge> _cloudPolicyClientObserver;

  // Timer used to automatically dismiss the screen after a timeout.
  base::OneShotTimer _timer;

  // YES if the enterprise launch screen has been dismissed.
  BOOL _launchScreenDismissed;
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  [super appState:appState didTransitionFromInitStage:previousInitStage];
  if (appState.initStage == AppInitStage::kEnterprise) {
    [self maybeShowEnterpriseLoadScreen];
  }

  if (previousInitStage == AppInitStage::kEnterprise) {
    [appState removeAgent:self];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [super sceneState:sceneState transitionedToActivationLevel:level];
  if (level > SceneActivationLevelBackground) {
    if (self.appState.initStage == AppInitStage::kEnterprise) {
      [self showUIInScene:sceneState];
    }
  }
}

#pragma mark - ChromeBrowserCloudManagementControllerObserverBridge

- (void)policyRegistrationDidCompleteSuccessfuly:(BOOL)succeeded {
  if (!succeeded) {
    [self dismissLaunchScreen];
  }
}

#pragma mark - CloudPolicyClientObserverBridge

- (void)cloudPolicyWasFetched:(policy::CloudPolicyClient*)client {
  [self dismissLaunchScreen];
}

- (void)cloudPolicyDidError:(policy::CloudPolicyClient*)client {
  [self dismissLaunchScreen];
}

- (void)cloudPolicyRegistrationChanged:(policy::CloudPolicyClient*)client {
  // Ignored as for initialization, only registration and fetch completion
  // results are needed.
}

#pragma mark - Private methods

- (void)showUIInScene:(SceneState*)sceneState {
  if ([sceneState.rootViewController
          isKindOfClass:[EnterpriseLoadScreenViewController class]]) {
    return;
  }

  [sceneState
      setRootViewController:[[EnterpriseLoadScreenViewController alloc] init]
          makeKeyAndVisible:YES];
}

- (BOOL)shouldShowEnterpriseLoadScreen {
  // Only show the screen if the FRE has not been presented yet.
  if (!self.appState.startupInformation.isFirstRun) {
    return NO;
  }

  BrowserPolicyConnectorIOS* policyConnector =
      GetApplicationContext()->GetBrowserPolicyConnector();

  // Only show the screen if the policies are enabled.
  if (!policyConnector ||
      !policyConnector->chrome_browser_cloud_management_controller()
           ->IsEnabled()) {
    return NO;
  }

  policy::MachineLevelUserCloudPolicyManager*
      machineLevelUserCloudPolicyManager =
          policyConnector->machine_level_user_cloud_policy_manager();

  // Only show the screen if the policies are not fetched yet.
  if (!machineLevelUserCloudPolicyManager ||
      machineLevelUserCloudPolicyManager->IsFirstPolicyLoadComplete(
          policy::POLICY_DOMAIN_CHROME)) {
    return NO;
  }

  return YES;
}

- (void)maybeShowEnterpriseLoadScreen {
  if (![self shouldShowEnterpriseLoadScreen]) {
    [self.appState queueTransitionToNextInitStage];
    return;
  }

  BrowserPolicyConnectorIOS* policyConnector =
      GetApplicationContext()->GetBrowserPolicyConnector();

  _cloudManagementControllerObserver =
      std::make_unique<ChromeBrowserCloudManagementControllerObserverBridge>(
          self, policyConnector->chrome_browser_cloud_management_controller());

  _cloudPolicyClientObserver =
      std::make_unique<CloudPolicyClientObserverBridge>(
          self, policyConnector->machine_level_user_cloud_policy_manager()
                    ->core()
                    ->client());

  for (SceneState* scene in self.appState.connectedScenes) {
    if (scene.activationLevel > SceneActivationLevelBackground) {
      [self showUIInScene:scene];
    }
  }

  // Ensure to never stay stuck on enterprise launch screen.
  __weak EnterpriseAppAgent* weakSelf = self;
  _timer.Start(FROM_HERE, kTimeout, base::BindOnce(^{
                 [weakSelf dismissLaunchScreen];
               }));
}

- (void)dismissLaunchScreen {
  if (!_launchScreenDismissed) {
    _launchScreenDismissed = YES;
    [self.appState queueTransitionToNextInitStage];
  }
}

@end
