// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_NAVIGATION_ITEM_HOLDER_H_
#define IOS_WEB_NAVIGATION_CRW_NAVIGATION_ITEM_HOLDER_H_

#import <WebKit/WebKit.h>

#include <memory>

NS_ASSUME_NONNULL_BEGIN

namespace web {
class NavigationItemImpl;
}  // namespace web

// An NSObject wrapper for an std::unique_ptr<NavigationItemImpl>. This object
// is used to associate a NavigationItemImpl to a WKBackForwardListItem to store
// additional navigation states needed by the embedder.
@interface CRWNavigationItemHolder : NSObject

// Returns the CRWNavigationItemHolder object associated with `item`. Creates an
// empty holder if none currently exists for `item`.
+ (instancetype)holderForBackForwardListItem:(WKBackForwardListItem*)item;

- (instancetype)init NS_UNAVAILABLE;

// Designated init method that associates the new instance with `item`.
- (instancetype)initWithBackForwardListItem:(WKBackForwardListItem*)item;

// Returns the NavigationItemImpl stored in this instance.
- (web::NavigationItemImpl*)navigationItem;

// Stores the NavigationItemImpl object in this instance.
- (void)setNavigationItem:
    (std::unique_ptr<web::NavigationItemImpl>)navigationItem;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_NAVIGATION_CRW_NAVIGATION_ITEM_HOLDER_H_
