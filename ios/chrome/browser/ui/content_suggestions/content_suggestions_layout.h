// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_LAYOUT_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_LAYOUT_H_

#import <MaterialComponents/MDCCollectionViewFlowLayout.h>

@protocol NewTabPageOmniboxPositioning;

// Layout used for ContentSuggestions. It makes sure the collection is high
// enough to be scrolled up to the point the fake omnibox is hidden. For size
// classes other than RegularXRegular, this layout makes sure the fake omnibox
// is pinned to the top of the collection.
@interface ContentSuggestionsLayout : MDCCollectionViewFlowLayout

// The parent collection view that contains the content suggestions collection
// view.
@property(nonatomic, weak) UICollectionView* parentCollectionView;

// Provides information relating to the fake omnibox size.
@property(nonatomic, weak) id<NewTabPageOmniboxPositioning> omniboxPositioner;

// Whether or not the user has scrolled into the feed, transferring ownership of
// the omnibox to allow it to stick to the top of the NTP.
@property(nonatomic, assign) BOOL isScrolledIntoFeed;

// Minimum height of the NTP scroll view to allow for scrolling to omnibox.
- (CGFloat)minimumNTPHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_LAYOUT_H_
