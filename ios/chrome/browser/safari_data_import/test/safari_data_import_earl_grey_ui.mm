// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safari_data_import/test/safari_data_import_earl_grey_ui.h"

#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/data_import/public/accessibility_utils.h"
#import "ios/chrome/browser/safari_data_import/public/utils.h"
#import "ios/chrome/browser/safari_data_import/test/safari_data_import_app_interface.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/strings/grit/ui_strings.h"

using chrome_test_util::ButtonStackPrimaryButton;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::StaticTextWithAccessibilityLabelId;

namespace {

/// Visibility of Safari data import entry point. If `verify_visibility`, fail
/// the test if the entry point is not visible.
bool IsSafariDataImportEntryPointVisible(bool verify_visibility) {
  bool visible = [ChromeEarlGrey
      testUIElementAppearanceWithMatcher:
          grey_accessibilityID(
              GetSafariDataEntryPointAccessibilityIdentifier())];
  if (verify_visibility) {
    GREYAssertTrue(visible, @"Safari import landing page is not found.");
  }
  return visible;
}

}  // namespace

void DismissSafariDataImportEntryPoint(bool verify_visibility) {
  if (IsSafariDataImportEntryPointVisible(verify_visibility)) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
        performAction:grey_tap()];
  }
}

bool IsSafariDataImportEntryPointVisible() {
  return IsSafariDataImportEntryPointVisible(/*verify_visibility=*/false);
}

void StartImportOnSafariDataImportEntryPoint() {
  if (IsSafariDataImportEntryPointVisible(true)) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
        performAction:grey_tap()];
  }
}

void SetReminderOnSafariDataImportEntryPoint() {
  if (IsSafariDataImportEntryPointVisible(true)) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::ButtonStackTertiaryButton()]
        performAction:grey_tap()];
  }
}

void GoToImportScreen() {
  StartImportOnSafariDataImportEntryPoint();
  /// Taps "I've exported my data."
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::ButtonStackSecondaryButton(),
                            grey_interactable(), nil)]
      performAction:grey_tap()];
}

id<GREYMatcher> ImportScreenButtonWithTextId(int text_id) {
  return grey_allOf(ButtonStackPrimaryButton(),
                    ButtonWithAccessibilityLabelId(text_id),
                    grey_interactable(), nil);
}

void LoadFile(SafariDataImportTestFile file) {
  std::unique_ptr<EarlGreyScopedBlockSwizzler> url_access_swizzler =
      std::make_unique<EarlGreyScopedBlockSwizzler>(
          @"NSURL", @"startAccessingSecurityScopedResource", ^bool() {
            return YES;
          });
  [[EarlGrey
      selectElementWithMatcher:
          ImportScreenButtonWithTextId(
              IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_SELECT_YOUR_FILE)]
      performAction:grey_tap()];
  /// Give it a few seconds for the file picker to be displayed and the
  /// selection to be done, which unfortunately could not be detected by the
  /// test framework.
  base::test::ios::SpinRunLoopWithMinDelay(
      base::test::ios::kWaitForUIElementTimeout);
  __block BOOL file_selected;
  NSString* error_message = [SafariDataImportAppInterface selectFile:file
                                                          completion:^{
                                                            file_selected =
                                                                true;
                                                          }];
  GREYAssertNil(error_message, error_message);
  GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(
                     base::test::ios::kWaitForActionTimeout,
                     ^{
                       return file_selected;
                     }),
                 @"File selection has failed.");

  id<GREYMatcher> any_activity_indicator =
      grey_allOf(grey_kindOfClassName(@"UIActivityIndicatorView"),
                 grey_ancestor(ButtonStackPrimaryButton()), nil);
  [ChromeEarlGrey
      waitForUIElementToDisappearWithMatcher:any_activity_indicator
                                     timeout:base::test::ios::
                                                 kWaitForActionTimeout];
}

void ExpectImportTableHasRowCount(int expected_count) {
  /// Wait until the table displays.
  BOOL visible = [ChromeEarlGrey
      testUIElementAppearanceWithMatcher:
          grey_accessibilityID(
              GetImportDataItemTableViewAccessibilityIdentifier())];
  GREYAssertEqual(visible, expected_count > 0,
                  visible ? @"Import table is unexpectedly displayed."
                          : @"Import table is unexpectedly hidden.");
  id<GREYMatcher> last_row =
      grey_allOf(grey_accessibilityID(
                     GetImportDataItemTableViewCellAccessibilityIdentifier(
                         expected_count - 1)),
                 grey_sufficientlyVisible(), nil);
  id<GREYMatcher> row_index_out_of_bounds =
      grey_allOf(grey_accessibilityID(
                     GetImportDataItemTableViewCellAccessibilityIdentifier(
                         expected_count)),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:last_row]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:row_index_out_of_bounds]
      assertWithMatcher:grey_nil()];
}

void ExpectPasswordConflictCellAtIndexSelected(int idx, bool selected) {
  id<GREYMatcher> row = grey_accessibilityID(
      GetPasswordConflictResolutionTableViewCellAccessibilityIdentifier(idx));
  [[EarlGrey selectElementWithMatcher:grey_allOf(grey_ancestor(row),
                                                 grey_selected(), nil)]
      assertWithMatcher:selected ? grey_sufficientlyVisible() : grey_nil()];
}

void TapInfoButtonForInvalidPasswords(int imported, int failed) {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_IMPORT_ITEM_TYPE_TITLE_PASSWORDS);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_CONCAT_TWO_STRINGS_WITH_PERIODS,
      l10n_util::GetPluralStringFUTF16(
          IDS_IOS_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_PASSWORDS, imported),
      l10n_util::GetPluralStringFUTF16(
          IDS_IOS_IMPORT_ITEM_TYPE_IMPORTED_DETAILED_TEXT_INVALID_PASSWORDS,
          failed));
  id<GREYMatcher> password_cell = ButtonWithAccessibilityLabel(
      [NSString stringWithFormat:@"%@, %@", title, subtitle]);

  [[EarlGrey selectElementWithMatcher:password_cell]
      assertWithMatcher:grey_sufficientlyVisible()];
  id<GREYMatcher> info_button =
      grey_allOf(grey_ancestor(password_cell),
                 grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
  [[EarlGrey selectElementWithMatcher:info_button] performAction:grey_tap()];
}

void CompletesImportWorkflow() {
  [[EarlGrey selectElementWithMatcher:
                 ImportScreenButtonWithTextId(
                     IDS_IOS_SAFARI_IMPORT_IMPORT_ACTION_BUTTON_DONE)]
      performAction:grey_tap()];
  /// Handles "Delete file" dialog. Keep the file for other test cases.
  id<GREYMatcher> delete_button = grey_allOf(
      StaticTextWithAccessibilityLabelId(IDS_CANCEL), grey_interactable(), nil);
  [[EarlGrey selectElementWithMatcher:delete_button] performAction:grey_tap()];
}
