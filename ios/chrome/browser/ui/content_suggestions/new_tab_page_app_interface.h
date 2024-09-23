// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NEW_TAB_PAGE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NEW_TAB_PAGE_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

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

// Disables the SetUpList via a pref.
+ (void)disableSetUpList;

// Resets SetUpList prefs to clear any completed items.
+ (void)resetSetUpListPrefs;

// Returns YES if the Default Browser SetUpListItemView item in the Magic Stack
// is complete.
+ (BOOL)setUpListItemDefaultBrowserInMagicStackIsComplete;

// Returns YES if the Autofill SetUpListItemView item in the Magic Stack is
// complete.
+ (BOOL)setUpListItemAutofillInMagicStackIsComplete;

// Returns the title of the Set Up List.
+ (NSString*)setUpListTitle;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NEW_TAB_PAGE_APP_INTERFACE_H_
