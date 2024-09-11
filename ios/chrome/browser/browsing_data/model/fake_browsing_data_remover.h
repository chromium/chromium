// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_FAKE_BROWSING_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_FAKE_BROWSING_DATA_REMOVER_H_

#include "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"

// Minimal implementation of BrowsingDataRemover, to be used in tests.
class FakeBrowsingDataRemover : public BrowsingDataRemover {
 public:
  FakeBrowsingDataRemover() = default;
  ~FakeBrowsingDataRemover() override = default;

  bool IsRemoving() const override;
  void Remove(browsing_data::TimePeriod time_period,
              BrowsingDataRemoveMask remove_mask,
              base::OnceClosure callback,
              RemovalParams params = RemovalParams::Default()) override;
  void RemoveInRange(base::Time start_time,
                     base::Time end_time,
                     BrowsingDataRemoveMask remove_mask,
                     base::OnceClosure callback,
                     RemovalParams params = RemovalParams::Default()) override;
  void SetCachedTabsInfo(
      tabs_closure_util::WebStateIDToTime cached_tabs_info) override;
  BrowsingDataRemoveMask GetLastUsedRemovalMask();
  void SetFailedForTesting();

 private:
  BrowsingDataRemoveMask last_remove_mask_;
  bool success{true};
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_FAKE_BROWSING_DATA_REMOVER_H_
