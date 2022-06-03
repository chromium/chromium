// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_back_forward_list_internal.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVBackForwardListItemArray {
  BOOL _isBackList;

  CWVBackForwardListItem* _fastEnumerationItemCache;
}

- (instancetype)initWithBackForwardList:(CWVBackForwardList*)list
                             isBackList:(BOOL)isBackList {
  self = [super init];
  if (self) {
    _list = list;
    _isBackList = isBackList;

    _fastEnumerationItemCache = nil;
  }
  return self;
}

- (NSUInteger)count {
  if (!self.list.navigationManager) {
    return 0;
  }

  int currentIndex = self.list.navigationManager->GetLastCommittedItemIndex();
  if (currentIndex == -1) {
    // If there is no item in the list, currentIndex will be -1 so at that time
    // |self| should be empty array.
    return 0;
  }

  if (_isBackList) {
    return currentIndex;
  }
  // |self| is forwardList.
  int count = self.list.navigationManager->GetItemCount();
  DCHECK(count >= currentIndex + 1);
  return count - currentIndex - 1;
}

- (CWVBackForwardListItem*)objectAtIndexedSubscript:(NSUInteger)index {
  if (index >= self.count) {
    [NSException raise:NSRangeException
                format:@"The index of %@ is out of boundary",
                       _isBackList ? @"backList" : @"forwardList"];
  }

  int internalIndex;
  if (_isBackList) {
    internalIndex = index;
  } else {
    internalIndex =
        self.list.navigationManager->GetLastCommittedItemIndex() + 1 + index;
  }

  web::NavigationItem* item =
      self.list.navigationManager->GetItemAtIndex(internalIndex);
  DCHECK(item);
  return [[CWVBackForwardListItem alloc] initWithNavigationItem:item];
}

#pragma mark - NSFastEnumeration

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState*)state
                                  objects:(id __unsafe_unretained[])stackbuf
                                    count:(NSUInteger)len {
  DCHECK_GE(self.count, state->state);
  DCHECK_GE(len, 1UL);
  if (self.count == state->state) {
    return 0;
  }

  // |state| is a pure C-struct so retaining an item in |state| is impossible.
  // So, |_fastEnumerationItemCache| is designed to retain the returned item.
  // It is the caller's responsibility to ensure this is not deallocated during
  // enumeration, so a caller should not use nested for-in statement on the same
  // list, otherwise it will cause a use-after-free bug.
  _fastEnumerationItemCache = [self objectAtIndexedSubscript:state->state];

  // This tells Obj-C runtime that |self| is immutable (because
  // |&state->extra[0]| is constant) during enumeration.
  state->mutationsPtr = &state->extra[0];

  state->itemsPtr = stackbuf;
  state->itemsPtr[0] = _fastEnumerationItemCache;
  ++state->state;
  return 1;
}

@end

@implementation CWVBackForwardList

@synthesize backList = _backList;
@synthesize forwardList = _forwardList;

- (instancetype)initWithNavigationManager:
    (const web::NavigationManager*)navigationManager {
  self = [super init];
  if (self) {
    DCHECK(navigationManager);
    _navigationManager = navigationManager;
    _forwardList =
        [[CWVBackForwardListItemArray alloc] initWithBackForwardList:self
                                                          isBackList:NO];
    _backList =
        [[CWVBackForwardListItemArray alloc] initWithBackForwardList:self
                                                          isBackList:YES];
  }
  return self;
}

- (CWVBackForwardListItem*)currentItem {
  if (!self.navigationManager) {
    return nil;
  }

  web::NavigationItem* item = self.navigationManager->GetLastCommittedItem();
  if (!item) {
    return nil;
  }
  return [[CWVBackForwardListItem alloc] initWithNavigationItem:item];
}

- (CWVBackForwardListItem*)backItem {
  if (self.backList.count == 0) {
    return nil;
  }
  return self.backList[self.backList.count - 1];
}

- (CWVBackForwardListItem*)forwardItem {
  if (self.forwardList.count == 0) {
    return nil;
  }
  return self.forwardList[0];
}

- (CWVBackForwardListItem*)itemAtIndex:(NSInteger)index {
  if (index == 0) {
    return self.currentItem;
  } else if (index < 0) {
    NSInteger internalIndex = self.backList.count + index;
    if (internalIndex < 0) {
      return nil;
    }
    return self.backList[internalIndex];
  } else {
    NSUInteger internalIndex = index - 1;
    if (internalIndex >= self.forwardList.count) {
      return nil;
    }
    return self.forwardList[internalIndex];
  }
}

- (int)internalIndexOfItem:(CWVBackForwardListItem*)item {
  int count = self.navigationManager->GetItemCount();
  for (int i = 0; i < count; i++) {
    if (item.uniqueID ==
        self.navigationManager->GetItemAtIndex(i)->GetUniqueID()) {
      return i;
    }
  }
  return -1;
}

@end
