// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_observer_bridge.h"

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
