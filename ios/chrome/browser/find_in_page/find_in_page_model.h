// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_MODEL_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_MODEL_H_

#include <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

// This is a simplified version of find_tab_helper.cc.
@interface FindInPageModel : NSObject

// Should find in page be displayed.
@property(nonatomic, assign) BOOL enabled;

// The current search string.
@property(copy, nonatomic, readonly) NSString* text;

// The number of matches for `text`.
@property(nonatomic, readonly) NSUInteger matches;

// The currently higlighted index.
@property(nonatomic, readonly) NSUInteger currentIndex;

// The content offset needed to display the `currentIndex` match.
@property(nonatomic, readonly) CGPoint currentPoint;

// Update the query string and the number of matches.
- (void)updateQuery:(NSString*)query matches:(NSUInteger)matches;
// Update the current match index and its found position.
- (void)updateIndex:(NSInteger)index atPoint:(CGPoint)point;

@end

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_FIND_IN_PAGE_MODEL_H_
