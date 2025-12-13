// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_manager.h"

#import "base/check.h"
#import "base/containers/contains.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

TabsDependencyInstallerManager::TabsDependencyInstallerManager(Browser* browser)
    : BrowserUserData(browser) {}

TabsDependencyInstallerManager::~TabsDependencyInstallerManager() {
  installers_.Clear();
}

void TabsDependencyInstallerManager::AddInstaller(
    TabsDependencyInstaller* installer) {
  CHECK(installer);
  installers_.AddObserver(installer);
  for (web::WebState* web_state : installed_web_states_) {
    installer->OnWebStateInserted(web_state);
  }
}

void TabsDependencyInstallerManager::RemoveInstaller(
    TabsDependencyInstaller* installer) {
  CHECK(installer);
  installers_.RemoveObserver(installer);
}

void TabsDependencyInstallerManager::InstallDependencies(
    web::WebState* web_state) {
  if (base::Contains(installed_web_states_, web_state)) {
    return;
  }
  for (TabsDependencyInstaller& installer : installers_) {
    installer.OnWebStateInserted(web_state);
  }
  installed_web_states_.push_back(web_state);
}

void TabsDependencyInstallerManager::UninstallDependencies(
    web::WebState* web_state) {
  if (!base::Contains(installed_web_states_, web_state)) {
    return;
  }
  for (TabsDependencyInstaller& installer : installers_) {
    installer.OnWebStateRemoved(web_state);
  }
  std::erase(installed_web_states_, web_state);
}

void TabsDependencyInstallerManager::PurgeDependencies(
    web::WebState* web_state) {
  if (!base::Contains(installed_web_states_, web_state)) {
    return;
  }
  for (TabsDependencyInstaller& installer : installers_) {
    installer.OnWebStateDeleted(web_state);
  }
  std::erase(installed_web_states_, web_state);
}
