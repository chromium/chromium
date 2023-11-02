// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_DEPENDENCY_INSTALLER_BRIDGE_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_DEPENDENCY_INSTALLER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installation_observer.h"

// Protocol for classes wishing to install and/or uninstall dependencies
// (delegates, etc) for each WebState using WebStateDependencyInstallerBridge.
// This is the Objective-C analogue to the C++ DependencyInstaller class.
@protocol DependencyInstalling <NSObject>
@optional

- (void)installDependencyForWebState:(web::WebState*)webState;
- (void)uninstallDependencyForWebState:(web::WebState*)webState;

@end

// Bridge allowing Objective-C classes to install dependencies by conforming to
// DependencyInstalling protocol.
class WebStateDependencyInstallerBridge : public DependencyInstaller {
 public:
  WebStateDependencyInstallerBridge(id<DependencyInstalling> installing,
                                    WebStateList* web_state_list);
  ~WebStateDependencyInstallerBridge() override {}

  // DependencyInstaller:
  void InstallDependency(web::WebState* web_state) override;
  void UninstallDependency(web::WebState* web_state) override;

  WebStateDependencyInstallerBridge(const WebStateDependencyInstallerBridge&) =
      delete;
  WebStateDependencyInstallerBridge& operator=(
      const WebStateDependencyInstallerBridge&) = delete;

 private:
  // The Objective-C class which installs/uninstalls dependencies in response to
  // forwarded messages.
  id<DependencyInstalling> installing_;

  // The observer which informs this bridge that a dependency needs to be
  // installed/uninstalled; those calls are then forwarded to `installing_`.
  WebStateDependencyInstallationObserver observer_;
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_DEPENDENCY_INSTALLER_BRIDGE_H_
