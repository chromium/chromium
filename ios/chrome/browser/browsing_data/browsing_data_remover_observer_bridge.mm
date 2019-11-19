// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/browsing_data_remover_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowsingDataRemoverObserverBridge::BrowsingDataRemoverObserverBridge(
    id<BrowsingDataRemoverObserving> observer)
    : observer_(observer) {}

BrowsingDataRemoverObserverBridge::~BrowsingDataRemoverObserverBridge() =
    default;

void BrowsingDataRemoverObserverBridge::OnBrowsingDataRemoved(
    BrowsingDataRemover* remover,
    BrowsingDataRemoveMask mask) {
  if ([observer_ respondsToSelector:@selector(browsingDataRemover:
                                        didRemoveBrowsingDataWithMask:)]) {
    [observer_ browsingDataRemover:remover didRemoveBrowsingDataWithMask:mask];
  }
}
