// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_H_

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

class BrowsingDataRemoverObserver;

// BrowsingDataRemover is responsible for removing data related to
// browsing: history, downloads, cookies, ...
class BrowsingDataRemover : public KeyedService {
 public:
  // If kAutomatic, BrowsingDataRemover decides if the activity indicator is
  // necessary. Otherwise, forces if indicator is shown or not.
  enum class ActivityIndicatorPolicy {
    kAutomatic,
    kNoIndicator,
    kForceIndicator,
  };

  // If kAutomatic, BrowsingDataRemover decides if reloading is necessary.
  // Otherwise, forces tagging or not all WebStates for reload.
  enum class WebStatesReloadPolicy {
    kAutomatic,
    kNoReload,
    kForceReload,
  };

  // If kAutomatic, BrowsingDataRemover decides if the active tab should stay
  // open. Otherwise, forces the active tab to close or not when using
  // BrowsingDataRemoverMask::CLOSE_TABS.
  enum class KeepActiveTabPolicy {
    kAutomatic,
    kKeepActiveTab,
    kCloseActiveTab,
  };

  // Parameters for removing browsing data.
  struct RemovalParams {
    // Returns RemovalParams with default values.
    static RemovalParams Default() { return {}; }

    // Indicates if the activity indicator should be or not shown while the
    // deletion is ongoing, or if BrowsingDataRemover decides if the activity
    // indicator is necessary.
    ActivityIndicatorPolicy show_activity_indicator =
        ActivityIndicatorPolicy::kAutomatic;

    // Indicates if all WebStates should be or not tagged for reload after the
    // deletion has completed, or if BrowsingDataRemover decides if reloading is
    // necessary.
    WebStatesReloadPolicy reload_web_states = WebStatesReloadPolicy::kAutomatic;

    // Indicates if BrowsingDataRemoverMask::CLOSE_TABS is allowed to close
    // the active tab or not.
    KeepActiveTabPolicy keep_active_tab = KeepActiveTabPolicy::kAutomatic;
  };

  BrowsingDataRemover();

  BrowsingDataRemover(const BrowsingDataRemover&) = delete;
  BrowsingDataRemover& operator=(const BrowsingDataRemover&) = delete;

  ~BrowsingDataRemover() override;

  // Returns a weak pointer to BrowsingDataRemover.
  base::WeakPtr<BrowsingDataRemover> AsWeakPtr();

  // Is the service currently in the process of removing data?
  virtual bool IsRemoving() const = 0;

  // Removes browsing data for the given `time_period` with data types specified
  // by `remove_mask`. The `callback` is invoked asynchronously when the data
  // has been removed. `params` indicates if some behaviour should be
  // overloaded.
  virtual void Remove(browsing_data::TimePeriod time_period,
                      BrowsingDataRemoveMask remove_mask,
                      base::OnceClosure callback,
                      RemovalParams params = RemovalParams::Default()) = 0;

  // A version of `Remove` that removes browsing data between a given
  // `start_time` and `end_time` instead of a pre-specified `TimePeriod`.
  virtual void RemoveInRange(
      base::Time start_time,
      base::Time end_time,
      BrowsingDataRemoveMask mask,
      base::OnceClosure callback,
      RemovalParams params = RemovalParams::Default()) = 0;

  // Allows the remover to have cached information in order to close tabs as
  // part of the removal of browsing data.
  virtual void SetCachedTabsInfo(
      tabs_closure_util::WebStateIDToTime cached_tabs_info) = 0;

  // Adds/removes `observer` from the list of observers notified when data is
  // removed by BrowsingDataRemover.
  void AddObserver(BrowsingDataRemoverObserver* observer);
  void RemoveObserver(BrowsingDataRemoverObserver* observer);

 protected:
  // Invokes `OnBrowsingDataRemoved` on all registered observers.
  void NotifyBrowsingDataRemoved(BrowsingDataRemoveMask mask);

 private:
  base::ObserverList<BrowsingDataRemoverObserver, true> observers_;

  base::WeakPtrFactory<BrowsingDataRemover> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_REMOVER_H_
