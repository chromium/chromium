// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_egtest_utils.h"

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace reading_list_test_utils {

id<GREYMatcher> AddedToLocalReadingListSnackbar() {
  NSString* snackbarMessage =
      l10n_util::GetNSString(IDS_IOS_READING_LIST_SNACKBAR_MESSAGE);
  return grey_allOf(
      grey_accessibilityID(@"MDCSnackbarMessageTitleAutomationIdentifier"),
      grey_text(snackbarMessage), nil);
}

id<GREYMatcher> ReadingListItem(NSString* entryTitle) {
  return grey_allOf(grey_accessibilityID(entryTitle),
                    grey_kindOfClassName(@"TableViewURLCell"), nil);
}

id<GREYMatcher> VisibleReadingListItem(NSString* entryTitle) {
  return grey_allOf(grey_accessibilityID(entryTitle),
                    grey_kindOfClassName(@"TableViewURLCell"),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> VisibleLocalItemIcon(NSString* title) {
  return grey_allOf(grey_ancestor(ReadingListItem(title)),
                    grey_accessibilityID(kTableViewURLCellMetadataImageID),
                    grey_sufficientlyVisible(), nil);
}

// Opens the reading list menu.
void OpenReadingList() {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::ReadingListDestinationButton()];
  // It seems that sometimes there is a delay before the ReadingList is
  // displayed. See https://crbug.com/1109202 .
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                                          kReadingListViewID)];
}

void AddURLToReadingListWithoutSnackbarDismiss(const GURL& URL) {
  // Open the URL.
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForPageToFinishLoading];
  // Add the page to the Reading List.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:chrome_test_util::ButtonWithAccessibilityLabelId(
                             IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)];
}

void AddURLToReadingListWithSnackbarDismiss(const GURL& URL, NSString* email) {
  AddURLToReadingListWithoutSnackbarDismiss(URL);
  id<GREYMatcher> matcher = nil;
  if (email) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        IDS_IOS_READING_LIST_SNACKBAR_MESSAGE_FOR_ACCOUNT);
    std::u16string utf16Text =
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            pattern, "count", 1, "email", base::SysNSStringToUTF16(email));
    NSString* snackbarMessage = base::SysUTF16ToNSString(utf16Text);
    matcher = grey_allOf(
        grey_accessibilityID(@"MDCSnackbarMessageTitleAutomationIdentifier"),
        grey_text(snackbarMessage), nil);
  } else {
    matcher = reading_list_test_utils::AddedToLocalReadingListSnackbar();
  }
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

}  // namespace reading_list_test_utils
