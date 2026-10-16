// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/test/at_memory_test_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Link tag delimiters in the inline privacy notice text.
NSString* const kLinkBeginTag = @"<link>";
NSString* const kLinkEndTag = @"</link>";

// Accessibility label and class name for the collapsed search bar button in the
// floating bar on iPhone Duo.
NSString* const kSearchButtonAccessibilityLabel = @"Search";
NSString* const kUIButtonBarButtonClassName = @"_UIButtonBarButton";

// Line-height multiplier threshold used to determine if a link wraps across
// multiple lines.
const CGFloat kMultilineHeightMultiplier = 1.5;

}  // namespace

@implementation AtMemoryTestUtil

+ (id<GREYMatcher>)atMemoryButton {
  return grey_accessibilityID(
      kFormInputAccessoryViewAtMemoryButtonAccessibilityID);
}

+ (id<GREYMatcher>)bottomSheetTitle {
  return grey_allOf(grey_text(l10n_util::GetNSString(
                        IDS_IOS_AUTOFILL_AI_FIND_AND_FILL_TITLE)),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)searchBar {
  return grey_allOf(
      grey_accessibilityID(kAtMemorySearchBarAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)searchButton {
  return grey_allOf(
      grey_anyOf(
          grey_accessibilityID(kAtMemorySearchBarAccessibilityIdentifier),
          grey_allOf(grey_accessibilityLabel(kSearchButtonAccessibilityLabel),
                     grey_kindOfClassName(kUIButtonBarButtonClassName), nil),
          nil),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)suggestionStripAtMemoryButton {
  NSString* accessibilityLabel = l10n_util::GetNSStringF(
      IDS_IOS_AUTOFILL_ACCNAME_SUGGESTION,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AT_MEMORY_SEARCH_AFFORDANCE_TITLE),
      u"");
  return grey_allOf(grey_accessibilityLabel(accessibilityLabel),
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

+ (id<GREYMatcher>)searchResultCellWithTitle:(NSString*)title {
  NSString* accessibilityID = [NSString
      stringWithFormat:@"%@%@",
                       kAtMemorySearchResultCellAccessibilityIdentifierPrefix,
                       title];
  return grey_allOf(grey_accessibilityID(accessibilityID),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)infoButtonForSearchResultWithTitle:(NSString*)title {
  NSString* accessibilityID = [NSString
      stringWithFormat:
          @"%@%@", kAtMemorySearchResultInfoButtonAccessibilityIdentifierPrefix,
          title];
  return grey_allOf(grey_accessibilityID(accessibilityID),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)chipButtonWithLabel:(NSString*)label {
  return grey_allOf(
      chrome_test_util::ButtonWithAccessibilityLabel(l10n_util::GetNSStringF(
          IDS_IOS_MANUAL_FALLBACK_CHIP_ACCESSIBILITY_LABEL,
          base::SysNSStringToUTF16(label))),
      grey_interactable(), nil);
}

+ (id<GREYMatcher>)manageSavedInfoCell {
  return grey_allOf(
      grey_accessibilityID(
          kAtMemoryManageEnhancedAutofillItemAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)emptyView {
  return grey_allOf(
      grey_accessibilityID(kAtMemoryEmptyViewAccessibilityIdentifier),
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_AUTOFILL_AT_MEMORY_ZERO_STATE_SUBTITLE)),
      grey_descendant(grey_accessibilityID(@"at_memory_empty")),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)inlineNoticeTitle {
  return grey_allOf(
      grey_text(l10n_util::GetNSString(IDS_AT_MEMORY_NOTICE_TITLE)),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)inlineNoticeOKButton {
  return grey_allOf(grey_buttonTitle(l10n_util::GetNSString(IDS_OK)),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)aiDisclosureFooter {
  NSString* disclosureText =
      ParseStringWithLinks(
          l10n_util::GetNSString(IDS_IOS_AT_MEMORY_AI_DISCLOSURE))
          .string;
  return grey_allOf(grey_text(disclosureText), grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)aiDisclosureFooterLink {
  StringWithTags parsed = ParseStringWithLinks(
      l10n_util::GetNSString(IDS_IOS_AT_MEMORY_AI_DISCLOSURE));
  NSString* linkText = [parsed.string substringWithRange:parsed.ranges[0]];
  return grey_allOf(grey_accessibilityLabel(linkText),
                    grey_accessibilityTrait(UIAccessibilityTraitLink),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)isMultilineLink {
  GREYMatchesBlock matches = ^BOOL(id element) {
    CGFloat lineHeight =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote].lineHeight;
    return [element accessibilityFrame].size.height >
           lineHeight * kMultilineHeightMultiplier;
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:@"is a multiline link"];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

+ (id<GREYMatcher>)inlineNoticeSettingsLink {
  StringWithTag parsed =
      ParseStringWithTag(l10n_util::GetNSString(IDS_AT_MEMORY_NOTICE_TEXT),
                         kLinkBeginTag, kLinkEndTag);
  NSString* linkText = [parsed.string substringWithRange:parsed.range];
  return grey_allOf(grey_accessibilityLabel(linkText),
                    grey_accessibilityTrait(UIAccessibilityTraitLink),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)helpImproveSettingsPage {
  return grey_allOf(
      grey_text(l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE)),
      grey_sufficientlyVisible(), nil);
}

@end
