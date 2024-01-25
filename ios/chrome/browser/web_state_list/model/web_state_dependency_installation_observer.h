// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_DEPENDENCY_INSTALLATION_OBSERVER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_DEPENDENCY_INSTALLATION_OBSERVER_H_

#import "base/memory/raw_ptr.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

// Interface for classes wishing to install and/or uninstall dependencies
// (delegates, etc) for each WebState using
// WebStateDependencyInstallationObserver (below).
class DependencyInstaller {
 public:
  // Serves as a hook for any installation work needed to set up a per-WebState
  // dependency.
  virtual void InstallDependency(web::WebState* web_state) {}
  // Serves as a hook for any cleanup work needed to remove a dependency when it
  // is no longer needed.
  virtual void UninstallDependency(web::WebState* web_state) {}
  virtual ~DependencyInstaller() {}
};

// Classes wishing to install/uninstall dependencies (such as delegates) for
// each WebState can create an instance and pass a DependencyInstaller
// configured to do the installing/uninstalling work. This class acts as a
// forwarder, listening for changes in the WebStateList and invoking the
// installation/uninstallation methods as necessary.
class WebStateDependencyInstallationObserver : public WebStateListObserver,
                                               public web::WebStateObserver {
 public:
  WebStateDependencyInstallationObserver(
      WebStateList* web_state_list,
      DependencyInstaller* dependency_installer);
  ~WebStateDependencyInstallationObserver() override;

  // WebStateListObserver:
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;
  void WebStateListDestroyed(WebStateList* web_state_list) override;

  WebStateDependencyInstallationObserver(
      const WebStateDependencyInstallationObserver&) = delete;
  WebStateDependencyInstallationObserver& operator=(
      const WebStateDependencyInstallationObserver&) = delete;

 private:
  // Helper methods that call InstallDependency/UninstallDependency on the
  // `dependency_installer_` if the WebState is realized, or start observing
  // the WebState for `WebStateRealized()` event.
  void OnWebStateAdded(web::WebState* web_state);
  void OnWebStateRemoved(web::WebState* web_state);

  // web::WebStateObserver:
  void WebStateRealized(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // The WebStateList being observed for addition, replacement, and detachment
  // of WebStates
  raw_ptr<WebStateList> web_state_list_;
  // The class which installs/uninstalls dependencies in response to changes to
  // the WebStateList
  raw_ptr<DependencyInstaller> dependency_installer_;
  // Automatically detaches `this` from the WebStateList when destroyed
  base::ScopedObservation<WebStateList, WebStateListObserver>
      web_state_list_observation_{this};
  // Automatically detaches `this` from the WebStates when destroyed.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_STATE_DEPENDENCY_INSTALLATION_OBSERVER_H_
