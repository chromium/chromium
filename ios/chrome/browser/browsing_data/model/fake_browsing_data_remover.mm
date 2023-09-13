// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/fake_browsing_data_remover.h"

bool FakeBrowsingDataRemover::IsRemoving() const {
  return false;
}

void FakeBrowsingDataRemover::Remove(browsing_data::TimePeriod time_period,
                                     BrowsingDataRemoveMask remove_mask,
                                     base::OnceClosure callback) {}

void FakeBrowsingDataRemover::RemoveSessionsData(
    NSArray<NSString*>* session_ids) {}
