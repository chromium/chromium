// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/browsing_data_remover_observer_bridge.h"

#import "ios/chrome/browser/browsing_data/fake_browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/fake_browsing_data_remover_observer.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class BrowsingDataRemoverObserverBridgeTest : public PlatformTest {
 protected:
  BrowsingDataRemoverObserverBridgeTest()
      : remover_(std::make_unique<FakeBrowsingDataRemover>()),
        observer_([[FakeBrowsingDataRemoverObserver alloc] init]),
        observer_bridge_(observer_) {}

  std::unique_ptr<FakeBrowsingDataRemover> remover_;
  FakeBrowsingDataRemoverObserver* observer_;
  BrowsingDataRemoverObserverBridge observer_bridge_;
};

// Tests |OnBrowsingDataRemoved| forwarding.
TEST_F(BrowsingDataRemoverObserverBridgeTest, OnBrowsingDataRemoved) {
  ASSERT_FALSE([observer_ didRemoveBrowsingDataWithMaskInfo]);
  observer_bridge_.OnBrowsingDataRemoved(remover_.get(),
                                         BrowsingDataRemoveMask::REMOVE_ALL);
  ASSERT_TRUE([observer_ didRemoveBrowsingDataWithMaskInfo]);
  EXPECT_EQ(remover_.get(),
            [observer_ didRemoveBrowsingDataWithMaskInfo] -> remover);
  EXPECT_EQ(BrowsingDataRemoveMask::REMOVE_ALL,
            [observer_ didRemoveBrowsingDataWithMaskInfo] -> mask);
}
