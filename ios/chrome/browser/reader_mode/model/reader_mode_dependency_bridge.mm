// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_dependency_bridge.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_manager.h"

ReaderModeDependencyBridge::ReaderModeDependencyBridge(Browser* browser)
    : browser_(browser) {}

ReaderModeDependencyBridge::~ReaderModeDependencyBridge() = default;

void ReaderModeDependencyBridge::ReaderModeWebStateDidLoadContent(
    web::WebState* web_state) {
  TabsDependencyInstallerManager* manager =
      TabsDependencyInstallerManager::FromBrowser(browser_);
  CHECK(manager);
  manager->InstallDependencies(web_state);
}

void ReaderModeDependencyBridge::ReaderModeWebStateWillBecomeUnavailable(
    web::WebState* web_state) {
  TabsDependencyInstallerManager* manager =
      TabsDependencyInstallerManager::FromBrowser(browser_);
  CHECK(manager);
  manager->UninstallDependencies(web_state);
}

void ReaderModeDependencyBridge::ReaderModeTabHelperDestroyed(
    web::WebState* web_state) {
  TabsDependencyInstallerManager* manager =
      TabsDependencyInstallerManager::FromBrowser(browser_);
  CHECK(manager);
  manager->PurgeDependencies(web_state);
}
