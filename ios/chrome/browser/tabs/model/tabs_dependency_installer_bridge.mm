// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"

TabsDependencyInstallerBridge::TabsDependencyInstallerBridge() = default;

TabsDependencyInstallerBridge::~TabsDependencyInstallerBridge() = default;

void TabsDependencyInstallerBridge::StartObserving(
    id<TabsDependencyInstalling> installing,
    Browser* browser,
    Policy policy) {
  installing_ = installing;
  TabsDependencyInstaller::StartObserving(browser, policy);
}

void TabsDependencyInstallerBridge::StopObserving() {
  TabsDependencyInstaller::StopObserving();
  installing_ = nil;
}

void TabsDependencyInstallerBridge::OnWebStateInserted(
    web::WebState* web_state) {
  [installing_ webStateInserted:web_state];
}

void TabsDependencyInstallerBridge::OnWebStateRemoved(
    web::WebState* web_state) {
  [installing_ webStateRemoved:web_state];
}

void TabsDependencyInstallerBridge::OnWebStateDeleted(
    web::WebState* web_state) {
  [installing_ webStateDeleted:web_state];
}

void TabsDependencyInstallerBridge::OnActiveWebStateChanged(
    web::WebState* old_active,
    web::WebState* new_active) {
  [installing_ newWebStateActivated:new_active oldActiveWebState:old_active];
}
