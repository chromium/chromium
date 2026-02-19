// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/omnibox/browser/omnibox_pref_names.h"
#import "ios/chrome/browser/autocomplete/test/autocomplete_app_interface.h"
#import "ios/chrome/browser/composebox/eg_tests/inttest/composebox_inttest_app_interface.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/omnibox/eg_tests/omnibox_matchers.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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

}  // namespace

// Tests the composebox integration with ComposeboxInttestCoordinator.
@interface ComposeboxInttestTestCase : ChromeTestCase
@end

@implementation ComposeboxInttestTestCase

- (BOOL)loadMinimalAppUI {
  return YES;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kComposeboxIOS);
  config.features_enabled.push_back(kComposeboxIpad);

  if ([self isRunningTest:@selector
            (testAdaptiveSuggestionURLSuggestionIsVisible)] ||
      [self isRunningTest:@selector
            (testAdaptiveSuggestionSearchBelowURLIsHidden)]) {
    // Adaptive suggestions tests only work with bottom composebox as
    // suggestions are not considered hidden when they are covered by the
    // keyboard. To workaround this, use bottom composebox where suggestions
    // covered by it will be detected a not visible.
    config.features_disabled.push_back(kComposeboxForceTop);
    config.features_disabled.push_back(kComposeboxCompactMode);
  }

  return config;
}

- (void)setUp {
  [super setUp];
  [ChromeCoordinatorAppInterface startBrowser];
  [AutocompleteAppInterface
      enableFakeSuggestionsInContext:kPresentationContext];
}

- (void)tearDownHelper {
  [super tearDownHelper];
  [ChromeEarlGrey setBoolValue:NO
             forLocalStatePref:omnibox::kIsOmniboxInBottomPosition];
  [ChromeCoordinatorAppInterface reset];
}

#pragma mark - Helpers

/// Returns the URL suggestion index after setting up the test. Or -1 if not
/// found.
- (int)setupAdaptiveSuggestionTestAndReturnURLSuggestionIndex {
  // Suggestions are sorted with search above URLs. Without adaptive
  // suggestions, on phone some suggestions might be covered by the keyboard or
  // the bottom composebox. Adaptive suggestions will first sort suggestions
  // that are visible so high ranked URLs will stay visible above the keyboard.

  // Setup bottom composebox and populate suggestions.
  [ChromeEarlGrey setBoolValue:YES
             forLocalStatePref:omnibox::kIsOmniboxInBottomPosition];

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

  // Adaptive suggestions is not availabe on ZPS (zero prefix state, so type
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

  // Iterate through search suggestions until we find the first non search or
  // non visible suggestion.
  for (NSInteger i = 1; i < kNumberOfFakeSuggestions; i++) {
    NSString* searchQuery = SearchQuery(i);
    NSIndexPath* indexPath = [NSIndexPath indexPathForRow:i inSection:0];
    BOOL isSearchSuggestionAndVisible = [ChromeEarlGrey
        isMatcherSufficientlyVisible:
            grey_allOf(omnibox::PopupRowAtIndex(indexPath),
                       chrome_test_util::OmniboxPopupRowWithString(searchQuery),
                       nil)];
    if (!isSearchSuggestionAndVisible) {
      // This index either contains the URL suggestion or is the first hidden
      // row. This is where we expect the URL suggestion to be.
      return i;
    }
  }
  return -1;
}

#pragma mark - Tests

// Tests that the high ranked URL suggestion is visible.
- (void)testAdaptiveSuggestionURLSuggestionIsVisible {
  // Bottom composebox wich is required for this test is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped as bottom composebox is not available on iPad.");
  }

  int urlSuggestionIndex =
      [self setupAdaptiveSuggestionTestAndReturnURLSuggestionIndex];

  // Verify that the URL suggestion is present and visible at the identified
  // index.
  NSIndexPath* urlIndexPath = [NSIndexPath indexPathForRow:urlSuggestionIndex
                                                 inSection:0];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   omnibox::PopupRowAtIndex(urlIndexPath),
                                   chrome_test_util::OmniboxPopupRowWithString(
                                       kURLSuggestionTitle),
                                   nil)]
      // Lower the visibility percentage as the last suggestion might be
      // partially covered by the gradient or the composebox.
      assertWithMatcher:grey_minimumVisiblePercent(0.4)];
}

// Tests that the search suggestion below the high ranked URL suggestion is
// hidden.
- (void)testAdaptiveSuggestionSearchBelowURLIsHidden {
  // Bottom composebox wich is required for this test is not available on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"Skipped as bottom composebox is not available on iPad.");
  }

  int urlSuggestionIndex =
      [self setupAdaptiveSuggestionTestAndReturnURLSuggestionIndex];

  // See comment in `setupAdaptiveSuggestionTestAndReturnURLSuggestionIndex`.
  // Verify that search N is present at index N+1 and is not visible.
  int displacedSearchNumber = urlSuggestionIndex;
  if (displacedSearchNumber < kNumberOfSearchSuggestions) {
    NSString* displacedSearchQuery = SearchQuery(displacedSearchNumber);
    NSIndexPath* displacedSearchIndex =
        [NSIndexPath indexPathForRow:displacedSearchNumber + 1 inSection:0];
    [[EarlGrey selectElementWithMatcher:
                   grey_allOf(omnibox::PopupRowAtIndex(displacedSearchIndex),
                              chrome_test_util::OmniboxPopupRowWithString(
                                  displacedSearchQuery),
                              nil)]
        assertWithMatcher:grey_allOf(grey_notNil(),
                                     grey_not(grey_sufficientlyVisible()),
                                     nil)];
  }
}

@end
