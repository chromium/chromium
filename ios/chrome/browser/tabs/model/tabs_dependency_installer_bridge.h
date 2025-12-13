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

// Serves as a hook for any installation work needed to set up a per-WebState
// dependency.
- (void)webStateInserted:(web::WebState*)webState;

// Serves as a hook for any cleanup work needed to remove a dependency when it
// is no longer needed but the data must not be removed, e.g. will be moved
// to another list, the window is closed, the application is terminating, ...
- (void)webStateRemoved:(web::WebState*)webState;

// Serves as a hook for purging any data associated with a WebState before
// it is permanently removed (i.e. cannot be re-opened).
- (void)webStateDeleted:(web::WebState*)webState;

// Serves as a hook for performing any action when the active WebState
// change. Either of `newActive` or `oldActive` may be null (in case
// of the WebStateList transitioning to/from the empty state).
- (void)newWebStateActivated:(web::WebState*)newActive
           oldActiveWebState:(web::WebState*)oldActive;

@end

// Bridge allowing Objective-C classes to install dependencies by conforming to
// TabsDependencyInstalling protocol.
class TabsDependencyInstallerBridge final : public TabsDependencyInstaller {
 public:
  TabsDependencyInstallerBridge();
  ~TabsDependencyInstallerBridge() final;

  // Starts observing the Browser's WebStateList and installing the
  // dependencies.
  void StartObserving(id<TabsDependencyInstalling> installing,
                      Browser* browser,
                      Policy policy);

  // Stops observing the Browser's WebStateList (and if there are still
  // WebStates with installed dependencies, uninstall them). Must be called
  // before the destructor of DependencyInstaller is called.
  void StopObserving();

  // TabsDependencyInstaller:
  void OnWebStateInserted(web::WebState* web_state) final;
  void OnWebStateRemoved(web::WebState* web_state) final;
  void OnWebStateDeleted(web::WebState* web_state) final;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) final;

 private:
  // The Objective-C class which installs/uninstalls dependencies in response to
  // forwarded messages.
  __weak id<TabsDependencyInstalling> installing_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_BRIDGE_H_
