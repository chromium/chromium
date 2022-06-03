// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CONTENT_WIDGET_EXTENSION_MOST_VISITED_TILE_VIEW_H_
#define IOS_CHROME_CONTENT_WIDGET_EXTENSION_MOST_VISITED_TILE_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/elements/highlight_button.h"

@class FaviconView;

// View to display a Most Visited tile based on the suggestion.
// It displays the favicon for this Most Visited suggestion and its title.
@interface MostVisitedTileView : HighlightButton

// Returns the fixed width of a tile.
+ (CGFloat)tileWidth;

// Designated initializer.
- (nonnull instancetype)init NS_DESIGNATED_INITIALIZER;
- (nonnull instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (nonnull instancetype)initWithCoder:(nonnull NSCoder*)aDecoder NS_UNAVAILABLE;

// FaviconView displaying the favicon.
@property(nonatomic, strong, readonly, nonnull) FaviconView* faviconView;

// Title of the Most Visited.
@property(nonatomic, strong, readonly, nonnull) UILabel* titleLabel;

// URL of the Most Visited.
@property(nonatomic, strong, nullable) NSURL* URL;

@end

#endif  // IOS_CHROME_CONTENT_WIDGET_EXTENSION_MOST_VISITED_TILE_VIEW_H_
