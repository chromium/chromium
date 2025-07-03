// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_BRIDGE_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

// Protocol for classes wishing to install and/or uninstall dependencies
// for each WebState using TabsDependencyInstallerBridge. This is
// the Objective-C analogue to the C++ TabsDependencyInstaller class.
@protocol TabsDependencyInstalling <NSObject>
@optional

- (void)installDependencyForWebState:(web::WebState*)webState;
- (void)uninstallDependencyForWebState:(web::WebState*)webState;

@end

// Bridge allowing Objective-C classes to install dependencies by conforming to
// TabsDependencyInstalling protocol.
class TabsDependencyInstallerBridge : public TabsDependencyInstaller {
 public:
  TabsDependencyInstallerBridge(id<TabsDependencyInstalling> installing,
                                WebStateList* web_state_list);
  ~TabsDependencyInstallerBridge() override {}

  // TabsDependencyInstaller:
  void InstallDependency(web::WebState* web_state) override;
  void UninstallDependency(web::WebState* web_state) override;

 private:
  // The Objective-C class which installs/uninstalls dependencies in response to
  // forwarded messages.
  id<TabsDependencyInstalling> installing_;

  // The helper which informs this bridge that a dependency needs to be
  // installed/uninstalled; those calls are then forwarded to `installing_`.
  TabsDependencyInstallationHelper installation_helper_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_BRIDGE_H_
