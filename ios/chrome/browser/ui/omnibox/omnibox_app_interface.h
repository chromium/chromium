// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@protocol GREYAssertion;

// Contains the app-side implementation of helpers.
@interface OmniboxAppInterface : NSObject

// Rewrite google URLs to localhost so they can be loaded by the test server.
+ (void)rewriteGoogleURLToLocalhost;

// Forces a variation to be used on the current HTTP header provider. Returns
// YES if the forcing was successful.
+ (BOOL)forceVariationID:(int)variationID;

// Blocks `URL` from most visited sites.
+ (void)blockURLFromTopSites:(NSString*)URL;

// Set up a service that serves custom search suggestions in the omnibox. These
// replace suggestions provided by the suggest server. Fake suggestions are
// loaded from `filename` (json) and the file must be in
// ios/test/chrome/data/omnibox/. Fake suggestions can be generated with
// chrome://suggest-internals available on Desktop. The service must be teared
// down with `tearDownFakeSuggestionsService`.
+ (void)setUpFakeSuggestionsService:(NSString*)filename;

// Tear down test fake suggestions service.
+ (void)tearDownFakeSuggestionsService;

// Returns whether the shortcuts backend is initialized.
+ (BOOL)shortcutsBackendInitialized;

// Returns the number of suggestions in the shortcuts database.
+ (NSInteger)numberOfShortcutsInDatabase;

// Returns YES if `element` or `element.text` is a valid URL.
+ (BOOL)isElementURL:(id)element;

/// Asserts the omnibox text field `shouldHaveAutocompleteText`.
+ (id<GREYAssertion>)displaysInlineAutocompleteText:
    (BOOL)shouldHaveAutocompleteText;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_APP_INTERFACE_H_
