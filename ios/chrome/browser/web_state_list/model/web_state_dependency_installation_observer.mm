// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installation_observer.h"

#import "base/check.h"

WebStateDependencyInstallationObserver::WebStateDependencyInstallationObserver(
    WebStateList* web_state_list,
    DependencyInstaller* dependency_installer)
    : web_state_list_(web_state_list),
      dependency_installer_(dependency_installer) {
  DCHECK(web_state_list_);
  DCHECK(dependency_installer_);

  web_state_list_observation_.Observe(web_state_list_.get());
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

#pragma mark - WebStateListObserver

void WebStateDependencyInstallationObserver::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      OnWebStateRemoved(detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      OnWebStateRemoved(replace_change.replaced_web_state());
      OnWebStateAdded(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      OnWebStateAdded(insert_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }
}

void WebStateDependencyInstallationObserver::WebStateListDestroyed(
    WebStateList* web_state_list) {
  // Checking that all WebStates have been destroyed before destroying
  // the WebStateList, so we should not be observing anything.
  DCHECK(!web_state_observations_.IsObservingAnySource());
  web_state_list_observation_.Reset();
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
