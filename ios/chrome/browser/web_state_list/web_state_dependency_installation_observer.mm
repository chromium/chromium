// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_dependency_installation_observer.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebStateDependencyInstallationObserver::WebStateDependencyInstallationObserver(
    WebStateList* web_state_list,
    DependencyInstaller* dependency_installer)
    : web_state_list_(web_state_list),
      dependency_installer_(dependency_installer) {
  DCHECK(web_state_list_);
  DCHECK(dependency_installer_);

  web_state_list_observation_.Observe(web_state_list_);
  for (int i = 0; i < web_state_list_->count(); i++) {
    OnWebStateAdded(web_state_list_->GetWebStateAt(i));
  }
}

WebStateDependencyInstallationObserver::
    ~WebStateDependencyInstallationObserver() {
  for (int i = 0; i < web_state_list_->count(); i++) {
    OnWebStateRemoved(web_state_list_->GetWebStateAt(i));
  }
}

void WebStateDependencyInstallationObserver::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  OnWebStateAdded(web_state);
}

void WebStateDependencyInstallationObserver::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  OnWebStateRemoved(old_web_state);
  OnWebStateAdded(new_web_state);
}

void WebStateDependencyInstallationObserver::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  OnWebStateRemoved(web_state);
}

void WebStateDependencyInstallationObserver::OnWebStateAdded(
    web::WebState* web_state) {
  if (web_state->IsRealized()) {
    dependency_installer_->InstallDependency(web_state);
  } else if (!web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.AddObservation(web_state);
  }
}

void WebStateDependencyInstallationObserver::OnWebStateRemoved(
    web::WebState* web_state) {
  if (web_state->IsRealized()) {
    dependency_installer_->UninstallDependency(web_state);
  } else if (web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.RemoveObservation(web_state);
  }
}

void WebStateDependencyInstallationObserver::WebStateRealized(
    web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
  OnWebStateAdded(web_state);
}

void WebStateDependencyInstallationObserver::WebStateDestroyed(
    web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
}
