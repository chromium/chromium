// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_RESULT_WRAPPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_RESULT_WRAPPER_DELEGATE_H_

struct AutocompleteMatch;
@class AutocompleteResultWrapper;
@protocol AutocompleteSuggestionGroup;

// The autocomplete match wrapper delegate.
@protocol AutocompleteResultWrapperDelegate

// TODO(crbug.com/400626674): Move isStarredMatch logic to the wrapper so it
// doesn't rely on its delegate
/// Whether `match` is a starred/bookmarked match.
- (BOOL)isStarredMatch:(const AutocompleteMatch&)match;

/// Informs the delegate when pedals are invalidated.
- (void)autocompleteResultWrapper:(AutocompleteResultWrapper*)wrapper
              didInvalidatePedals:(NSArray<id<AutocompleteSuggestionGroup>>*)
                                      nonPedalSuggestionsGroups;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_AUTOCOMPLETE_RESULT_WRAPPER_DELEGATE_H_
