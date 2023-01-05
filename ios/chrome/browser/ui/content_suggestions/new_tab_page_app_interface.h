// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NEW_TAB_PAGE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NEW_TAB_PAGE_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// App interface for the NTP.
@interface NewTabPageAppInterface : NSObject

// Returns the short name of the default search engine.
+ (NSString*)defaultSearchEngine;

// Resets the default search engine to `defaultSearchEngine`.
// `defaultSearchEngine` should be its short name.
+ (void)resetSearchEngineTo:(NSString*)defaultSearchEngine;

// Returns the width the search field is supposed to have when the collection
// has `collectionWidth`. `traitCollection` is the trait collection of the view
// displaying the omnibox, its Size Class is used in the computation.
+ (CGFloat)searchFieldWidthForCollectionWidth:(CGFloat)collectionWidth
                              traitCollection:
                                  (UITraitCollection*)traitCollection;

// Returns the NTP collection view.
+ (UICollectionView*)collectionView;

// Returns the content suggestions collection view.
+ (UICollectionView*)contentSuggestionsCollectionView;

// Returns the fake omnibox.
+ (UIView*)fakeOmnibox;

// Returns the Discover header label.
+ (UILabel*)discoverHeaderLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NEW_TAB_PAGE_APP_INTERFACE_H_
