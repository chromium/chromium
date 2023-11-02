// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installer_bridge.h"

WebStateDependencyInstallerBridge::WebStateDependencyInstallerBridge(
    id<DependencyInstalling> installing,
    WebStateList* web_state_list)
    : installing_(installing), observer_(web_state_list, this) {}

void WebStateDependencyInstallerBridge::InstallDependency(
    web::WebState* web_state) {
  if ([installing_
          respondsToSelector:@selector(installDependencyForWebState:)]) {
    [installing_ installDependencyForWebState:web_state];
  }
}
void WebStateDependencyInstallerBridge::UninstallDependency(
    web::WebState* web_state) {
  if ([installing_
          respondsToSelector:@selector(uninstallDependencyForWebState:)]) {
    [installing_ uninstallDependencyForWebState:web_state];
  }
}
