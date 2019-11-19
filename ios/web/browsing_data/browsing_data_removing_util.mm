// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/browsing_data/browsing_data_removing_util.h"

#import "ios/web/browsing_data/browsing_data_remover.h"
#import "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

void ClearBrowsingData(BrowserState* browser_state,
                       ClearBrowsingDataMask types,
                       base::Time modified_since,
                       base::OnceClosure closure) {
  BrowsingDataRemover::FromBrowserState(browser_state)
      ->ClearBrowsingData(types, modified_since, std::move(closure));
}

}  // namespace web
