// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

#import "base/check.h"
#import "base/check_deref.h"
#import "base/memory/raw_ref.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

// Helper observing the WebStateList and the unrealized WebStates events and
// forwaring them to the owning TabsDependencyInstaller instance.
class TabsDependencyInstallationHelper : public WebStateListObserver,
                                         public web::WebStateObserver {
 public:
  TabsDependencyInstallationHelper(
      WebStateList& web_state_list,
      TabsDependencyInstaller& dependency_installer);
  ~TabsDependencyInstallationHelper() override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WebStateListDestroyed(WebStateList* web_state_list) override;

 private:
  // Invoked when a WebState is inserted/removed. They handle the fact that
  // the WebState may be unrealized (by observing it) or realized (invokes
  // the correct method of TabDependencyInstaller).
  void OnWebStateAdded(web::WebState* web_state);
  void OnWebStateRemoved(web::WebState* web_state);

  // web::WebStateObserver:
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebStateList being observed for addition, replacement, and detachment
  // of WebStates
  const raw_ref<WebStateList> web_state_list_;
  // The class which installs/uninstalls dependencies in response to changes to
  // the WebStateList
  const raw_ref<TabsDependencyInstaller> dependency_installer_;
  // Observation of the WebStateList.
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
  // Observation of the unrealized WebStates in the list.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

TabsDependencyInstallationHelper::TabsDependencyInstallationHelper(
    WebStateList& web_state_list,
    TabsDependencyInstaller& dependency_installer)
    : web_state_list_(web_state_list),
      dependency_installer_(dependency_installer) {
  web_state_list_observation_.Observe(&(web_state_list_.get()));
  for (int i = 0; i < web_state_list_->count(); i++) {
    OnWebStateAdded(web_state_list_->GetWebStateAt(i));
  }
}

TabsDependencyInstallationHelper::~TabsDependencyInstallationHelper() {
  for (int i = 0; i < web_state_list_->count(); i++) {
    OnWebStateRemoved(web_state_list_->GetWebStateAt(i));
  }
}

#pragma mark - WebStateListObserver

void TabsDependencyInstallationHelper::WebStateListDidChange(
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

void TabsDependencyInstallationHelper::WebStateListDestroyed(
    WebStateList* web_state_list) {
  // Checking that all WebStates have been destroyed before destroying
  // the WebStateList, so we should not be observing anything.
  DCHECK(!web_state_observations_.IsObservingAnySource());
  web_state_list_observation_.Reset();
}

void TabsDependencyInstallationHelper::OnWebStateAdded(
    web::WebState* web_state) {
  if (web_state->IsRealized()) {
    dependency_installer_->OnWebStateInserted(web_state);
  } else if (!web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.AddObservation(web_state);
  }
}

void TabsDependencyInstallationHelper::OnWebStateRemoved(
    web::WebState* web_state) {
  if (web_state->IsRealized()) {
    dependency_installer_->OnWebStateRemoved(web_state);
  } else if (web_state_observations_.IsObservingSource(web_state)) {
    web_state_observations_.RemoveObservation(web_state);
  }
}

#pragma mark - WebStateObserver

void TabsDependencyInstallationHelper::WebStateRealized(
    web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
  OnWebStateAdded(web_state);
}

void TabsDependencyInstallationHelper::WebStateDestroyed(
    web::WebState* web_state) {
  web_state_observations_.RemoveObservation(web_state);
}

#pragma mark - TabsDependencyInstaller

// Note: as the vtable is initialized as part of the constructor (and cleaned
// as part of the destructor), it is not possible to start observing the list
// in the constructor (nor stop in the destructor).
//
// This is why the TabsDependencyInstaller API exposes StartObserving() and
// StopObserving() methods and requires the sub-class to call them. At the
// point where the sub-class can call those methods, the vtable is correctly
// initialized (i.e. points to the sub-classes version of the methods).

TabsDependencyInstaller::TabsDependencyInstaller() = default;

TabsDependencyInstaller::~TabsDependencyInstaller() {
  CHECK(!installation_helper_) << "StopObserving() must be called before "
                                  "destroying a TabsDependencyInstaller.";
}

void TabsDependencyInstaller::StartObserving(WebStateList* web_state_list) {
  installation_helper_ = std::make_unique<TabsDependencyInstallationHelper>(
      CHECK_DEREF(web_state_list), *this);
}

void TabsDependencyInstaller::StopObserving() {
  installation_helper_.reset();
}
