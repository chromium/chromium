// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_OTR_WEB_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_OTR_WEB_STATE_OBSERVER_H_

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager_observer.h"

class ChromeBrowserState;

namespace segmentation_platform {

// Keeps track of all the OTR WebState(s) across all browsers in an application.
class OTRWebStateObserver final : public ChromeBrowserStateManagerObserver {
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
      ChromeBrowserStateManager* browser_state_manager);
  ~OTRWebStateObserver() final;

  // ChromeBrowserStateManagerObserver:
  void OnChromeBrowserStateManagerDestroyed(
      ChromeBrowserStateManager* manager) final;
  void OnChromeBrowserStateCreated(ChromeBrowserStateManager* manager,
                                   ChromeBrowserState* browser_state) override;
  void OnChromeBrowserStateLoaded(ChromeBrowserStateManager* manager,
                                  ChromeBrowserState* browser_state) override;

  // Add/Remove observers.
  void AddObserver(ObserverClient* client);
  void RemoveObserver(ObserverClient* client);

  void TearDown();

 private:
  class WebStateObserver;
  class BrowserStateData;

  // Invoked when the count of OTR WebState for BrowserState named
  // `browser_state_name` may have changed.
  void OnWebStateListChanged(std::string_view browser_state_name,
                             bool has_otr_web_states);

  // Counts OTR WebState(s) across all the BrowserState(s) and returns true if
  // any OTR WebState exists.
  bool HasAnyOtrWebState() const;

  base::ScopedObservation<ChromeBrowserStateManager,
                          ChromeBrowserStateManagerObserver>
      browser_state_manager_observation_{this};

  base::ObserverList<ObserverClient, true> observer_clients_;
  base::flat_map<std::string, std::unique_ptr<BrowserStateData>, std::less<>>
      browser_state_data_;

  bool shutting_down_ = false;
};

}  // namespace segmentation_platform

#endif  // IOS_CHROME_BROWSER_SEGMENTATION_PLATFORM_MODEL_OTR_WEB_STATE_OBSERVER_H_
