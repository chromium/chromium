// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autocomplete/test/autocomplete_app_interface.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_coordinator_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

/// Omnibox presentation context for the composebox.
const OmniboxPresentationContext kPresentationContext =
    OmniboxPresentationContext::kComposebox;

/// Returns the query for search suggestion `number`.
NSString* SearchQuery(int number) {
  return [[NSString alloc] initWithFormat:@"search %d", number];
}

/// Title of the URL suggestion added in adaptive suggestions tests.
NSString* kURLSuggestionTitle = @"URL suggestion";
/// Number of fake search suggestions added in adaptive suggestions tests
const int kNumberOfSearchSuggestions = 12;
/// Number of suggestions added in adaptive suggestions tests, search
/// suggestions + 1 verbatim + 1 URL suggestion.
const int kNumberOfFakeSuggestions = kNumberOfSearchSuggestions + 2;

/// Returns a matcher for the popup row at `row` containing `string`. Hidden
/// rows are excluded as the table view keeps reused cells hidden in the view
/// hierarchy with the same accessibility identifier as the visible row.
id<GREYMatcher> PopupRowMatcher(NSInteger row, NSString* string) {
  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:row inSection:0];
  return grey_allOf(omnibox::PopupRowAtIndex(indexPath),
                    chrome_test_util::OmniboxPopupRowWithString(string),
                    grey_hidden(NO), nil);
}

}  // namespace

// Tests the composebox adaptive suggestions integration.
@interface ComposeboxAdaptiveSuggestionsTestCase : ChromeTestCase {
  int _urlSuggestionIndex;
}
@end

@implementation ComposeboxAdaptiveSuggestionsTestCase

- (BOOL)loadMinimalAppUI {
  return YES;
}

- (void)setUp {
  [super setUp];
  [ChromeCoordinatorAppInterface startBrowser];
  [AutocompleteAppInterface
      enableFakeSuggestionsInContext:kPresentationContext];

  // Suggestions are sorted with search above URLs. Without adaptive
  // suggestions, on phone some suggestions might be covered by the keyboard.
  // Adaptive suggestions will first sort suggestions that are visible so high
  // ranked URLs will stay visible above the keyboard.

  // Suggestions are populated and ranked as:
  // Verbatim
  // URL suggestion (high ranked, should remain visible below searches)
  // search 1
  // search 2
  // ...
  // search `kNumberOfSearchSuggestions`

  // Add URL suggestion first for high ranking.
  [AutocompleteAppInterface addHistoryURLSuggestion:kURLSuggestionTitle
                               destinationURLString:@"https://www.someurl.com"
                                            context:kPresentationContext];
  // Add kNumberOfSearchSuggestions search suggestions from 1 to
  // kNumberOfSearchSuggestions.
  for (int i = 1; i <= kNumberOfSearchSuggestions; ++i) {
    [AutocompleteAppInterface addSearchSuggestion:SearchQuery(i)
                                          context:kPresentationContext];
  }

  [ChromeCoordinatorAppInterface startComposeboxCoordinator];

  // Adaptive suggestions are not available on ZPS (zero prefix state, so type
  // something).
  [ChromeEarlGreyUI replaceTextInOmnibox:@"search"];

  // When adaptive suggestions are active, the URL suggestion is moved up to
  // the last visible slot to ensure it remains accessible, even though search
  // suggestions are generally prioritized. This shifts the remaining search
  // suggestions down.
  //
  // Without adaptive suggestions the ordering would be:
  // 0. Verbatim
  // 1. search 1
  // ...
  // N-1. search N-1
  // N. search N
  // ------ (suggestions below this line are not visible)
  // N+1. search N+1
  // URL suggestion
  //
  //
  // With adaptive suggestions the expected order is:
  // 0. Verbatim
  // 1. search 1
  // ...
  // N-1. search N-1
  // N. URL suggestion
  // ------ (suggestions below this line are not visible)
  // N+1. search N <-- search N has been displaced to index N+1
  // ...

  _urlSuggestionIndex = -1;
  // Iterate through search suggestions until we find the first non search or
  // non visible suggestion.
  for (NSInteger i = 1; i < kNumberOfFakeSuggestions; i++) {
    BOOL isSearchSuggestionAndVisible = [ChromeEarlGrey
        isMatcherSufficientlyVisible:PopupRowMatcher(i, SearchQuery(i))];
    if (!isSearchSuggestionAndVisible) {
      // This index either contains the URL suggestion or is the first hidden
      // row. This is where we expect the URL suggestion to be.
      _urlSuggestionIndex = i;
      break;
    }
  }
}

- (void)tearDownHelper {
  [super tearDownHelper];
  [ChromeCoordinatorAppInterface reset];
}

#pragma mark - Tests

// Tests that the high ranked URL suggestion is visible.
- (void)testAdaptiveSuggestionURLSuggestionIsVisible {
  // Adaptive suggestions require the keyboard to cover the suggestions, which
  // is only the case on iPhone.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped as the keyboard covers the suggestions "
                           @"only on iPhone.");
  }

  // Verify that the URL suggestion is present and visible at the identified
  // index.
  [[EarlGrey selectElementWithMatcher:PopupRowMatcher(_urlSuggestionIndex,
                                                      kURLSuggestionTitle)]
      // Lower the visibility percentage as the last suggestion might be
      // partially covered by the gradient or the keyboard.
      assertWithMatcher:grey_minimumVisiblePercent(0.4)];
}

// Tests that the search suggestion below the high ranked URL suggestion is
// hidden.
- (void)testAdaptiveSuggestionSearchBelowURLIsHidden {
  // Adaptive suggestions require the keyboard to cover the suggestions, which
  // is only the case on iPhone.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped as the keyboard covers the suggestions "
                           @"only on iPhone.");
  }

  // Verify that search N is present at index N+1 and is not visible.
  int displacedSearchNumber = _urlSuggestionIndex;
  if (displacedSearchNumber < kNumberOfSearchSuggestions) {
    [[EarlGrey selectElementWithMatcher:PopupRowMatcher(
                                            displacedSearchNumber + 1,
                                            SearchQuery(displacedSearchNumber))]
        assertWithMatcher:grey_allOf(grey_notNil(),
                                     grey_not(grey_sufficientlyVisible()),
                                     nil)];
  }
}

@end
