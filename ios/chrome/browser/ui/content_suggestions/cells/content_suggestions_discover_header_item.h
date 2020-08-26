// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_DISCOVER_HEADER_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_DISCOVER_HEADER_ITEM_H_

#import <MaterialComponents/MaterialCollectionCells.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"

@class ContentSuggestionsDiscoverHeaderCell;

// Item representing header on top of Discover feed.
@interface ContentSuggestionsDiscoverHeaderItem : CollectionViewItem

// The title for the feed header label.
@property(nonatomic, copy) NSString* title;

// Represents whether the Discover feed is visible or hidden.
@property(nonatomic, assign) BOOL discoverFeedVisible;

// Initializes header with 'title' as main label.
- (instancetype)initWithType:(NSInteger)type
         discoverFeedVisible:(BOOL)visible NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(NSInteger)type NS_UNAVAILABLE;

@end

// Cell containing view for Discover feed header.
@interface ContentSuggestionsDiscoverHeaderCell : MDCCollectionViewCell

// Button for opening feed-level controls menu.
@property(nonatomic, readonly, strong) UIButton* menuButton;

// Title label for the feed.
@property(nonatomic, copy) NSString* title;

// Changes header UI based on Discover feed visibility.
- (void)changeHeaderForFeedVisible:(BOOL)visible;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_DISCOVER_HEADER_ITEM_H_
