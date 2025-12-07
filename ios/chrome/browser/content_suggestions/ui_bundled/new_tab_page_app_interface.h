// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_NEW_TAB_PAGE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_NEW_TAB_PAGE_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

@class NewTabPageColorPalette;

// App interface for the NTP.
@interface NewTabPageAppInterface : NSObject

// Returns the width the search field is supposed to have when the collection
// has `collectionWidth`. `traitCollection` is the trait collection of the view
// displaying the omnibox, its Size Class is used in the computation.
+ (CGFloat)searchFieldWidthForCollectionWidth:(CGFloat)collectionWidth
                              traitCollection:
                                  (UITraitCollection*)traitCollection;

// Returns the NTP parent view.
+ (UIView*)NTPView;

// Returns the NTP collection view.
+ (UICollectionView*)collectionView;

// Returns the content suggestions collection view.
+ (UICollectionView*)contentSuggestionsCollectionView;

// Returns the fake omnibox.
+ (UIView*)fakeOmnibox;

// Returns the Discover header label.
+ (UILabel*)discoverHeaderLabel;

// Disables Chrome Tips cards via a pref.
+ (void)disableTipsCards;

// Resets SetUpList prefs to clear any completed items.
+ (void)resetSetUpListPrefs;

// Returns YES if the Default Browser SetUpListItemView item in the Magic Stack
// is complete.
+ (BOOL)setUpListItemDefaultBrowserInMagicStackIsComplete;

// Returns YES if the Autofill SetUpListItemView item in the Magic Stack is
// complete.
+ (BOOL)setUpListItemAutofillInMagicStackIsComplete;

// Returns the current color palette of the NTP's background.
+ (NewTabPageColorPalette*)currentBackgroundColor;

// Returns whether the NTP has a custom background image.
+ (BOOL)hasBackgroundImage;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_UI_BUNDLED_NEW_TAB_PAGE_APP_INTERFACE_H_
