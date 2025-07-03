// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"

TabsDependencyInstallerBridge::TabsDependencyInstallerBridge(
    id<TabsDependencyInstalling> installing,
    WebStateList* web_state_list)
    : installing_(installing), installation_helper_(web_state_list, this) {}

void TabsDependencyInstallerBridge::InstallDependency(
    web::WebState* web_state) {
  if ([installing_
          respondsToSelector:@selector(installDependencyForWebState:)]) {
    [installing_ installDependencyForWebState:web_state];
  }
}
void TabsDependencyInstallerBridge::UninstallDependency(
    web::WebState* web_state) {
  if ([installing_
          respondsToSelector:@selector(uninstallDependencyForWebState:)]) {
    [installing_ uninstallDependencyForWebState:web_state];
  }
}
