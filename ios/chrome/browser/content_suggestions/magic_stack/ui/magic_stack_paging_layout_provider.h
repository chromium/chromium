// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_PAGING_LAYOUT_PROVIDER_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_PAGING_LAYOUT_PROVIDER_H_

#import <UIKit/UIKit.h>

// Protocol implemented by collection view layouts that define their own
// pagination math.
@protocol MagicStackPagingLayoutProvider <NSObject>

// Calculates the target content offset for pagination given user drag velocity
// and bounds.
- (CGFloat)targetPageOffsetForOffset:(CGFloat)currentOffset
                            velocity:(CGFloat)velocity
                     traitCollection:(UITraitCollection*)traitCollection
                          viewBounds:(CGRect)bounds
                         currentPage:(NSUInteger*)pageIndex;

// Calculates the target content offset for a given page index.
- (CGFloat)offsetForPage:(NSUInteger)page
         traitCollection:(UITraitCollection*)traitCollection
              viewBounds:(CGRect)bounds;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_MAGIC_STACK_UI_MAGIC_STACK_PAGING_LAYOUT_PROVIDER_H_
