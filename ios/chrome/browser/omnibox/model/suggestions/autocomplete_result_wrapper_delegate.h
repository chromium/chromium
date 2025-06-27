// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_AUTOCOMPLETE_RESULT_WRAPPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_AUTOCOMPLETE_RESULT_WRAPPER_DELEGATE_H_

@class AutocompleteResultWrapper;
@protocol AutocompleteSuggestionGroup;

// The autocomplete match wrapper delegate.
@protocol AutocompleteResultWrapperDelegate

/// Informs the delegate when pedals are invalidated.
- (void)autocompleteResultWrapper:(AutocompleteResultWrapper*)wrapper
              didInvalidatePedals:(NSArray<id<AutocompleteSuggestionGroup>>*)
                                      nonPedalSuggestionsGroups;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_SUGGESTIONS_AUTOCOMPLETE_RESULT_WRAPPER_DELEGATE_H_
