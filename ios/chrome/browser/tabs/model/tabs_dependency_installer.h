// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_H_
#define IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_H_

#import <memory>

class TabsDependencyInstallationHelper;
class WebStateList;

namespace web {
class WebState;
}  // namespace web

// Interface for classes wishing to install and/or uninstall dependencies
// (delegates, etc) for each WebState when they are inserted/removed from
// a WebstateList.
class TabsDependencyInstaller {
 public:
  TabsDependencyInstaller();
  virtual ~TabsDependencyInstaller();

  // Starts observing the WebStateList and installing the dependencies.
  void StartObserving(WebStateList* web_state_list);

  // Stops observing the WebStateList (and if there are still WebStates
  // with installed dependencies, uninstall them). Must be called before
  // the destructor of DependencyInstaller is called.
  void StopObserving();

  // Serves as a hook for any installation work needed to set up a per-WebState
  // dependency.
  virtual void OnWebStateInserted(web::WebState* web_state) = 0;

  // Serves as a hook for any cleanup work needed to remove a dependency when it
  // is no longer needed but the data must not be removed, e.g. will be moved
  // to another list, the window is closed, the application is terminating, ...
  virtual void OnWebStateRemoved(web::WebState* web_state) = 0;

 private:
  // Helper used to observe the WebStateList and WebStates and forward the
  // events to the current instance.
  std::unique_ptr<TabsDependencyInstallationHelper> installation_helper_;
};

#endif  // IOS_CHROME_BROWSER_TABS_MODEL_TABS_DEPENDENCY_INSTALLER_H_
