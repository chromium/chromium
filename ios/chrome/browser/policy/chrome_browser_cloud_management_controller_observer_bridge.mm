// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/chrome_browser_cloud_management_controller_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromeBrowserCloudManagementControllerObserverBridge::
    ChromeBrowserCloudManagementControllerObserverBridge(
        id<ChromeBrowserCloudManagementControllerObserver> observer_delegate,
        policy::ChromeBrowserCloudManagementController*
            chrome_cloud_management_observer)
    : observer_(observer_delegate) {
  DCHECK(observer_);

  scoped_observation_.Observe(chrome_cloud_management_observer);
}

ChromeBrowserCloudManagementControllerObserverBridge::
    ~ChromeBrowserCloudManagementControllerObserverBridge() {}

void ChromeBrowserCloudManagementControllerObserverBridge::
    OnPolicyRegisterFinished(bool succeeded) {
  [observer_ policyRegistrationDidCompleteSuccessfuly:succeeded];
}
