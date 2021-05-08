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

// The cached scroll position of the NTP.
@property(nonatomic, assign) CGFloat offset;

// The total scroll height of the NTP.
@property(nonatomic, assign) CGFloat ntpHeight;

// The parent collection view that contains the content suggestions collection
// view.
@property(nonatomic, weak) UICollectionView* parentCollectionView;

// Provides information relating to the fake omnibox size.
@property(nonatomic, weak) id<NewTabPageOmniboxPositioning> omniboxPositioner;

// Whether or not the user has scrolled into the feed, transferring ownership of
// the omnibox to allow it to stick to the top of the NTP.
@property(nonatomic, assign) BOOL isScrolledIntoFeed;

// Creates layout with |offset| as additional height. Allows the view's height
// to be increased enough to maintain the scroll position. Only needed if
// Discover feed is visible.
// TODO(crbug.com/1200303): Change |refactoredFeedVisible| to only represent
// feed visibility after launch.
- (instancetype)initWithOffset:(CGFloat)offset
         refactoredFeedVisible:(BOOL)visible;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_LAYOUT_H_
