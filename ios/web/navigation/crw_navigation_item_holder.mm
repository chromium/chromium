// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_navigation_item_holder.h"

#import <objc/runtime.h>

#import "ios/web/navigation/navigation_item_impl.h"

// The address of this static variable is used to set and get the associated
// NavigationItemImpl object from a WKBackForwardListItem.
const void* kNavigationItemKey = &kNavigationItemKey;

@implementation CRWNavigationItemHolder {
  std::unique_ptr<web::NavigationItemImpl> _navigationItem;
}

+ (instancetype)holderForBackForwardListItem:(WKBackForwardListItem*)item {
  DCHECK(item);
  CRWNavigationItemHolder* holder =
      objc_getAssociatedObject(item, &kNavigationItemKey);
  if (!holder) {
    holder = [[CRWNavigationItemHolder alloc] initWithBackForwardListItem:item];
  }
  return holder;
}

- (instancetype)initWithBackForwardListItem:(WKBackForwardListItem*)item {
  self = [super init];
  if (self) {
    objc_setAssociatedObject(item, &kNavigationItemKey, self,
                             OBJC_ASSOCIATION_RETAIN);
  }
  return self;
}

- (web::NavigationItemImpl*)navigationItem {
  return _navigationItem.get();
}

- (void)setNavigationItem:
    (std::unique_ptr<web::NavigationItemImpl>)navigationItem {
  _navigationItem = std::move(navigationItem);
}

@end
