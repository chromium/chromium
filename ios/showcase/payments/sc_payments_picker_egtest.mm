// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/ios/ios_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;

// Returns the GREYMatcher for the section with the given title.
id<GREYMatcher> SectionWithTitle(NSString* title) {
  return grey_allOf(grey_text(title), grey_kindOfClass([UILabel class]),
                    grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the picker row with the given label. |selected|
// states whether or not the row must be selected.
id<GREYMatcher> RowWithLabel(NSString* label, BOOL selected) {
  id<GREYMatcher> matcher =
      grey_allOf(grey_ancestor(chrome_test_util::PaymentRequestPickerRow()),
                 grey_text(label), grey_kindOfClass([UILabel class]),
                 grey_sufficientlyVisible(), nil);

  if (selected) {
    return grey_allOf(
        matcher,
        grey_ancestor(grey_accessibilityTrait(UIAccessibilityTraitSelected)),
        nil);
  }
  return matcher;
}

// Returns the GREYMatcher for the search bar's cancel button.
id<GREYMatcher> CancelButton() {
  return grey_allOf(grey_accessibilityLabel(@"Cancel"),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_sufficientlyVisible(), nil);
}

// Returns the GREYMatcher for the UIAlertView's message displayed for a call
// that notifies the delegate of a selection.
id<GREYMatcher> UIAlertViewMessageForDelegateCallWithArgument(
    NSString* argument) {
  return grey_allOf(
      grey_text(
          [NSString stringWithFormat:
                        @"paymentRequestPickerViewController:"
                        @"kPaymentRequestPickerViewControllerAccessibilityID "
                        @"didSelectRow:%@",
                        argument]),
      grey_sufficientlyVisible(), nil);
}

}  // namespace

// Tests for the payment request picker view controller.
@interface SCPaymentsPickerTestCase : ShowcaseTestCase
@end

@implementation SCPaymentsPickerTestCase

- (void)setUp {
  [super setUp];
  Open(@"PaymentRequestPickerViewController");
}

- (void)tearDown {
  Close();
  [super tearDown];
}

- (void)scrollToTop {
  // Scroll to the top, starting at the center of the screen.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClass([UITableView class])]
      performAction:grey_scrollToContentEdgeWithStartPoint(kGREYContentEdgeTop,
                                                           0.5f, 0.5f)];
}

