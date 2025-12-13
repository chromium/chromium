// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

#import "base/check.h"
#import "base/check_deref.h"
#import "base/memory/raw_ref.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_manager.h"
#import "ios/web/common/features.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

namespace {

// Returns whether TabsDependencyInstaller should be notified about
// unrealized WebState insertion/removal or not.
bool WaitForRealizationToInstallDependencies(
    TabsDependencyInstaller::Policy policy) {
  switch (policy) {
    case TabsDependencyInstaller::Policy::kOnlyRealized:
      return true;

    case TabsDependencyInstaller::Policy::kAccordingToFeature:
      return web::features::CreateTabHelperOnlyForRealizedWebStates();
  }
}

}  // namespace

// Helper observing the WebStateList and the unrealized WebStates events and
// forwaring them to the owning TabsDependencyInstaller instance.
class TabsDependencyInstallationHelper : public WebStateListObserver,
                                         public web::WebStateObserver {
 public:
  TabsDependencyInstallationHelper(
      Browser& browser,
      TabsDependencyInstaller& dependency_installer,
      TabsDependencyInstaller::Policy policy);
  ~TabsDependencyInstallationHelper() override;

  // WebStateListObserver:
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WebStateListDestroyed(WebStateList* web_state_list) override;

  // web::WebStateObserver:
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  // Invoked when a WebState is inserted/removed. They handle the fact that
  // the WebState may be unrealized (by observing it) or realized (invokes
  // the correct method of TabDependencyInstaller).
  void OnWebStateAdded(web::WebState* web_state);
  void OnWebStateRemoved(web::WebState* web_state);

  // Original browser that is being observed.
  const raw_ref<Browser> browser_;
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
  // Whether the TabsDependencyInstaller should be notified of insertions
  // and removals of unrealized WebStates.
  const bool wait_for_realization_to_install_dependencies_;
};

TabsDependencyInstallationHelper::TabsDependencyInstallationHelper(
    Browser& browser,
    TabsDependencyInstaller& dependency_installer,
    TabsDependencyInstaller::Policy policy)
    : browser_(browser),
      web_state_list_(CHECK_DEREF(browser.GetWebStateList())),
      dependency_installer_(dependency_installer),
      wait_for_realization_to_install_dependencies_(
          WaitForRealizationToInstallDependencies(policy)) {
  web_state_list_observation_.Observe(&(web_state_list_.get()));
  for (int i = 0; i < web_state_list_->count(); i++) {
    OnWebStateAdded(web_state_list_->GetWebStateAt(i));
  }
  // Start tracking TabsDependencyInstaller.
  TabsDependencyInstallerManager* manager =
      TabsDependencyInstallerManager::FromBrowser(&*browser_);
  if (manager) {
    manager->AddInstaller(&*dependency_installer_);
  }
}

TabsDependencyInstallationHelper::~TabsDependencyInstallationHelper() {
  for (int i = 0; i < web_state_list_->count(); i++) {
    OnWebStateRemoved(web_state_list_->GetWebStateAt(i));
  }
  // Stop tracking TabsDependencyInstaller.
  TabsDependencyInstallerManager* manager =
      TabsDependencyInstallerManager::FromBrowser(&*browser_);
  if (manager) {
    manager->RemoveInstaller(&*dependency_installer_);
  }
}

#pragma mark - WebStateListObserver

void TabsDependencyInstallationHelper::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  if (!detach_change.is_closing()) {
    return;
  }

  if (!detach_change.is_user_action() && !detach_change.is_tabs_cleanup()) {
    return;
  }

  dependency_installer_->OnWebStateDeleted(detach_change.detached_web_state());
}

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

  if (status.active_web_state_change()) {
    dependency_installer_->OnActiveWebStateChanged(status.old_active_web_state,
                                                   status.new_active_web_state);
  }
}

void TabsDependencyInstallationHelper::WebStateListDestroyed(
    WebStateList* web_state_list) {
  // Checking that all WebStates have been destroyed before destroying
  // the WebStateList, so we should not be observing anything.
  CHECK(!web_state_observations_.IsObservingAnySource());
  web_state_list_observation_.Reset();
}

#pragma mark - WebStateObserver

void TabsDependencyInstallationHelper::WebStateRealized(
    web::WebState* web_state) {
  CHECK(wait_for_realization_to_install_dependencies_);
  web_state_observations_.RemoveObservation(web_state);
  OnWebStateAdded(web_state);
}

void TabsDependencyInstallationHelper::WebStateDestroyed(
    web::WebState* web_state) {
  CHECK(wait_for_realization_to_install_dependencies_);
  web_state_observations_.RemoveObservation(web_state);
}

#pragma mark - Private methods

void TabsDependencyInstallationHelper::OnWebStateAdded(
    web::WebState* web_state) {
  if (wait_for_realization_to_install_dependencies_) {
    if (!web_state->IsRealized()) {
      web_state_observations_.AddObservation(web_state);
      return;
    }
  }

  dependency_installer_->OnWebStateInserted(web_state);
}

void TabsDependencyInstallationHelper::OnWebStateRemoved(
    web::WebState* web_state) {
  if (wait_for_realization_to_install_dependencies_) {
    if (!web_state->IsRealized()) {
      web_state_observations_.RemoveObservation(web_state);
      return;
    }
  }

  dependency_installer_->OnWebStateRemoved(web_state);
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

void TabsDependencyInstaller::StartObserving(Browser* browser, Policy policy) {
  installation_helper_ = std::make_unique<TabsDependencyInstallationHelper>(
      CHECK_DEREF(browser), *this, policy);
}

void TabsDependencyInstaller::StopObserving() {
  installation_helper_.reset();
}
