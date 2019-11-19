// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_FAKE_BROWSING_DATA_REMOVER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_FAKE_BROWSING_DATA_REMOVER_H_

#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"

// Minimal implementation of BrowsingDataRemover, to be used in tests.
class FakeBrowsingDataRemover : public BrowsingDataRemover {
 public:
  FakeBrowsingDataRemover() = default;
  ~FakeBrowsingDataRemover() override = default;

  bool IsRemoving() const override;
  void Remove(browsing_data::TimePeriod time_period,
              BrowsingDataRemoveMask remove_mask,
              base::OnceClosure callback) override;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_FAKE_BROWSING_DATA_REMOVER_H_
