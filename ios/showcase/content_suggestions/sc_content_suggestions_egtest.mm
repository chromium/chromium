// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/ios/ios_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_learn_more_item.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/showcase/content_suggestions/sc_content_suggestions_data_source.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns a text label beginning with |text|.
id<GREYMatcher> TextBeginsWith(NSString* text) {
  MatchesBlock matches = ^BOOL(id element) {
    return [[element text] hasPrefix:text];
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:[NSString stringWithFormat:@"beginsWithText('%@')", text]];
  };
  id<GREYMatcher> matcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return grey_allOf(grey_anyOf(grey_kindOfClass([UILabel class]),
                               grey_kindOfClass([UITextField class]),
                               grey_kindOfClass([UITextView class]), nil),
                    matcher, nil);
}

// Select the cell with the |matcher| by scrolling the collection.
GREYElementInteraction* CellWithMatcher(id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_sufficientlyVisible(),
                                          nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 150)
      onElementWithMatcher:chrome_test_util::ContentSuggestionCollectionView()];
}

// Select the cell with the |ID| by scrolling the collection.
GREYElementInteraction* CellWithID(NSString* ID) {
  return CellWithMatcher(grey_accessibilityID(ID));
}

// Returns the string displayed when the Reading List section is empty.
NSString* ReadingListEmptySection() {
  return [NSString
      stringWithFormat:@"%@, %@",
                       l10n_util::GetNSString(IDS_NTP_TITLE_NO_SUGGESTIONS),
                       l10n_util::GetNSString(
                           IDS_NTP_READING_LIST_SUGGESTIONS_SECTION_EMPTY)];
}
}  // namespace

// Tests for the suggestions view controller.
@interface SCContentSuggestionsTestCase : ShowcaseTestCase
@end

@implementation SCContentSuggestionsTestCase

// Per crbug.com/845186, Disable flakey iPad Retina tests that are limited
// to iOS 10.2.
+ (NSArray*)testInvocations {
#if TARGET_IPHONE_SIMULATOR
  if (IsIPadIdiom() && !base::ios::IsRunningOnOrLater(10, 3, 0))
    return @[];
#endif  // TARGET_IPHONE_SIMULATOR
  return [super testInvocations];
}

// Tests launching ContentSuggestionsViewController.
- (void)testLaunch {
  showcase_utils::Open(@"ContentSuggestionsViewController");
  NSString* section_header = l10n_util::GetNSStringWithFixup(
      IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_HEADER);
  if (IsUIRefreshPhase1Enabled()) {
    section_header = [section_header uppercaseString];
  }
  [CellWithMatcher(chrome_test_util::StaticTextWithAccessibilityLabel(
      section_header)) assertWithMatcher:grey_notNil()];
  [CellWithMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE))
      assertWithMatcher:grey_interactable()];
  showcase_utils::Close();
}

// Tests the opening of a suggestion item by tapping on it.
- (void)testOpenItem {
  showcase_utils::Open(@"ContentSuggestionsViewController");
  [CellWithID([SCContentSuggestionsDataSource titleFirstSuggestion])
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:TextBeginsWith(@"openPageForItemAtIndexPath:")]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  showcase_utils::Close();
}

// Tests dismissing an item with swipe-to-dismiss.
- (void)testSwipeToDismiss {
  showcase_utils::Open(@"ContentSuggestionsViewController");

  [CellWithID([SCContentSuggestionsDataSource titleFirstSuggestion])
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(@"dismissModals"),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              [SCContentSuggestionsDataSource
                                                  titleFirstSuggestion]),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];

  showcase_utils::Close();
}

// Tests that long pressing an item starts a context menu.
- (void)testLongPressItem {
  showcase_utils::Open(@"ContentSuggestionsViewController");
  [CellWithID([SCContentSuggestionsDataSource titleFirstSuggestion])
      performAction:grey_longPress()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   TextBeginsWith(
                                       @"displayContextMenuForSuggestion:"),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  showcase_utils::Close();
}

// Tests that swipe-to-dismiss on empty item does nothing.
- (void)testNoSwipeToDismissEmptyItem {
  if (IsUIRefreshPhase1Enabled()) {
    EARL_GREY_TEST_DISABLED(
        @"Test disabled in UI Refresh as there's no reading list.");
  }
  showcase_utils::Open(@"ContentSuggestionsViewController");
  [CellWithID([SCContentSuggestionsDataSource titleReadingListItem])
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          ReadingListEmptySection())]
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  // Check that it is not dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          ReadingListEmptySection())]
      assertWithMatcher:grey_sufficientlyVisible()];

  showcase_utils::Close();
}

// Tests that tapping the "Learn More" item opens the help center.
- (void)testTapLearnMore {
  showcase_utils::Open(@"ContentSuggestionsViewController");
  [CellWithID([ContentSuggestionsLearnMoreItem accessibilityIdentifier])
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          StaticTextWithAccessibilityLabel(
                                              @"handleLearnMoreTapped")]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  showcase_utils::Close();
}

@end
