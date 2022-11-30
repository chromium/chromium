// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_OBSERVER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_OBSERVER_H_

#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"

class BrowsingDataRemover;

// BrowsingDataRemoverObserver allows for observing browsing data removal
// by BrowsingDataRemover.
class BrowsingDataRemoverObserver {
 public:
  BrowsingDataRemoverObserver() = default;

  BrowsingDataRemoverObserver(const BrowsingDataRemoverObserver&) = delete;
  BrowsingDataRemoverObserver& operator=(const BrowsingDataRemoverObserver&) =
      delete;

  virtual ~BrowsingDataRemoverObserver() = default;

  // Invoked when data was successfully removed. The `mask` will represent
  // the type of removed data. See BrowsingDataRemoveMask for details.
  virtual void OnBrowsingDataRemoved(BrowsingDataRemover* remover,
                                     BrowsingDataRemoveMask mask) = 0;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_OBSERVER_H_
