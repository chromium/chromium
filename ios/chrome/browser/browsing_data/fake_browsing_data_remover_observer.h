// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_FAKE_BROWSING_DATA_REMOVER_OBSERVER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_FAKE_BROWSING_DATA_REMOVER_OBSERVER_H_

#import "ios/chrome/browser/browsing_data/browsing_data_remover_observer_bridge.h"

#import <Foundation/Foundation.h>

// Arguments passed to |browsingDataRemover:didRemoveBrowsingDataWithMask:|.
struct TestDidRemoveBrowsingDataWithMaskInfo {
  BrowsingDataRemover* remover = nullptr;
  BrowsingDataRemoveMask mask = BrowsingDataRemoveMask::REMOVE_NOTHING;
};

@interface FakeBrowsingDataRemoverObserver
    : NSObject <BrowsingDataRemoverObserving>

// Arguments passed to |browsingDataRemover:didRemoveBrowsingDataWithMask:|.
@property(nonatomic, readonly)
    TestDidRemoveBrowsingDataWithMaskInfo* didRemoveBrowsingDataWithMaskInfo;

@end

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_FAKE_BROWSING_DATA_REMOVER_OBSERVER_H_
