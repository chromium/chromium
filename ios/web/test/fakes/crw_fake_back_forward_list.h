// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_TEST_BACK_FORWARD_LIST_H_
#define IOS_WEB_NAVIGATION_CRW_TEST_BACK_FORWARD_LIST_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class WKBackForwardListItem;

// A CRWFakeBackForwardList can be used to stub out WKBackForwardList in tests.
@interface CRWFakeBackForwardList : NSObject

// Returns an OCMock of WKBackForwardListItem with the given URL.
+ (WKBackForwardListItem*)itemWithURLString:(NSString*)URL;

// WKBackForwardList interface
@property(nullable, nonatomic, copy) NSArray<WKBackForwardListItem*>* backList;
@property(nullable, nonatomic, copy)
    NSArray<WKBackForwardListItem*>* forwardList;
@property(nullable, nonatomic, strong) WKBackForwardListItem* currentItem;
@property(nullable, nonatomic, readonly, strong)
    WKBackForwardListItem* backItem;
@property(nullable, nonatomic, readonly, strong)
    WKBackForwardListItem* forwardItem;
- (nullable WKBackForwardListItem*)itemAtIndex:(NSInteger)index;

// Resets this instance to simulate a session with no back/forward history, and
// a single entry for |currentItemURL| if it is not nil. If |currentItemURL| is
// nil, the session is reset to empty.
- (void)setCurrentURL:(nullable NSString*)currentItemURL;

// Resets this instance to simulate a session with the current entry at
// |currentItemURL|, and back and forward history entries as specified in
// |backListURLs| and |forwardListURLs|.
- (void)setCurrentURL:(NSString*)currentItemURL
         backListURLs:(nullable NSArray<NSString*>*)backListURLs
      forwardListURLs:(nullable NSArray<NSString*>*)forwardListURLs;

// Simulates go-to-index operation on this instance. Shuffles the items in this
// instance such that currentItem is at position |index| of the logical list
// formed by backList + currentItem + forwardList. backList[0] has index 0.
// |index| must be in the range [0, # of items in this instance).
- (void)moveCurrentToIndex:(NSUInteger)index;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_NAVIGATION_CRW_TEST_BACK_FORWARD_LIST_H_
