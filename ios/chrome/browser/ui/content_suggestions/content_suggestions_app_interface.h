// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

// App interface for the Content Suggestions.
@interface ContentSuggestionsAppInterface : NSObject

// Sets the fake service up.
+ (void)setUpService;

// Resets the service to the real, non-fake service to avoid leaking the fake.
+ (void)resetService;

// Marks the suggestions as available.
+ (void)makeSuggestionsAvailable;

// Disables the suggestions.
+ (void)disableSuggestions;

// Adds |numberOfSuggestions| suggestions to the list of suggestions provided.
// The suggestions have the name "chromium<suggestionNumber>" and the url
// http://chromium/<suggestionNumber>.
+ (void)addNumberOfSuggestions:(NSInteger)numberOfSuggestions
      additionalSuggestionsURL:(NSURL*)URL;

// Add one particular suggestion, following the convention explained above, with
// |suggestionNumber|.
+ (void)addSuggestionNumber:(NSInteger)suggestionNumber;

// Returns the short name of the default search engine.
+ (NSString*)defaultSearchEngine;

// Resets the default search engine to |defaultSearchEngine|.
// |defaultSearchEngine| should be its short name.
+ (void)resetSearchEngineTo:(NSString*)defaultSearchEngine;

// Sets the what's new promo to "Move to Dock".
+ (void)setWhatsNewPromoToMoveToDock;

// Resets the what's new promo.
+ (void)resetWhatsNewPromo;

// Returns the width the search field is supposed to have when the collection
// has |collectionWidth|.
+ (CGFloat)searchFieldWidthForCollectionWidth:(CGFloat)collectionWidth;

// Returns the collection view.
+ (UICollectionView*)collectionView;

// Returns the fake omnibox.
+ (UIView*)fakeOmnibox;

// Swizzles the callback for the search button tap action so it is recorded.
+ (void)swizzleSearchButtonLogging;

// Resets the swizzle for the search button tap, and return the whether the
// swizzled method was called.
+ (BOOL)resetSearchButtonLogging;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_APP_INTERFACE_H_
