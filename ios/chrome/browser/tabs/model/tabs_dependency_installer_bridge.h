// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_BRIDGE_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

// Protocol for classes wishing to install and/or uninstall dependencies
// for each WebState using TabsDependencyInstallerBridge. This is
// the Objective-C analogue to the C++ TabsDependencyInstaller class.
@protocol TabsDependencyInstalling <NSObject>

@optional

// Serves as a hook for any installation work needed to set up a per-WebState
// dependency.
- (void)webStateInserted:(web::WebState*)webState;

// Serves as a hook for any cleanup work needed to remove a dependency when it
// is no longer needed but the data must not be removed, e.g. will be moved
// to another list, the window is closed, the application is terminating, ...
- (void)webStateRemoved:(web::WebState*)webState;

@end

// Bridge allowing Objective-C classes to install dependencies by conforming to
// TabsDependencyInstalling protocol.
class TabsDependencyInstallerBridge final : public TabsDependencyInstaller {
 public:
  TabsDependencyInstallerBridge();
  ~TabsDependencyInstallerBridge() final;

  // Starts observing the WebStateList and installing the dependencies.
  void StartObserving(id<TabsDependencyInstalling> installing,
                      WebStateList* web_state_list,
                      Policy policy);

  // Stops observing the WebStateList (and if there are still WebStates
  // with installed dependencies, uninstall them). Must be called before
  // the destructor of DependencyInstaller is called.
  void StopObserving();

  // TabsDependencyInstaller:
  void OnWebStateInserted(web::WebState* web_state) final;
  void OnWebStateRemoved(web::WebState* web_state) final;

 private:
  // The Objective-C class which installs/uninstalls dependencies in response to
  // forwarded messages.
  __weak id<TabsDependencyInstalling> installing_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_BRIDGE_H_
