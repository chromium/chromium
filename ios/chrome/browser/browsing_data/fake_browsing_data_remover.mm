// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/fake_browsing_data_remover.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool FakeBrowsingDataRemover::IsRemoving() const {
  return false;
}

void FakeBrowsingDataRemover::Remove(browsing_data::TimePeriod time_period,
                                     BrowsingDataRemoveMask remove_mask,
                                     base::OnceClosure callback) {}
