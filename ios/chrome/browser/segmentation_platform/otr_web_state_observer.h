// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_OTR_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_OTR_WEB_STATE_OBSERVER_H_

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache_observer.h"

class AllWebStateListObservationRegistrar;

namespace ios {
class ChromeBrowserStateManager;
}

namespace segmentation_platform {

// Keeps track of all the OTR WebState(s) across all browsers in an application.
class OTRWebStateObserver : public BrowserStateInfoCacheObserver {
 public:
  // Observer interface to listen to changes in number of OTR WebState.
  class ObserverClient : public base::CheckedObserver {
   public:
    ObserverClient() = default;

    // Called every time the number of OTR WebState changes in any of the
    // browsers in the application.
    virtual void OnOTRWebStateCountChanged(bool otr_state_exists) = 0;
  };

  explicit OTRWebStateObserver(
      ios::ChromeBrowserStateManager* browser_state_manager);
  ~OTRWebStateObserver() override;

  OTRWebStateObserver(OTRWebStateObserver&) = delete;
  OTRWebStateObserver& operator=(OTRWebStateObserver&) = delete;

  // BrowserStateInfoCacheObserver:
  void OnBrowserStateAdded(const base::FilePath& path) override;
  void OnBrowserStateWasRemoved(const base::FilePath& path) override;

  // Add/Remove observers.
  void AddObserver(ObserverClient* client);
  void RemoveObserver(ObserverClient* client);

  void TearDown();

 private:
  class WebStateObserver;

  // Stores data about a ChromeBrowserState.
  struct BrowserStateData {
    BrowserStateData();
    ~BrowserStateData();

    // Observer for all WebState(s) in the state.
    std::unique_ptr<AllWebStateListObservationRegistrar>
        all_web_state_observation;
    // Count of number of OTR WebState(s) in the BrowserState.
    int otr_web_state_count = 0;
  };

  void OnWebStateListChanged(const base::FilePath& browser_state_path,
                             int otr_web_state_count);

  // Counts OTR WebState(s) across all the BrowserState(s) and returns true if
  // any OTR WebState exists.
  bool HasAnyOtrWebState() const;

  base::ObserverList<ObserverClient, true> observer_clients_;
  raw_ptr<ios::ChromeBrowserStateManager> browser_state_manager_;
  base::flat_map<base::FilePath, std::unique_ptr<BrowserStateData>>
      browser_state_data_;

  bool shutting_down_ = false;
};

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_OTR_WEB_STATE_OBSERVER_H_
