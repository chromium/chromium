// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOCOMPLETE_TEST_AUTOCOMPLETE_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_AUTOCOMPLETE_TEST_AUTOCOMPLETE_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"

/// App interface for interacting with the autocomplete system in tests.
@interface AutocompleteAppInterface : NSObject

/// Sets up the fake suggestions autocomplete controller for `context`.
/// This **must** be called before starting the omnibox.
+ (void)enableFakeSuggestionsInContext:(OmniboxPresentationContext)context;

// Use these methods to add suggestions; note that match relevance is determined
// by insertion order (first in, highest relevance).
#pragma mark - Adding suggestions

/// Adds an URL shortcut match to the suggestions. Note that match relevance is
/// determined by insertion order (first in, highest relevance).
+ (void)addURLShortcutMatch:(NSString*)shortcutText
       destinationURLString:(NSString*)URLString
                    context:(OmniboxPresentationContext)context;

/// Adds a search suggestion with `searchText`.  Note that match relevance is
/// determined by insertion order (first in, highest relevance).
+ (void)addSearchSuggestion:(NSString*)query
                    context:(OmniboxPresentationContext)context;

/// Adds a URL suggestion with title and destination URL.  Note that match
/// relevance is determined by insertion order (first in, highest relevance).
+ (void)addHistoryURLSuggestion:(NSString*)title
           destinationURLString:(NSString*)URLString
                        context:(OmniboxPresentationContext)context;

@end

#endif  // IOS_CHROME_BROWSER_AUTOCOMPLETE_TEST_AUTOCOMPLETE_APP_INTERFACE_H_
