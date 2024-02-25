// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_MODEL_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_POLICY_MODEL_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"

#include "base/scoped_observation.h"

// Objective-C protocol mirroring
// policy::ChromeBrowserCloudManagementController::Observer.
@protocol ChromeBrowserCloudManagementControllerObserver <NSObject>
- (void)policyRegistrationDidCompleteSuccessfuly:(BOOL)succeeded;
@end

// Simple observer bridge that forwards all events to its delegate observer.
class ChromeBrowserCloudManagementControllerObserverBridge
    : public policy::ChromeBrowserCloudManagementController::Observer {
 public:
  ChromeBrowserCloudManagementControllerObserverBridge(
      id<ChromeBrowserCloudManagementControllerObserver> observer_delegate,
      policy::ChromeBrowserCloudManagementController*
          chrome_cloud_management_observer);
  ChromeBrowserCloudManagementControllerObserverBridge(
      const ChromeBrowserCloudManagementControllerObserverBridge&) = delete;
  ChromeBrowserCloudManagementControllerObserverBridge& operator=(
      const ChromeBrowserCloudManagementControllerObserverBridge&) = delete;
  ~ChromeBrowserCloudManagementControllerObserverBridge() override;

  // policy::ChromeBrowserCloudManagementController::Observer implementation.
  void OnPolicyRegisterFinished(bool succeeded) override;

 private:
  __weak id<ChromeBrowserCloudManagementControllerObserver> observer_;
  base::ScopedObservation<
      policy::ChromeBrowserCloudManagementController,
      policy::ChromeBrowserCloudManagementController::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_POLICY_MODEL_CHROME_BROWSER_CLOUD_MANAGEMENT_CONTROLLER_OBSERVER_BRIDGE_H_
