// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_H_

#import <memory>

#import "base/observer_list_types.h"

class Browser;
class TabsDependencyInstallationHelper;

namespace web {
class WebState;
}  // namespace web

// Interface for classes wishing to install and/or uninstall dependencies
// (delegates, etc) for each WebState when they are inserted/removed from
// a Browser's WebStateList.
class TabsDependencyInstaller : public base::CheckedObserver {
 public:
  // Policy controlling when the TabsDependencyInstaller should be
  // notified that a WebState is ready to be configured.
  enum class Policy {
    // The TabsDependencyInstaller will only be notified if the WebState is
    // realized. If it become realized later (e.g. when becoming active) the
    // TabsDependencyInstaller will be notified at that point in time.
    kOnlyRealized,

    // The notification of the TabsDependencyInstaller will depends on the
    // feature kCreateTabHelperOnlyForRealizedWebStates. If enabled, this
    // will behave as kOnlyRealized, otherwise, the TabsDependencyInstaller
    // will be notified as soon as the WebState is inserted even if it is
    // still unrealized.
    kAccordingToFeature,
  };

  TabsDependencyInstaller();
  ~TabsDependencyInstaller() override;

  // Starts observing the Browser's WebStateList and installing the
  // dependencies.
  void StartObserving(Browser* browser, Policy policy);

  // Stops observing the Browser's WebStateList (and if there are still
  // WebStates with installed dependencies, uninstall them). Must be called
  // before the destructor of DependencyInstaller is called.
  void StopObserving();

  // Serves as a hook for any installation work needed to set up a per-WebState
  // dependency.
  virtual void OnWebStateInserted(web::WebState* web_state) = 0;

  // Serves as a hook for any cleanup work needed to remove a dependency when it
  // is no longer needed but the data must not be removed, e.g. will be moved
  // to another list, the window is closed, the application is terminating, ...
  virtual void OnWebStateRemoved(web::WebState* web_state) = 0;

  // Serves as a hook for purging any data associated with a WebState before
  // it is permanently removed (i.e. cannot be re-opened).
  virtual void OnWebStateDeleted(web::WebState* web_state) = 0;

  // Serves as a hook for performing any action when the active WebState
  // change. Either of `new_active` or `old_active` may be null (in case
  // of the WebStateList transitioning to/from the empty state).
  virtual void OnActiveWebStateChanged(web::WebState* old_active,
                                       web::WebState* new_active) = 0;

  // The following methods are for WebStates that are not part of a
  // WebStateList. They allow manual installation and uninstallation of
  // dependencies.

  // Installs dependencies. Corresponds to OnWebStateInserted.
  void InstallDependencies(web::WebState* web_state);

  // Uninstalls dependencies, but keeps data. Corresponds to OnWebStateRemoved.
  void UninstallDependencies(web::WebState* web_state);

  // Uninstalls dependencies and purges data. Corresponds to
  // OnWebStateDeleted.
  void PurgeDependencies(web::WebState* web_state);

 private:
  friend class TabsDependencyInstallerManager;
  // Helper used to observe the WebStateList and WebStates and forward the
  // events to the current instance.
  std::unique_ptr<TabsDependencyInstallationHelper> installation_helper_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_H_
