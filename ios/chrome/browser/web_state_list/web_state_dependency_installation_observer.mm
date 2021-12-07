// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "ios/chrome/browser/web_state_list/web_state_dependency_installation_observer.h"

WebStateDependencyInstallationObserver::WebStateDependencyInstallationObserver(
    WebStateList* web_state_list,
    DependencyInstaller* dependency_installer)
    : web_state_list_(web_state_list),
      dependency_installer_(dependency_installer) {
  observation_.Observe(web_state_list_);
  for (int i = 0; i < web_state_list_->count(); i++) {
    dependency_installer_->InstallDependency(web_state_list_->GetWebStateAt(i));
  }
}

WebStateDependencyInstallationObserver::
    ~WebStateDependencyInstallationObserver() {}

void WebStateDependencyInstallationObserver::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  dependency_installer_->InstallDependency(web_state);
}

void WebStateDependencyInstallationObserver::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  dependency_installer_->UninstallDependency(old_web_state);
  dependency_installer_->InstallDependency(new_web_state);
}

void WebStateDependencyInstallationObserver::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  dependency_installer_->UninstallDependency(web_state);
}
