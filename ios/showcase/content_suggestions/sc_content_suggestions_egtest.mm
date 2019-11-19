// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns a text label beginning with |text|.
id<GREYMatcher> TextBeginsWith(NSString* text) {
  GREYMatchesBlock matches = ^BOOL(id element) {
    return [[element text] hasPrefix:text];
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
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
      onElementWithMatcher:grey_accessibilityID(
                               kContentSuggestionsCollectionIdentifier)];
}

// Select the cell with the |ID| by scrolling the collection.
GREYElementInteraction* CellWithID(NSString* ID) {
  return CellWithMatcher(grey_accessibilityID(ID));
}

}  // namespace

// Tests for the suggestions view controller.
@interface SCContentSuggestionsTestCase : ShowcaseTestCase
@end

@implementation SCContentSuggestionsTestCase

// Tests launching ContentSuggestionsViewController.
- (void)testLaunch {
  showcase_utils::Open(@"ContentSuggestionsViewController");
  NSString* section_header = [l10n_util::GetNSStringWithFixup(
      IDS_NTP_ARTICLE_SUGGESTIONS_SECTION_HEADER) uppercaseString];
  [CellWithMatcher(
      grey_allOf(grey_accessibilityLabel(section_header),
                 grey_accessibilityTrait(UIAccessibilityTraitStaticText), nil))
      assertWithMatcher:grey_notNil()];
  [CellWithMatcher(
      grey_allOf(grey_accessibilityLabel(l10n_util::GetNSStringWithFixup(
                     IDS_IOS_CONTENT_SUGGESTIONS_FOOTER_TITLE)),
                 grey_accessibilityTrait(UIAccessibilityTraitButton), nil))
      assertWithMatcher:grey_interactable()];
  showcase_utils::Close();
}

// Tests the opening of a suggestion item by tapping on it.
- (void)testOpenItem {
  showcase_utils::Open(@"ContentSuggestionsViewController");
  [CellWithID(@"Title of the first suggestions") performAction:grey_tap()];

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

  [CellWithID(@"Title of the first suggestions")
      performAction:grey_swipeFastInDirection(kGREYDirectionLeft)];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(@"dismissModals"),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  id<GREYMatcher> firstSuggestion =
      grey_allOf(grey_accessibilityID(@"Title of the first suggestions"),
                 grey_sufficientlyVisible(), nil);

  [[EarlGrey selectElementWithMatcher:firstSuggestion]
      assertWithMatcher:grey_nil()];

  showcase_utils::Close();
}

// Tests that long pressing an item starts a context menu.
- (void)testLongPressItem {
  showcase_utils::Open(@"ContentSuggestionsViewController");
  [CellWithID(@"Title of the first suggestions")
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

// Tests that tapping the "Learn More" item opens the help center.
- (void)testTapLearnMore {
  showcase_utils::Open(@"ContentSuggestionsViewController");
  [CellWithID(kContentSuggestionsLearnMoreIdentifier) performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              @"handleLearnMoreTapped"),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitStaticText),
                                          nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  showcase_utils::Close();
}

@end
