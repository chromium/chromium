// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_OBSERVER_BRIDGE_H_

#include "ios/chrome/browser/browsing_data/browsing_data_remover_observer.h"

#import <Foundation/Foundation.h>

// Objective-C interface for BrowsingDataRemoverObserver.
@protocol BrowsingDataRemoverObserving <NSObject>
@optional

// Invoked by BrowsingDataRemoverObserverBridge::OnBrowsingDataRemoved.
- (void)browsingDataRemover:(BrowsingDataRemover*)remover
    didRemoveBrowsingDataWithMask:(BrowsingDataRemoveMask)mask;

@end

// Adapter to use an id<BrowsingDataRemoverObserving> as a
// BrowsingDataRemoverObserver.
class BrowsingDataRemoverObserverBridge : public BrowsingDataRemoverObserver {
 public:
  explicit BrowsingDataRemoverObserverBridge(
      id<BrowsingDataRemoverObserving> observer);
  ~BrowsingDataRemoverObserverBridge() override;

  // BrowsingDataRemoverObserver methods.
  void OnBrowsingDataRemoved(BrowsingDataRemover* remover,
                             BrowsingDataRemoveMask mask) override;

 private:
  __weak id<BrowsingDataRemoverObserving> observer_ = nil;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverObserverBridge);
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_OBSERVER_BRIDGE_H_
