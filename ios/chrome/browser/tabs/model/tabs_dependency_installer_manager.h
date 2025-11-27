// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_MANAGER_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_MANAGER_H_

#import <vector>

#import "base/observer_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

class TabsDependencyInstaller;

namespace web {
class WebState;
}

// A browser-scoped service that manages all the TabsDependencyInstaller
// instances.
class TabsDependencyInstallerManager
    : public BrowserUserData<TabsDependencyInstallerManager> {
 public:
  ~TabsDependencyInstallerManager() override;

  // Adds an installer to the list of installers.
  void AddInstaller(TabsDependencyInstaller* installer);

  // Removes an installer from the list of installers.
  void RemoveInstaller(TabsDependencyInstaller* installer);

  // The following methods are for WebStates that are not part of a
  // WebStateList. They allow manual installation and uninstallation of
  // dependencies.

  // Install dependencies for all observed tabs dependency installers.
  void InstallDependencies(web::WebState* web_state);

  // Uninstall dependencies for all observed tabs dependency installers.
  void UninstallDependencies(web::WebState* web_state);

  // Remove dependencies for all observed tabs dependency installers.
  void PurgeDependencies(web::WebState* web_state);

 private:
  friend class BrowserUserData<TabsDependencyInstallerManager>;

  explicit TabsDependencyInstallerManager(Browser* browser);

  base::ObserverList<TabsDependencyInstaller, true> installers_;
  std::vector<web::WebState*> installed_web_states_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_MANAGER_H_
