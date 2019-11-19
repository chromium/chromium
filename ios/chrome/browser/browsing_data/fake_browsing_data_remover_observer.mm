// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/fake_browsing_data_remover_observer.h"

#include <memory>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
