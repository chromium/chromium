// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_PINNED_TAB_COLLECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_PINNED_TAB_COLLECTION_CONSUMER_H_

#import <vector>

#import "ios/chrome/browser/ui/tab_switcher/pinned_tab_collection_consumer.h"

namespace web {
class WebStateID;
}  // namespace web

// Test object that conforms to PinnedTabCollectionConsumer and exposes inner
// state for test verification.
@interface FakePinnedTabCollectionConsumer
    : NSObject <PinnedTabCollectionConsumer>

// The fake consumer only keeps the identifiers of items for simplicity.
@property(nonatomic, readonly) const std::vector<web::WebStateID>& items;
@property(nonatomic, assign) web::WebStateID selectedItemID;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_PINNED_TAB_COLLECTION_CONSUMER_H_