- (void)assertSection:(NSString*)label visible:(BOOL)visible {
  [[[EarlGrey selectElementWithMatcher:SectionWithTitle(label)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_kindOfClass([UITableView class])]
      assertWithMatcher:visible ? grey_notNil() : grey_nil()];

  // Return to top, to ensure we are in the same state before every search,
  // as each search action may scroll down, thereby making it impossible for
  // the next search action to work properly, as we only scroll down when
  // searching.
  [self scrollToTop];
}

- (void)assertRow:(NSString*)label
         selected:(BOOL)selected
          visible:(BOOL)visible {
  [[[EarlGrey selectElementWithMatcher:RowWithLabel(label, selected)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_kindOfClass([UITableView class])]
      assertWithMatcher:visible ? grey_notNil() : grey_nil()];

  // Return to top, to ensure we are in the same state before every search,
  // as each search action may scroll down, thereby making it impossible for
  // the next search action to work properly, as we only scroll down when
  // searching.
  [self scrollToTop];
}

// Tests if all the expected rows and sections are present and the expected row
// is selected.
- (void)testVerifyRowsAndSection {
  [self assertSection:@"B" visible:YES];
  [self assertRow:@"Belgium" selected:NO visible:YES];
  [self assertRow:@"Brazil" selected:NO visible:YES];

  [self assertSection:@"C" visible:YES];
  [self assertRow:@"Canada" selected:NO visible:YES];
  [self assertRow:@"Chile" selected:NO visible:YES];
  [self assertRow:@"China" selected:YES visible:YES];

  [self assertSection:@"E" visible:YES];
  [self assertRow:@"España" selected:NO visible:YES];

  [self assertSection:@"M" visible:YES];
  [self assertRow:@"México" selected:NO visible:YES];
}

// Tests if filtering works.
- (void)testVerifyFiltering {
  // TODO(crbug.com/753098): Re-enable this test on iPad once grey_typeText
  // works.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }

  [self scrollToTop];

  // Type 'c' in the search bar.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PaymentRequestPickerSearchBar()]
      performAction:grey_typeText(@"chi")];

  [self assertSection:@"B" visible:NO];
  [self assertRow:@"Belgium" selected:NO visible:NO];
  [self assertRow:@"Brazil" selected:NO visible:NO];

  [self assertSection:@"C" visible:YES];
  [self assertRow:@"Canada" selected:NO visible:NO];
  [self assertRow:@"Chile" selected:NO visible:YES];
  [self assertRow:@"China" selected:YES visible:YES];

  [self assertSection:@"E" visible:NO];
  [self assertRow:@"España" selected:NO visible:NO];

  [self assertSection:@"M" visible:NO];
  [self assertRow:@"México" selected:NO visible:NO];

  // Type 'l' in the search bar. So far we have typed "chil".
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          PaymentRequestPickerSearchBar()]
      performAction:grey_typeText(@"l")];

  [self assertSection:@"B" visible:NO];
  [self assertRow:@"Belgium" selected:NO visible:NO];
  [self assertRow:@"Brazil" selected:NO visible:NO];

  [self assertSection:@"C" visible:YES];
  [self assertRow:@"Canada" selected:NO visible:NO];
  [self assertRow:@"Chile" selected:NO visible:YES];
  [self assertRow:@"China" selected:YES visible:NO];

  [self assertSection:@"E" visible:NO];
  [self assertRow:@"España" selected:NO visible:NO];

  [self assertSection:@"M" visible:NO];
  [self assertRow:@"México" selected:NO visible:NO];

  // Cancel filtering the text in the search bar.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()];

  [self assertSection:@"B" visible:YES];
  [self assertRow:@"Belgium" selected:NO visible:YES];
  [self assertRow:@"Brazil" selected:NO visible:YES];
  [self assertSection:@"C" visible:YES];
  [self assertRow:@"Canada" selected:NO visible:YES];
  [self assertRow:@"Chile" selected:NO visible:YES];
  [self assertRow:@"China" selected:YES visible:YES];
  [self assertSection:@"E" visible:YES];
  [self assertRow:@"España" selected:NO visible:YES];
  [self assertSection:@"M" visible:YES];
  [self assertRow:@"México" selected:NO visible:YES];
}

// Tests that tapping a row should make it the selected row.
- (void)testVerifySelection {
  // 'China' is selected.
  [self assertRow:@"China" selected:YES visible:YES];

  // 'Canada' is not selected. Tap it.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Canada", NO)]
      performAction:grey_tap()];

  // Confirm the delegate is informed.
  [[EarlGrey
      selectElementWithMatcher:UIAlertViewMessageForDelegateCallWithArgument(
                                   @"Label: Canada, Value: CAN")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  // 'China' is not selected anymore.
  [self assertRow:@"China" selected:NO visible:YES];

  // Now 'Canada' is selected. Tap it again.
  [[EarlGrey selectElementWithMatcher:RowWithLabel(@"Canada", YES)]
      performAction:grey_tap()];

  // Confirm the delegate is informed.
  [[EarlGrey
      selectElementWithMatcher:UIAlertViewMessageForDelegateCallWithArgument(
                                   @"Label: Canada, Value: CAN")]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          @"protocol_alerter_done")]
      performAction:grey_tap()];

  // 'China' is still not selected.
  [self assertRow:@"China" selected:NO visible:YES];

  // 'Canada' is still selected.
  [self assertRow:@"Canada" selected:YES visible:YES];
}

@end
