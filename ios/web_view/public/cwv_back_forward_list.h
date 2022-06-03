// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_BACK_FORWARD_LIST_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_BACK_FORWARD_LIST_H_

#import <UIKit/UIKit.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVBackForwardListItem;
@class CWVWebView;

// This just behaves like a NSArray<CWVBackForwardListItem*>.
//
// |objectAtIndexedSubscript:| and |NSFastEnumeration| are implemented to
// allow to use [] operator and a for-in statement to iterate |self|, like:
//   for (CWVBackForwardListItem* item in backList) { ... }
//   CWVBackForwardListItem* lastItem = backList[backList.count-1];
//
// WARNING: Do not use nested for-in statements to iterate one list at the
// same time, otherwise it will cause a use-after-free bug. Example:
//   for (CWVBackForwardListItem* item in forwardList) {  // OK
//     for (CWVBackForwardListItem* item2 in forwardList) {  // BAD!!
//       // |item| will be deallocated here so the above line is BAD!
//     }
//     // |item| has been deallocated, so using |item| will cause a
//     // use-after-free bug.
//     for (CWVBackForwardListItem* item2 in backList) {}  // OK
//   }
CWV_EXPORT
@interface CWVBackForwardListItemArray : NSObject <NSFastEnumeration>

// The number of items in this array-like |self|.
@property(nonatomic, readonly) NSUInteger count;

// This overloads the [] array-style subscripting operator.
- (CWVBackForwardListItem*)objectAtIndexedSubscript:(NSUInteger)index;

// Instances are supposed to be created only internally.
- (instancetype)init NS_UNAVAILABLE;

@end

// A equivalent of
// https://developer.apple.com/documentation/webkit/wkbackforwardlist
CWV_EXPORT
@interface CWVBackForwardList : NSObject

// A NSArray of CWVBackForwardListItem. The item with the greatest index
// will be closest to current item.
@property(nonatomic, readonly) CWVBackForwardListItemArray* backList;

// A NSArray of CWVBackForwardListItem. The item with index 0 will be
// closest to current item.
@property(nonatomic, readonly) CWVBackForwardListItemArray* forwardList;

// The current item, or nil if there isn't one.
@property(nonatomic, readonly, nullable) CWVBackForwardListItem* currentItem;

// The item in |backList| immediately preceding the current item, or nil if
// there isn't one.
@property(nonatomic, readonly, nullable) CWVBackForwardListItem* backItem;

// The item in |forwardList| immediately following the current item, or nil if
// there isn't one.
@property(nonatomic, readonly, nullable) CWVBackForwardListItem* forwardItem;

// Returns the item at a specified distance from the current item. For the
// meaning of |index|: 0 for the current item, -1 for the immediately preceding
// item, 1 for the immediately following item, and so on (which means |index|
// here is a OFFSET in fact). Returns nil if there isn't one.
- (nullable CWVBackForwardListItem*)itemAtIndex:(NSInteger)index;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_BACK_FORWARD_LIST_H_
