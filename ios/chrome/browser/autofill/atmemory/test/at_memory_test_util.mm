// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/test/at_memory_test_util.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

@implementation AtMemoryTestUtil

+ (id<GREYMatcher>)atMemoryButton {
  return grey_accessibilityID(
      kFormInputAccessoryViewAtMemoryButtonAccessibilityID);
}

+ (id<GREYMatcher>)searchBar {
  return grey_allOf(
      grey_accessibilityID(kAtMemorySearchBarAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)closeButton {
  return grey_accessibilityID(kAtMemoryCloseButtonAccessibilityIdentifier);
}

+ (id<GREYMatcher>)searchPromptCell {
  return grey_allOf(grey_kindOfClassName(@"UITableViewCell"),
                    grey_descendant(grey_accessibilityLabel(
                        @"Find and fill this with Gemini")),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)searchPromptCellWithQuery:(NSString*)query {
  NSString* expectedLabel = [NSString
      stringWithFormat:@"%@, %@", query,
                       l10n_util::GetNSString(
                           IDS_AUTOFILL_AT_MEMORY_SEARCH_AFFORDANCE_SUBTITLE)];
  return grey_allOf(
      grey_accessibilityID(kAtMemorySearchCellAccessibilityIdentifier),
      grey_accessibilityLabel(expectedLabel), grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)noDataCell {
  return grey_allOf(
      grey_accessibilityID(kAtMemoryNoDataCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)noConnectionCell {
  return grey_allOf(
      grey_accessibilityID(kAtMemoryNoConnectionCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)emptyStateImage {
  return grey_allOf(grey_kindOfClass([UIImageView class]),
                    grey_ancestor(grey_accessibilityID(
                        kAtMemoryEmptyViewAccessibilityIdentifier)),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)searchResultCellWithSubtitle:(NSString*)subtitle {
  return grey_allOf(grey_kindOfClassName(@"LegacyTableViewCell"),
                    grey_descendant(grey_text(subtitle)),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)infoButtonForSearchResultWithSubtitle:(NSString*)subtitle {
  return grey_allOf(
      grey_kindOfClassName(@"UIButton"),
      grey_ancestor(grey_allOf(grey_kindOfClassName(@"LegacyTableViewCell"),
                               grey_descendant(grey_text(subtitle)), nil)),
      nil);
}

+ (id<GREYMatcher>)chipButtonWithLabel:(NSString*)label {
  return grey_allOf(grey_kindOfClassName(@"UIButton"),
                    grey_accessibilityLabel(label), nil);
}

@end
