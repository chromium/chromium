// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_

#import <Foundation/Foundation.h>

#include "base/callback.h"
#include "base/observer_list.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"

class BrowsingDataRemoverObserver;

// BrowsingDataRemover is responsible for removing data related to
// browsing: history, downloads, cookies, ...
class BrowsingDataRemover : public KeyedService {
 public:
  BrowsingDataRemover();

  BrowsingDataRemover(const BrowsingDataRemover&) = delete;
  BrowsingDataRemover& operator=(const BrowsingDataRemover&) = delete;

  ~BrowsingDataRemover() override;

  // Is the service currently in the process of removing data?
  virtual bool IsRemoving() const = 0;

  // Removes browsing data for the given `time_period` with data types specified
  // by `remove_mask`. The `callback` is invoked asynchronously when the data
  // has been removed.
  virtual void Remove(browsing_data::TimePeriod time_period,
                      BrowsingDataRemoveMask remove_mask,
                      base::OnceClosure callback) = 0;

  // Removes all persisted data for sessions with `session_ids`.
  virtual void RemoveSessionsData(NSArray<NSString*>* session_ids) = 0;

  // Adds/removes `observer` from the list of observers notified when data is
  // removed by BrowsingDataRemover.
  void AddObserver(BrowsingDataRemoverObserver* observer);
  void RemoveObserver(BrowsingDataRemoverObserver* observer);

 protected:
  // Invokes `OnBrowsingDataRemoved` on all registered observers.
  void NotifyBrowsingDataRemoved(BrowsingDataRemoveMask mask);

 private:
  base::ObserverList<BrowsingDataRemoverObserver, true>::Unchecked observers_;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
