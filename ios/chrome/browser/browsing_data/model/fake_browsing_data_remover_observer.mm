// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/fake_browsing_data_remover_observer.h"

#import <memory>

@implementation FakeBrowsingDataRemoverObserver {
  std::unique_ptr<TestDidRemoveBrowsingDataWithMaskInfo>
      _didRemoveBrowsingDataWithMaskInfo;
}

- (TestDidRemoveBrowsingDataWithMaskInfo*)didRemoveBrowsingDataWithMaskInfo {
  return _didRemoveBrowsingDataWithMaskInfo.get();
}

- (void)browsingDataRemover:(BrowsingDataRemover*)remover
    didRemoveBrowsingDataWithMask:(BrowsingDataRemoveMask)mask {
  _didRemoveBrowsingDataWithMaskInfo =
      std::make_unique<TestDidRemoveBrowsingDataWithMaskInfo>();
  _didRemoveBrowsingDataWithMaskInfo->remover = remover;
  _didRemoveBrowsingDataWithMaskInfo->mask = mask;
}

@end
