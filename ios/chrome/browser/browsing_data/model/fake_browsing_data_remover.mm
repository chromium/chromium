// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/fake_browsing_data_remover.h"

bool FakeBrowsingDataRemover::IsRemoving() const {
  return false;
}

void FakeBrowsingDataRemover::Remove(browsing_data::TimePeriod time_period,
                                     BrowsingDataRemoveMask remove_mask,
                                     base::OnceClosure callback,
                                     RemovalParams params) {
  last_remove_mask_ = remove_mask;
  if (success) {
    NotifyBrowsingDataRemoved(last_remove_mask_);
  } else {
    NotifyBrowsingDataRemoved(BrowsingDataRemoveMask::REMOVE_NOTHING);
  }
}

void FakeBrowsingDataRemover::RemoveInRange(base::Time start_time,
                                            base::Time end_time,
                                            BrowsingDataRemoveMask remove_mask,
                                            base::OnceClosure callback,
                                            RemovalParams params) {}

void FakeBrowsingDataRemover::SetCachedTabsInfo(
    tabs_closure_util::WebStateIDToTime cached_tabs_info) {}

BrowsingDataRemoveMask FakeBrowsingDataRemover::GetLastUsedRemovalMask() {
  return last_remove_mask_;
}

void FakeBrowsingDataRemover::SetFailedForTesting() {
  success = false;
}
