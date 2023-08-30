// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_egtest_utils.h"

#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_constants.h"
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

void AddURLToReadingList(const GURL& URL) {
  // Open the URL.
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForPageToFinishLoading];
  // Add the page to the Reading List.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuAction:chrome_test_util::ButtonWithAccessibilityLabelId(
                             IDS_IOS_SHARE_MENU_READING_LIST_ACTION)];
}

}  // namespace reading_list_test_utils
