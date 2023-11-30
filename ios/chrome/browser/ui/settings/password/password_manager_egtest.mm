// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <TargetConditionals.h>

#import <utility>

#import "base/functional/callback.h"
#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/password_manager/core/common/password_manager_constants.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/policy/policy_constants.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_prefs.h"
#import "ios/chrome/browser/credential_provider_promo/model/features.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_app_interface.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_constants.h"
#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/earl_grey_scoped_block_swizzler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ios/web/public/test/element_selector.h"
#import "ui/base/l10n/l10n_util.h"

#import "ios/third_party/earl_grey2/src/CommonLib/Matcher/GREYLayoutConstraint.h"  // nogncheck

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::PasswordsTableViewMatcher;
using chrome_test_util::SettingsCollectionView;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsNavigationBar;
using chrome_test_util::TabGridEditButton;
using chrome_test_util::TextFieldForCellWithLabelId;
using chrome_test_util::TurnTableViewSwitchOn;
using password_manager::kPasswordManagerSurfaceVisitHistogramName;
using password_manager_test_utils::DeleteButtonForUsernameAndSites;
using password_manager_test_utils::DeleteCredential;
using password_manager_test_utils::EditDoneButton;
using password_manager_test_utils::EditPasswordConfirmationButton;
using password_manager_test_utils::GetInteractionForPasswordIssueEntry;
using password_manager_test_utils::kDefaultPassword;
using password_manager_test_utils::kDefaultSite;
using password_manager_test_utils::kDefaultUsername;
using password_manager_test_utils::kPasswordStoreErrorMessage;
using password_manager_test_utils::kScrollAmount;
using password_manager_test_utils::NavigationBarEditButton;
using password_manager_test_utils::OpenPasswordManager;
using password_manager_test_utils::PasswordDetailPassword;
using password_manager_test_utils::PasswordSettingsTableView;
using password_manager_test_utils::PasswordTextfieldForUsernameAndSites;
using password_manager_test_utils::ReauthenticationController;
using password_manager_test_utils::SavePasswordForm;
using password_manager_test_utils::TapNavigationBarEditButton;
using password_manager_test_utils::UsernameTextfieldForUsernameAndSites;
using testing::ElementWithAccessibilityLabelSubstring;

namespace {

constexpr base::TimeDelta kSyncInitializedTimeout = base::Seconds(5);

id<GREYMatcher> ButtonWithAccessibilityID(NSString* id) {
  return grey_allOf(grey_accessibilityID(id),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

NSString* GetTextFieldForID(int category_id) {
  return [NSString
      stringWithFormat:@"%@_textField", l10n_util::GetNSString(category_id)];
}

// Returns the GREYElementInteraction* for the item on the password list with
// the given `matcher`. It scrolls in `direction` if necessary to ensure that
// the matched item is sufficiently visible, thus interactable.
GREYElementInteraction* GetInteractionForListItem(id<GREYMatcher> matcher,
                                                  GREYDirection direction) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_sufficientlyVisible(),
                                          nil)]
         usingSearchAction:grey_scrollInDirection(direction, kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)];
}

// Returns the GREYElementInteraction* for the cell on the password list with
// the given `username`. It scrolls down if necessary to ensure that the matched
// cell is interactable.
GREYElementInteraction* GetInteractionForPasswordEntry(NSString* username) {
  // ID, not label because the latter might contain an extra label for the
  // "local password icon" and most tests don't care about it.
  return GetInteractionForListItem(ButtonWithAccessibilityID(username),
                                   kGREYDirectionDown);
}

// Returns the GREYElementInteraction* for the item on the detail view
// identified with the given `matcher`. It scrolls down if necessary to ensure
// that the matched cell is interactable.
GREYElementInteraction* GetInteractionForPasswordDetailItem(
    id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
         usingSearchAction:grey_scrollToContentEdge(kGREYContentEdgeTop)
      onElementWithMatcher:grey_accessibilityID(kPasswordDetailsTableViewID)];
}

// Returns the GREYElementInteraction* for the item on the deletion alert
// identified with the given `matcher`.
GREYElementInteraction* GetInteractionForPasswordsExportConfirmAlert(
    id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
      inRoot:grey_accessibilityID(kPasswordSettingsExportConfirmViewId)];
}

GREYElementInteraction* GetPasswordDetailTextFieldWithID(int detail_id) {
  return GetInteractionForPasswordDetailItem(
      grey_allOf(grey_accessibilityID(GetTextFieldForID(detail_id)),
                 grey_kindOfClassName(@"UITextField"), nil));
}

// Matcher for "Save in Account" confirmation dialog button for bulk save
// passwords in account flow.
GREYElementInteraction* SaveInAccountConfirmationDialogButton() {
  return [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                         IDS_IOS_BULK_UPLOAD_BUTTON_TITLE),
                     grey_interactable(), nil)]
      inRoot:grey_accessibilityID(
                 kPasswordSettingsBulkMovePasswordsToAccountAlertViewId)];
}

// Matcher for "Saved Passwords" header in the password list.
id<GREYMatcher> SavedPasswordsHeaderMatcher() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_SAVED_HEADING)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

// Matcher for a UITextField inside a SettingsSearchCell.
id<GREYMatcher> SearchTextField() {
  return grey_accessibilityID(kPasswordsSearchBarID);
}

GREYLayoutConstraint* Below() {
  return [GREYLayoutConstraint
      layoutConstraintWithAttribute:kGREYLayoutAttributeTop
                          relatedBy:kGREYLayoutRelationGreaterThanOrEqual
               toReferenceAttribute:kGREYLayoutAttributeBottom
                         multiplier:1.0
                           constant:0.0];
}

// Matcher for the website in the Add Password view.
id<GREYMatcher> AddPasswordWebsite() {
  return TextFieldForCellWithLabelId(IDS_IOS_SHOW_PASSWORD_VIEW_SITE);
}

// Matcher for the username in Password Details view.
id<GREYMatcher> PasswordDetailUsername() {
  return TextFieldForCellWithLabelId(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
}

// Matcher for the note in Password Details view.
id<GREYMatcher> PasswordDetailNote() {
  return grey_allOf(
      grey_accessibilityID(GetTextFieldForID(IDS_IOS_SHOW_PASSWORD_VIEW_NOTE)),
      grey_kindOfClassName(@"UITextView"), nil);
}

// Matcher for the federation details in Password Details view.
id<GREYMatcher> PasswordDetailFederation() {
  return grey_allOf(grey_accessibilityID(GetTextFieldForID(
                        IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION)),
                    grey_kindOfClassName(@"UITextField"), nil);
}

// Matcher for the Show password button in Password Details view.
id<GREYMatcher> ShowPasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON)),
                    grey_interactable(), nullptr);
}

// Matcher for the Hide password button in Password Details view.
id<GREYMatcher> HidePasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON)),
                    grey_interactable(), nullptr);
}

// Matcher for the Delete button in Password Details view.
id<GREYMatcher> DeleteButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_SETTINGS_TOOLBAR_DELETE),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)),
      nullptr);
}

// Matcher for the Delete button in Confirmation Alert for password
// deletion.
id<GREYMatcher> BatchDeleteConfirmationButton() {
  return chrome_test_util::AlertAction(
      l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE));
}

// Matcher for the Delete button in the list view, located at the bottom of the
// screen.
id<GREYMatcher> DeleteButtonAtBottom() {
  return grey_accessibilityID(kSettingsToolbarDeleteButtonId);
}

// Matcher for the "Delete" associated with the blocked site.
id<GREYMatcher> DeleteBlockedSiteButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(
                        l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)),
                    grey_interactable(), nullptr);
}

// Matcher for the "View Password" Button presented when a duplicated credential
// is found in the add credential flow.
id<GREYMatcher> DuplicateCredentialViewPasswordButton() {
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_PASSWORD_SETTINGS_VIEW_PASSWORD_BUTTON)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    nullptr);
}

// Matches the pop-up (call-out) menu item with accessibility label equal to the
// translated string identified by `label`.
id<GREYMatcher> PopUpMenuItemWithLabel(int label) {
  // iOS13 reworked menu button subviews to no longer be accessibility
  // elements.  Multiple menu button subviews no longer show up as potential
  // matches, which means the matcher logic does not need to be as complex as
  // the iOS 11/12 logic.  Various table view cells may share the same
  // accesibility label, but those can be filtered out by ignoring
  // UIAccessibilityTraitButton.
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(label)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitButton)),
      grey_userInteractionEnabled(), nil);
}

// Returns matcher for the "Add Password" button.
id<GREYMatcher> AddPasswordButton() {
  return grey_accessibilityID(kAddPasswordButtonID);
}

// Returns matcher for the "Add Password" toolbar button located at the bottom
// of the screen.
id<GREYMatcher> AddPasswordToolbarButton() {
  return grey_accessibilityID(kSettingsToolbarAddButtonId);
}

// Returns matcher for the "Save" button in the "Add Password" view.
id<GREYMatcher> AddPasswordSaveButton() {
  return grey_accessibilityID(kPasswordsAddPasswordSaveButtonID);
}

id<GREYMatcher> ToolbarSettingsSubmenuButton() {
  return grey_accessibilityID(kSettingsToolbarSettingsButtonId);
}

// Returns matcher for the password details / add password view footer displayed
// when the note length is approaching max limit.
id<GREYMatcher> TooLongNoteFooter() {
  return grey_text(l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_PASSWORDS_TOO_LONG_NOTE_DESCRIPTION,
      base::NumberToString16(
          password_manager::constants::kMaxPasswordNoteLength)));
}

// Returns matcher for the Password Manager widget promo.
id<GREYMatcher> PasswordManagerWidgetPromo() {
  return grey_accessibilityID(kWidgetPromoID);
}

// Returns matcher for the Password Manager widget promo's close button.
id<GREYMatcher> PasswordManagerWidgetPromoCloseButton() {
  return grey_accessibilityID(kWidgetPromoCloseButtonID);
}

// Returns matcher for the Password Manager widget promo's more info button.
id<GREYMatcher> PasswordManagerWidgetPromoMoreInfoButton(bool enabled = YES) {
  if (enabled) {
    return grey_allOf(
        ButtonWithAccessibilityLabelId(
            IDS_IOS_PASSWORD_MANAGER_WIDGET_PROMO_BUTTON_TITLE),
        grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)),
        nullptr);
  }

  return grey_allOf(ButtonWithAccessibilityLabelId(
                        IDS_IOS_PASSWORD_MANAGER_WIDGET_PROMO_BUTTON_TITLE),
                    grey_accessibilityTrait(UIAccessibilityTraitNotEnabled),
                    nullptr);
}

// Returns matcher for the Password Manager widget promo instructions screen.
id<GREYMatcher> PasswordManagerWidgetPromoInstructions() {
  return grey_accessibilityID(password_manager::kWidgetPromoInstructionsViewID);
}

// Returns matcher for the close button of the Password Manager widget promo
// instruction screen.
id<GREYMatcher> PasswordManagerWidgetPromoInstructionsCloseButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel(l10n_util::GetNSString(IDS_CLOSE)),
      grey_interactable(), nullptr);
}

// Returns matcher for the Password Details move to account button.
id<GREYMatcher> PasswordDetailsMoveToAccountButton() {
  return grey_accessibilityID(kMovePasswordToAccountButtonID);
}

// Saves two example forms in the store.
void SaveExamplePasswordForms() {
  SavePasswordForm(/*password=*/@"password1",
                   /*username=*/@"user1",
                   /*origin=*/@"https://example11.com");
  SavePasswordForm(/*password=*/@"password2",
                   /*username=*/@"user2",
                   /*origin=*/@"https://example12.com");
}

// Saves an example form with note in the store.
void SaveExamplePasswordFormWithNote() {
  GREYAssert(
      [PasswordSettingsAppInterface saveExampleNote:@"concrete note"
                                           password:kDefaultPassword
                                           username:kDefaultUsername
                                             origin:@"https://example.com"],
      kPasswordStoreErrorMessage);
}

// Saves two example blocked forms in the store.
void SaveExampleBlockedForms() {
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://exclude1.com"],
             kPasswordStoreErrorMessage);
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://exclude2.com"],
             kPasswordStoreErrorMessage);
}

// Taps on the "Settings" option to show the submenu.
void OpenSettingsSubmenu() {
  [[EarlGrey selectElementWithMatcher:ToolbarSettingsSubmenuButton()]
      performAction:grey_tap()];
}

void CopyPasswordDetailWithInteraction(GREYElementInteraction* element) {
  [element performAction:grey_tap()];

  // Tap the context menu item for copying.
  [[EarlGrey selectElementWithMatcher:PopUpMenuItemWithLabel(
                                          IDS_IOS_SETTINGS_SITE_COPY_MENU_ITEM)]
      performAction:grey_tap()];
}

void CopyPasswordDetailWithID(int detail_id) {
  CopyPasswordDetailWithInteraction(
      GetPasswordDetailTextFieldWithID(detail_id));
}

// Ensure the save passwords in account section is visible.
void CheckSavePasswordsInAccountSectionVisible() {
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountDescriptionTableViewId)];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId)];
}

// Ensure the save passwords in account section is no longer visible.
void CheckSavePasswordsInAccountSectionHidden() {
  [ChromeEarlGrey
      waitForNotSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountDescriptionTableViewId)];

  [ChromeEarlGrey
      waitForNotSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId)];
}

// Verifies reauthentication UI histogram was recorded.
void CheckReauthenticationUIEventMetric(ReauthenticationEvent event) {
  NSString* histogram = base::SysUTF8ToNSString(
      password_manager::kReauthenticationUIEventHistogram);

  NSError* error = [MetricsAppInterface expectCount:1
                                          forBucket:static_cast<int>(event)
                                       forHistogram:histogram];

  GREYAssertNil(error,
                @"Failed to record reauthentication ui event histogram.");
}

// Verifies the total count of reauthentication UI histogram recorded.
void CheckReauthenticationUIEventMetricTotalCount(int count) {
  NSString* histogram = base::SysUTF8ToNSString(
      password_manager::kReauthenticationUIEventHistogram);

  NSError* error = [MetricsAppInterface expectTotalCount:count
                                            forHistogram:histogram];
  GREYAssertNil(error,
                @"Unexpected reauthentication ui event histogram count.");
}

// Verifies the total count of password manager visit histogram recorded.
void CheckPasswordManagerVisitMetricCount(int count) {
  // Check password manager visit metric.
  NSError* error = [MetricsAppInterface
      expectTotalCount:count
          forHistogram:@(kPasswordManagerSurfaceVisitHistogramName)];
  GREYAssertNil(error, @"Unexpected Password Manager Visit histogram count");

  error = [MetricsAppInterface
       expectCount:count
         forBucket:static_cast<int>(
                       password_manager::PasswordManagerSurface::kPasswordList)
      forHistogram:@(kPasswordManagerSurfaceVisitHistogramName)];
  GREYAssertNil(error, @"Unexpected Password Manager Visit histogram count");
}

// Verifies that the elements of the Password Manager widget promo are as
// expected.
void CheckPasswordManagerWidgetPromoVisible() {
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromo()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromoCloseButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  id<GREYMatcher> image_matcher = grey_accessibilityID(kWidgetPromoImageID);
  [[EarlGrey selectElementWithMatcher:image_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  id<GREYMatcher> text_matcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER_WIDGET_PROMO_TEXT));
  [[EarlGrey selectElementWithMatcher:text_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:PasswordManagerWidgetPromoMoreInfoButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies that the elements of the Password Manager widget promo instruction
// screen are as expected.
void CheckPasswordManagerWidgetPromoInstructionScreenVisible(
    bool image_hidden = false) {
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromoInstructions()]
      assertWithMatcher:grey_sufficientlyVisible()];

  id<GREYMatcher> image_matcher =
      grey_accessibilityID(password_manager::kWidgetPromoInstructionsImageID);
  [[EarlGrey selectElementWithMatcher:image_matcher]
      assertWithMatcher:image_hidden ? grey_notVisible()
                                     : grey_minimumVisiblePercent(0.4)];

  id<GREYMatcher> title_matcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_TITLE));
  [[EarlGrey selectElementWithMatcher:title_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  id<GREYMatcher> subtitle_matcher = grey_text(
      l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_SUBTITLE));
  [[EarlGrey selectElementWithMatcher:subtitle_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  id<GREYMatcher> step1_matcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_STEP_1));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(step1_matcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:
          grey_accessibilityID(
              password_manager::kWidgetPromoInstructionsScrollableViewID)];

  id<GREYMatcher> step2_matcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_STEP_2));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(step2_matcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:
          grey_accessibilityID(
              password_manager::kWidgetPromoInstructionsScrollableViewID)];

  id<GREYMatcher> step3_matcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_STEP_3));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(step3_matcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:
          grey_accessibilityID(
              password_manager::kWidgetPromoInstructionsScrollableViewID)];

  id<GREYMatcher> step4_matcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_STEP_4));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(step4_matcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:
          grey_accessibilityID(
              password_manager::kWidgetPromoInstructionsScrollableViewID)];

  [[EarlGrey selectElementWithMatcher:
                 PasswordManagerWidgetPromoInstructionsCloseButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

void CheckVisibilityOfElement(id<GREYMatcher> matcher, bool is_visible) {
  [[EarlGrey selectElementWithMatcher:matcher]
      assertWithMatcher:is_visible ? grey_sufficientlyVisible()
                                   : grey_notVisible()];
}

// Opens the instructions for enabling the Password Manager Widget.
void OpenPasswordManagerWidgetPromoInstructions() {
  OpenPasswordManager();

  // The Password Manager widget promo should be visible.
  CheckPasswordManagerWidgetPromoVisible();

  // Tap the promo's more info button.
  [[EarlGrey
      selectElementWithMatcher:PasswordManagerWidgetPromoMoreInfoButton()]
      performAction:grey_tap()];

  // The Password Manager widget promo's instructions should be visible.
  CheckPasswordManagerWidgetPromoInstructionScreenVisible();
}

}  // namespace

// Various tests for the main Password Manager UI.
@interface PasswordManagerTestCase : ChromeTestCase

- (GREYElementInteraction*)interactionForSinglePasswordEntryWithDomain:
    (NSString*)domain;

// Matcher for the websites in Password Details view.
// `websites` should be in the format "website1, website2,..." with `websiteN`
// being the website displayed in the nth detail row of the website cell.
- (id<GREYMatcher>)matcherForPasswordDetailCellWithWebsites:(NSString*)websites;

// Matcher for the delete button for a given username/password in the details
// screen.
- (id<GREYMatcher>)
    matcherForDeleteButtonInDetailsWithUsername:(NSString*)username
                                          sites:(NSString*)password;

@end

@implementation PasswordManagerTestCase {
  // A swizzler to observe fake auto-fill status instead of real one.
  std::unique_ptr<EarlGreyScopedBlockSwizzler> _passwordAutoFillStatusSwizzler;
}

- (GREYElementInteraction*)interactionForSinglePasswordEntryWithDomain:
    (NSString*)domain {
  // Since passwords notes launch authentication is required before interacting
  // with password details.
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // ID, not label because the latter might contain an extra label for the
  // "local password icon" and most tests don't care about it.
  return GetInteractionForListItem(ButtonWithAccessibilityID(domain),
                                   kGREYDirectionDown);
}

- (id<GREYMatcher>)matcherForPasswordDetailCellWithWebsites:
    (NSString*)websites {
  return grey_accessibilityLabel(
      [NSString stringWithFormat:@"Sites, %@", websites]);
}

- (id<GREYMatcher>)
    matcherForDeleteButtonInDetailsWithUsername:(NSString*)username
                                          sites:(NSString*)sites {
  return DeleteButtonForUsernameAndSites(username, sites);
}

- (void)setUp {
  [super setUp];
  // Manually clear sync passwords pref before testShowAccountStorageNotice*.
  // TODO(crbug.com/1069086): Wipe the PrefService between tests.
  [ChromeEarlGreyAppInterface
      clearUserPrefWithName:base::SysUTF8ToNSString(
                                syncer::SyncPrefs::GetPrefNameForTypeForTesting(
                                    syncer::UserSelectableType::kPasswords))];
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  _passwordAutoFillStatusSwizzler =
      std::make_unique<EarlGreyScopedBlockSwizzler>(
          @"PasswordAutoFillStatusManager", @"sharedManager",
          [PasswordsInOtherAppsAppInterface
              swizzlePasswordAutoFillStatusManagerWithFake]);

  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
}

- (void)tearDown {
  // Snackbars triggered by tests stay up for a limited time even if the
  // settings get closed. Ensure that they are closed to avoid interference with
  // other tests.
  [PasswordSettingsAppInterface dismissSnackBar];
  GREYAssert([PasswordSettingsAppInterface clearPasswordStore],
             @"PasswordStore was not cleared.");

  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");

  [PasswordsInOtherAppsAppInterface resetManager];
  _passwordAutoFillStatusSwizzler.reset();

  [PasswordSettingsAppInterface removeMockReauthenticationModule];

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  // See (crbug.com/1479593). Enforcing relaunch between tests is necessary to
  // prevent flakiness, due to a spinner that appears in some tests and blocks
  // later ones from interacting with the UI.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  // TODO(crbug.com/1448574): Re-enable CPE promo and update
  // testCopyPasswordToast and testCopyPasswordMenuItem to check for the promo.
  config.features_disabled.push_back(kCredentialProviderExtensionPromo);

  if ([self isRunningTest:@selector
            (testAccountStorageSwitchDisabledByPolicy_SyncToSigninDisabled)] ||
      [self isRunningTest:@selector
            (testAccountStorageSwitchShownIfSignedIn_SyncToSigninDisabled)] ||
      [self isRunningTest:@selector(testAccountStorageSwitchHiddenIfSyncing)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  if ([self isRunningTest:@selector
            (testAccountStorageSwitchHiddenIfSignedIn_SyncToSigninEnabled)] ||
      [self isRunningTest:@selector
            (testMovePasswordToAccountStoreIfSignedIn_SyncToSigninEnabled)]) {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }

  if ([self
          isRunningTest:@selector(testOpenPasswordManagerWithSuccessfulAuth)] ||
      [self isRunningTest:@selector(testOpenPasswordManagerWithFailedAuth)] ||
      [self isRunningTest:@selector
            (testOpenPasswordManagerWithWithoutPasscodeSet)] ||
      [self isRunningTest:@selector
            (testOpenPasswordSettingsSubmenuWithFailedAuth)] ||
      [self isRunningTest:@selector(testAddNewPasswordWithFailedAuth)]) {
    config.features_enabled.push_back(
        password_manager::features::kIOSPasswordAuthOnEntry);
    config.features_enabled.push_back(
        password_manager::features::kIOSPasswordAuthOnEntryV2);
  }

  if ([self isRunningTest:@selector
            (testPasswordManagerVisitMetricWithoutAuthRequired)]) {
    config.features_disabled.push_back(
        password_manager::features::kIOSPasswordAuthOnEntry);
    config.features_disabled.push_back(
        password_manager::features::kIOSPasswordAuthOnEntryV2);
  }

  if ([self isRunningTest:@selector
            (testSavePasswordsInAccountHiddenWhenNotSignedIn)] ||
      [self isRunningTest:@selector
            (testSavePasswordsInAccountShownWhenEligible)] ||
      [self isRunningTest:@selector
            (testSavePasswordsInAccountOneDistinctDomain)] ||
      [self isRunningTest:@selector
            (testSavePasswordsInAccountTwoDistinctDomains)] ||
      [self isRunningTest:@selector
            (testSavePasswordsInAccountThreeDistinctDomains)] ||
      [self isRunningTest:@selector
            (testSavePasswordsInAccountFourDistinctDomains)]) {
    config.features_enabled.push_back(
        password_manager::features::
            kIOSPasswordSettingsBulkUploadLocalPasswords);
  }

  if ([self isRunningTest:@selector(testSavePasswordsInAccountFlowCompletes)] ||
      [self
          isRunningTest:@selector(testSavePasswordsInAccountFlowAuthFailed)] ||
      [self isRunningTest:@selector
            (testSavePasswordsInAccountFlowNoAuthSetOnDevice)] ||
      [self isRunningTest:@selector
            (testSavePasswordsInAccountFlowCompletesMovingPasswords)]) {
    config.features_enabled.push_back(
        password_manager::features::
            kIOSPasswordSettingsBulkUploadLocalPasswords);
    config.features_disabled.push_back(
        password_manager::features::kIOSPasswordAuthOnEntry);
    config.features_disabled.push_back(
        password_manager::features::kIOSPasswordAuthOnEntryV2);
  }

  if ([self isRunningTest:@selector
            (testSavePasswordsInAccountHiddenWhenSyncing)] ||
      [self isRunningTest:@selector
            (testSavePasswordsInAccountHiddenWhenNotOptedInToAccountStorage)]) {
    config.features_enabled.push_back(
        password_manager::features::
            kIOSPasswordSettingsBulkUploadLocalPasswords);
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }

  if ([self isRunningTest:@selector(testClosingPasswordManagerWidgetPromo)] ||
      [self isRunningTest:@selector
            (testOpeningPasswordManagerWidgetPromoInstructions)] ||
      [self
          isRunningTest:@selector(testPasswordManagerWidgetPromoInEditMode)] ||
      [self isRunningTest:@selector
            (testPasswordManagerWidgetPromoDeviceOrientation)] ||
      [self isRunningTest:@selector
            (testDismissPasswordManagerWidgetPromoInstructionsScreen)] ||
      [self isRunningTest:@selector
            (testPasswordManagerWidgetPromoInstructionsDeviceOrientation)] ||
      [self
          isRunningTest:@selector
          (testOpeningPasswordManagerWidgetPromoInstructionsWithFailedAuth)]) {
    config.additional_args.push_back(base::StringPrintf(
        "--enable-features=%s:chosen_feature/"
        "IPH_iOSPromoPasswordManagerWidget,%s",
        feature_engagement::kIPHDemoMode.name,
        password_manager::features::kIOSPasswordAuthOnEntryV2.name));
  }

  return config;
}

// Verifies the UI elements are accessible on the Passwords page.
- (void)testAccessibilityOnPasswords {
  // Saving a form is needed for using the "password details" view.
  SavePasswordForm();

  OpenPasswordManager();
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  TapNavigationBarEditButton();
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  // Inspect "password details" view.
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempt to copy a password provides appropriate feedback.
- (void)testCopyPasswordToast {
  // Saving a form is needed for using the "password details" view.
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  CopyPasswordDetailWithID(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD);

  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that an attempt to show a password provides an appropriate feedback.
- (void)testShowPasswordSucceeded {
  // Saving a form is needed for using the "password details" view.
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];

  // Ensure that password is shown.
  [GetInteractionForPasswordDetailItem(grey_textFieldValue(kDefaultPassword))
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to copy a username provide appropriate feedback.
- (void)testCopyUsernameToast {
  // Saving a form is needed for using the "password details" view.
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  CopyPasswordDetailWithID(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME);
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to copy a site URL provide appropriate feedback.
- (void)testCopySiteToast {
  // Saving a form is needed for using the "password details" view.
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  CopyPasswordDetailWithInteraction(GetInteractionForPasswordDetailItem(
      [self matcherForPasswordDetailCellWithWebsites:kDefaultSite]));

  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITES_WERE_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a saved password from password details view goes back
// to the list-of-passwords view which doesn't display that form anymore.
- (void)testSavedFormDeletionInDetailView {
  // Save form to be deleted later.
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  DeleteCredential(kDefaultUsername, kDefaultSite);

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating
  // PasswordTableViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Add button is visible and enabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the detail view is dismissed when the last password is deleted
// after the user had edited the password.
- (void)testSavedFormDeletionInDetailViewAfterEditingFields {
  // Save form to be deleted later.
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  TapNavigationBarEditButton();

  // Edit password field.
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(kDefaultPassword)];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"")];

  // Delete password.
  DeleteCredential(kDefaultUsername, kDefaultSite);

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating
  // PasswordTableViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Add button is visible and enabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a saved password from password details view goes back
// to the list-of-passwords showing only previously saved blocked sites.
- (void)testSavedFormDeletionInDetailViewWithBlockedSites {
  // Save form to be deleted later.
  SavePasswordForm();

  // Saved blocked sites that should not be affected.
  SaveExampleBlockedForms();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  DeleteCredential(kDefaultUsername, kDefaultSite);

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating
  // PasswordTableViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(2, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify blocked sites are still there.
  [GetInteractionForPasswordEntry(@"exclude1.com")
      assertWithMatcher:grey_sufficientlyVisible()];
  [GetInteractionForPasswordEntry(@"exclude2.com")
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a duplicated saved password from password details view
// goes back to the list-of-passwords view which doesn't display that form
// anymore.
- (void)testDuplicatedSavedFormDeletionInDetailView {
  // Save form to be deleted later.
  SavePasswordForm();
  // Save duplicate of the previously saved form to be deleted at the same time.
  // This entry is considered duplicated because it maps to the same sort key
  // as the previous one.
  SavePasswordForm(/*password=*/kDefaultPassword,
                   /*username=*/kDefaultUsername,
                   /*origin=*/@"https://example.com/example");

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButtonForUsernameAndSites(
                                          kDefaultUsername, kDefaultSite)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:BatchDeleteConfirmationButton()]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating
  // PasswordTableViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Add button is visible and enabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a blocked form from password details view goes
// back to the list-of-passwords view which doesn't display that form anymore.
- (void)testBlockedFormDeletionInDetailView {
  // Save blocked form to be deleted later.
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://blocked.com"],
             kPasswordStoreErrorMessage);

  OpenPasswordManager();

  [GetInteractionForPasswordEntry(@"blocked.com") performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteBlockedSiteButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:BatchDeleteConfirmationButton()]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating
  // PasswordTableViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"secret.com")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Add button is visible and enabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a blocked form from password details view goes
// back to the list-of-passwords view which only displays a previously saved
// password.
- (void)testBlockedFormDeletionInDetailViewWithSavedForm {
  // Save blocked form to be deleted later.
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://blocked.com"],
             kPasswordStoreErrorMessage);

  SavePasswordForm();

  OpenPasswordManager();

  [GetInteractionForPasswordEntry(@"blocked.com") performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteBlockedSiteButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:BatchDeleteConfirmationButton()]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that the current view is now the list view, by locating
  // PasswordTableViewController.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(1, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed blocked site is no longer in the list.
  [GetInteractionForPasswordEntry(@"secret.com")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Verify existing saved password is still in the list.
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a password from password details can be cancelled.
- (void)testCancelDeletionInDetailView {
  // Save form to be deleted later.
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButtonForUsernameAndSites(
                                          kDefaultUsername, kDefaultSite)]
      performAction:grey_tap()];

  // Close the dialog by tapping on Password Details screen cancel button.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Check that the current view is still the detail view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsTableViewID)]
      assertWithMatcher:grey_notNil()];

  // Verify that the deletion did not happen.
  GREYAssertEqual(1u, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was removed from PasswordStore.");

  // Go back to the list view and verify that the password is still in the
  // list.
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that if the list view is in edit mode, the details password view is
// not accessible on tapping the entries.
- (void)testEditMode {
  // Save a form to have something to tap on.
  SavePasswordForm();

  OpenPasswordManager();

  TapNavigationBarEditButton();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  // Check that the current view is not the detail view, by failing to locate
  // the Copy button.
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to copy the password via the context menu item provide
// an appropriate feedback.
- (void)testCopyPasswordMenuItem {
  // Saving a form is needed for using the "password details" view.
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  // Tap the password cell to display the context menu.
  [GetInteractionForPasswordDetailItem(grey_text(kMaskedPassword))
      performAction:grey_tap()];

  // Tap the context menu item for copying.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_PASSWORD_COPY_MENU_ITEM)]
      performAction:grey_tap()];

  // Check the snackbar.
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that federated credentials have no password but show the federation.
- (void)testFederated {
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleFederatedOrigin:@"https://famous.provider.net"
                                   username:@"federated username"
                                     origin:@"https://example.com"],
             kPasswordStoreErrorMessage);

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  // Check that the Site and Username are present and correct.
  [[EarlGrey selectElementWithMatcher:
                 [self matcherForPasswordDetailCellWithWebsites:kDefaultSite]]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"federated username")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailFederation()]
      assertWithMatcher:grey_textFieldValue(@"famous.provider.net")];

  // Check that the password is not present.
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_nil()];

  // Check that editing doesn't require reauth.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
  // Ensure delete button is present after entering editing mode.
  [[EarlGrey selectElementWithMatcher:DeleteButtonForUsernameAndSites(
                                          @"federated username", kDefaultSite)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks the order of the elements in the detail view layout for a
// non-federated, non-blocked credential.
- (void)testLayoutNormal {
  SaveExamplePasswordFormWithNote();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 [self matcherForPasswordDetailCellWithWebsites:kDefaultSite]]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(kDefaultUsername)];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(kMaskedPassword)];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      assertWithMatcher:grey_text(@"concrete note")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailFederation()]
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordDetailItem(PasswordDetailUsername())
      assertWithMatcher:
          grey_layout(
              @[ Below() ],
              [self matcherForPasswordDetailCellWithWebsites:kDefaultSite])];
  [GetInteractionForPasswordDetailItem(PasswordDetailPassword())
      assertWithMatcher:grey_layout(@[ Below() ], PasswordDetailUsername())];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that entering too long note while editing a password blocks the save
// button and displays a footer explanation.
- (void)testLayoutWithLongNotes {
  SaveExamplePasswordFormWithNote();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  TapNavigationBarEditButton();
  [[EarlGrey selectElementWithMatcher:TooLongNoteFooter()]
      assertWithMatcher:grey_nil()];

  // Entering too long note results in "Done" button being disabled and footer
  // displayed.
  NSString* note = [@"" stringByPaddingToLength:1001
                                     withString:@"a"
                                startingAtIndex:0];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(note)];
  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsViewControllerID)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:TooLongNoteFooter()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Entering note with length close to the limit should result in displaying
  // footer only ("Done" button should be enabled).
  note = [@"" stringByPaddingToLength:1000 withString:@"a" startingAtIndex:0];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(note)];
  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:TooLongNoteFooter()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // For shorter notes there should be no footer and "Done" button enabled.
  note = [@"" stringByPaddingToLength:100 withString:@"a" startingAtIndex:0];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(note)];
  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:TooLongNoteFooter()]
      assertWithMatcher:grey_nil()];
}

// Checks the order of the elements in the detail view layout for a blocked
// credential.
- (void)testLayoutForBlockedCredential {
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleBlockedOrigin:@"https://example.com"],
             kPasswordStoreErrorMessage);

  OpenPasswordManager();

  [GetInteractionForPasswordEntry(@"example.com") performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 [self matcherForPasswordDetailCellWithWebsites:kDefaultSite]]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailFederation()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks the order of the elements in the detail view layout for a federated
// credential.
- (void)testLayoutFederated {
  GREYAssert([PasswordSettingsAppInterface
                 saveExampleFederatedOrigin:@"https://famous.provider.net"
                                   username:@"federated username"
                                     origin:@"https://example.com"],
             kPasswordStoreErrorMessage);

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:
                 [self matcherForPasswordDetailCellWithWebsites:kDefaultSite]]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"federated username")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailFederation()]
      assertWithMatcher:grey_textFieldValue(@"famous.provider.net")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_nil()];

  [GetInteractionForPasswordDetailItem(PasswordDetailUsername())
      assertWithMatcher:
          grey_layout(
              @[ Below() ],
              [self matcherForPasswordDetailCellWithWebsites:kDefaultSite])];
  [[EarlGrey selectElementWithMatcher:PasswordDetailFederation()]
      assertWithMatcher:grey_layout(@[ Below() ], PasswordDetailUsername())];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Check that stored entries are shown no matter what the preference for saving
// passwords is.
- (void)testStoredEntriesAlwaysShown {
  SavePasswordForm();

  OpenPasswordManager();

  // Toggle the "Save Passwords" control off and back on and check that stored
  // items are still present.
  BOOL isSwitchEnabled =
      [PasswordSettingsAppInterface isCredentialsServiceEnabled];
  BOOL kExpectedState[] = {isSwitchEnabled, !isSwitchEnabled};
  for (BOOL expected_state : kExpectedState) {
    OpenSettingsSubmenu();

    // Toggle the switch. It is located near the top, so if not interactable,
    // try scrolling up.
    [[EarlGrey
        selectElementWithMatcher:
            grey_allOf(chrome_test_util::TableViewSwitchCell(
                           kPasswordSettingsSavePasswordSwitchTableViewId,
                           expected_state),
                       grey_sufficientlyVisible(), nil)]
        performAction:TurnTableViewSwitchOn(!expected_state)];

    // Check that the switch has been modified.
    [EarlGrey selectElementWithMatcher:
                  grey_allOf(chrome_test_util::TableViewSwitchCell(
                                 kPasswordSettingsSavePasswordSwitchTableViewId,
                                 !expected_state),
                             grey_sufficientlyVisible(), nil)];

    // Close settings submenu.
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(SettingsDoneButton(),
                                            grey_sufficientlyVisible(), nil)]
        performAction:grey_tap()];

    // Check the stored items. Scroll down if needed.
    [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
        assertWithMatcher:grey_notNil()];
  }

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Check that toggling the switch for the "save passwords" preference changes
// the settings.
- (void)testPrefToggle {
  OpenPasswordManager();
  OpenSettingsSubmenu();
  // Toggle the "Save Passwords" control off and back on and check the
  // preferences.
  constexpr BOOL kExpectedState[] = {YES, NO};
  int count = 0;
  for (BOOL expected_initial_state : kExpectedState) {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::TableViewSwitchCell(
                       kPasswordSettingsSavePasswordSwitchTableViewId,
                       expected_initial_state)]
        performAction:TurnTableViewSwitchOn(!expected_initial_state)];
    const bool expected_final_state = !expected_initial_state;
    GREYAssertEqual(expected_final_state,
                    [PasswordSettingsAppInterface isCredentialsServiceEnabled],
                    @"State of the UI toggle differs from real preferences.");
    count++;
    // Verify histogram total count.
    GREYAssertNil(
        [MetricsAppInterface
            expectTotalCount:count
                forHistogram:
                    @"PasswordManager.Settings.ToggleOfferToSavePasswords"],
        @"Unexpected password settings toggle offer to save passwords switch "
        @"histogram count");

    // Verify histogram value and specific bucket count.
    GREYAssertNil(
        [MetricsAppInterface
             expectCount:1
               forBucket:expected_final_state
            forHistogram:
                @"PasswordManager.Settings.ToggleOfferToSavePasswords"],
        @"Unexpected histogram error for password settings toggle offer to "
        @"save passwords switch");
  }

  // "Done" to close settings submenu.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SettingsDoneButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // "Back" to go to root settings menu.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  // "Done" to close out.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a password from the list view works.
- (void)testDeletionInListView {
  // Save a password to be deleted later.
  SavePasswordForm();

  OpenPasswordManager();

  TapNavigationBarEditButton();

  // Select password entry to be removed.
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

    // Tap on the Delete button of the alert dialog.
  [[EarlGrey selectElementWithMatcher:BatchDeleteConfirmationButton()]
      performAction:grey_tap()];

  // Verify that the deletion was propagated to the PasswordStore.
  GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Stored password was not removed from PasswordStore.");
  // Verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Test that even with many passwords the settings are still usable. In
// particular, ensure that password entries "below the fold" are reachable and
// their detail view is shown on tapping.
// There are two bottlenecks potentially affecting the runtime of the test:
// (1) Storing passwords on initialisation.
// (2) Speed of EarlGrey UI operations such as scrolling.
// To keep the test duration reasonable, the delay from (1) is eliminated in
// storing just about enough passwords to ensure filling more than one page on
// any device. To limit the effect of (2), custom large scrolling steps are
// added to the usual scrolling actions.
// TODO(crbug.com/1442985): This test is flaky.
- (void)FLAKY_testManyPasswords {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // TODO(crbug.com/906551): Enable the test on iPad once the bug is fixed.
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad.");
  }

  // Enough just to ensure filling more than one page on all devices.
  constexpr int kPasswordsCount = 15;

  // Send the passwords to the queue to be added to the PasswordStore.
  [PasswordSettingsAppInterface saveExamplePasswordWithCount:kPasswordsCount];

  // Use TestStoreConsumer::GetStoreResults to wait for the background storing
  // task to complete and to verify that the passwords have been stored.
  GREYAssertEqual(kPasswordsCount,
                  [PasswordSettingsAppInterface passwordStoreResultsCount],
                  @"Unexpected PasswordStore results.");

  OpenPasswordManager();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // Wait for the loading indicator to disappear, and the sections to be on
  // screen, before scrolling.
  [[EarlGrey selectElementWithMatcher:SavedPasswordsHeaderMatcher()]
      assertWithMatcher:grey_notNil()];

  // Aim at an entry almost at the end of the list.
  constexpr int kRemoteIndex = kPasswordsCount - 4;

  [GetInteractionForPasswordEntry(
      [NSString stringWithFormat:@"example.com, %d accounts", kPasswordsCount])
      performAction:grey_tap()];

  // Check that the detail view loaded correctly by verifying the site content.
  [[[EarlGrey
      selectElementWithMatcher:
          [self matcherForPasswordDetailCellWithWebsites:
                    [NSString stringWithFormat:@"https://www%02d.example.com/",
                                               kRemoteIndex]]]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordDetailsTableViewID)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that if all passwords are deleted in the list view, the enabled Add
// button replaces the Done button.
- (void)testEditButtonUpdateOnDeletion {
  // Save a password to be deleted later.
  SavePasswordForm();

  OpenPasswordManager();

  TapNavigationBarEditButton();

  // Select password entry to be removed.
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

    // Tap on the Delete button of the alert dialog.
  [[EarlGrey selectElementWithMatcher:BatchDeleteConfirmationButton()]
      performAction:grey_tap()];

  // Verify that the Add button is visible and enabled.
  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Test export flow
- (void)testExportFlow {
  // Saving a form is needed for exporting passwords.
  SavePasswordForm();

  OpenPasswordManager();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:ToolbarSettingsSubmenuButton()]
      performAction:grey_tap()];

  [[[EarlGrey selectElementWithMatcher:
                  grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                                 IDS_IOS_EXPORT_PASSWORDS),
                             grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      performAction:grey_tap()];

  [GetInteractionForPasswordsExportConfirmAlert(
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_EXPORT_PASSWORDS)) performAction:grey_tap()];

  // Wait until the alerts are dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  id<GREYMatcher> exportButtonStatusMatcher =
      grey_accessibilityTrait(UIAccessibilityTraitNotEnabled);

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_EXPORT_PASSWORDS)]
      assertWithMatcher:exportButtonStatusMatcher];

  [ChromeEarlGrey verifyActivitySheetVisible];
  [ChromeEarlGrey closeActivitySheet];

  // Wait until the activity view is dismissed.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Check that export button is re-enabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_EXPORT_PASSWORDS)]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled))];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SettingsDoneButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Test that when user types text in search field, passwords and blocked
// items are filtered out and "save passwords" switch is removed.
- (void)testSearchPasswords {
  // TODO(crbug.com/1067818): Test doesn't pass on iPad device or simulator.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(
        @"This test doesn't pass on iPad device or simulator.");
  }

  SaveExamplePasswordForms();
  SaveExampleBlockedForms();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example11.com"]
      assertWithMatcher:grey_notNil()];

  [[self interactionForSinglePasswordEntryWithDomain:@"example12.com"]
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"exclude1.com")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"exclude2.com")
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      performAction:grey_replaceText(@"2")];

  [[self interactionForSinglePasswordEntryWithDomain:@"example11.com"]
      assertWithMatcher:grey_nil()];
  [[self interactionForSinglePasswordEntryWithDomain:@"example12.com"]
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"exclude1.com")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"exclude2.com")
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Test search and delete all passwords and blocked items.
// TODO(crbug.com/1441783): Flaky.
- (void)DISABLED_testSearchAndDeleteAllPasswords {
  SaveExamplePasswordForms();
  SaveExampleBlockedForms();

  OpenPasswordManager();

  // TODO(crbug.com/922511): Comment out because currently activating the search
  // bar will hide the "Edit" button in the top toolbar. Recover this when the
  // "Edit" button is moved to the bottom toolbar in the new Settings UI.
  //  [[EarlGrey selectElementWithMatcher:SearchTextField()]
  //      performAction:grey_replaceText(@"u")];
  //  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  TapNavigationBarEditButton();

  // Select all.
  [[self interactionForSinglePasswordEntryWithDomain:@"example11.com"]
      performAction:grey_tap()];
  [[self interactionForSinglePasswordEntryWithDomain:@"example12.com"]
      performAction:grey_tap()];

  [GetInteractionForPasswordEntry(@"exclude1.com") performAction:grey_tap()];
  [GetInteractionForPasswordEntry(@"exclude2.com") performAction:grey_tap()];

  // Delete them.
  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:BatchDeleteConfirmationButton()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];

  // All should be gone.
  [[self interactionForSinglePasswordEntryWithDomain:@"example11.com"]
      assertWithMatcher:grey_nil()];
  [[self interactionForSinglePasswordEntryWithDomain:@"example12.com"]
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"exclude1.com")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"exclude2.com")
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Test that user can't search passwords while in edit mode.
- (void)testCantSearchPasswordsWhileInEditMode {
  SaveExamplePasswordForms();

  OpenPasswordManager();
  TapNavigationBarEditButton();

  // Verify search bar is disabled.
  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      assertWithMatcher:grey_not(grey_userInteractionEnabled())];
  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Test that the user can edit a password that is part of search results.
- (void)testCanEditPasswordsFromASearch {
  SaveExamplePasswordForms();
  OpenPasswordManager();

  // TODO(crbug.com/922511): Comment out because currently activating the search
  // bar will hide the "Edit" button in the top toolbar. Recover this when the
  // "Edit" button is moved to the bottom toolbar in the new Settings UI.
  //  [[EarlGrey selectElementWithMatcher:SearchTextField()]
  //      performAction:grey_replaceText(@"2")];

  TapNavigationBarEditButton();

  // Select password entry to be edited.
  [GetInteractionForPasswordEntry(@"example12.com") performAction:grey_tap()];

  // Delete it
  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BatchDeleteConfirmationButton()]
      performAction:grey_tap()];

  // Filter results in nothing.
  // TODO(crbug.com/922511): Comment out because currently activating the search
  // bar will hide the "Edit" button in the top toolbar. Recover this when the
  // "Edit" button is moved to the bottom toolbar in the new Settings UI.
  //  [GetInteractionForPasswordEntry(@"example11.com, user1")
  //      assertWithMatcher:grey_nil()];
  //  [GetInteractionForPasswordEntry(@"example12.com, user2")
  //      assertWithMatcher:grey_nil()];

  // Get out of edit mode.
  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  // Remove filter search term.
  // TODO(crbug.com/1454514): Revert to grey_clearText when fixed in EG.
  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      performAction:grey_replaceText(@"")];

  // Only password 1 should show.
  [GetInteractionForPasswordEntry(@"example11.com")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"example12.com")
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Edit a password with only incognito tab opened should work.
- (void)testEditPasswordWithOnlyIncognitoTabOpen {
  SavePasswordForm();

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey closeAllNormalTabs];

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(kDefaultPassword)];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:EditPasswordConfirmationButton()]
      performAction:grey_tap()];

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(@"new password")];

  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to edit a password provide appropriate feedback.
- (void)testEditPassword {
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(kDefaultPassword)];

  // Check that empty password is not allowed, and done button is disabled.
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:EditPasswordConfirmationButton()]
      performAction:grey_tap()];

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(@"new password")];

  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to edit a username provide appropriate feedback.
- (void)testEditUsername {
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(kDefaultUsername)];

  // Empty username should work as well.
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"")];

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"new username")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"new username")];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to edit a username to a value which is already used for
// the same domain fails.
- (void)testEditUsernameFails {
  SavePasswordForm(/*password=*/kDefaultPassword,
                   /*username=*/@"concrete username1");

  SavePasswordForm(/*password=*/kDefaultPassword,
                   /*username=*/@"concrete username2");

  OpenPasswordManager();

  [GetInteractionForPasswordEntry(@"example.com, 2 accounts")
      performAction:grey_tap()];

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:UsernameTextfieldForUsernameAndSites(
                                          @"concrete username1", kDefaultSite)]
      assertWithMatcher:grey_textFieldValue(@"concrete username1")];

  // TODO(crbug.com/1454514): Revert to grey_clearText when fixed in EG.
  [[EarlGrey selectElementWithMatcher:UsernameTextfieldForUsernameAndSites(
                                          @"concrete username1", kDefaultSite)]
      performAction:grey_replaceText(@"")];

  [[EarlGrey selectElementWithMatcher:UsernameTextfieldForUsernameAndSites(
                                          @"concrete username1", kDefaultSite)]
      performAction:grey_replaceText(@"concrete username2")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      assertWithMatcher:grey_allOf(grey_not(grey_enabled()),
                                   grey_sufficientlyVisible(), nil)];

  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to edit a username provide appropriate feedback.
- (void)testCancelDuringEditing {
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];

  // Test that password value is unchanged.
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(kDefaultPassword)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that removing multiple passwords works fine.
- (void)testRemovingMultiplePasswords {
  constexpr int kPasswordsCount = 4;

  // Send the passwords to the queue to be added to the PasswordStore.
  [PasswordSettingsAppInterface saveExamplePasswordWithCount:kPasswordsCount];

    // Also save passwords for example11.com and example12.com, since the rest
    // will be grouped together.
  SaveExamplePasswordForms();

  OpenPasswordManager();
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  TapNavigationBarEditButton();

    [[GetInteractionForPasswordEntry(@"example.com, 4 accounts")
        assertWithMatcher:grey_notNil()] performAction:grey_tap()];
    [[GetInteractionForPasswordEntry(@"example11.com")
        assertWithMatcher:grey_notNil()] performAction:grey_tap()];
    [[GetInteractionForPasswordEntry(@"example12.com")
        assertWithMatcher:grey_notNil()] performAction:grey_tap()];

    [[EarlGrey selectElementWithMatcher:DeleteButton()]
        performAction:grey_tap()];

    [[EarlGrey selectElementWithMatcher:BatchDeleteConfirmationButton()]
        performAction:grey_tap()];

    // Wait until animation is over.
    [ChromeEarlGreyUI waitForAppToIdle];

    // Check that saved forms header is removed.
    [[EarlGrey selectElementWithMatcher:SavedPasswordsHeaderMatcher()]
        assertWithMatcher:grey_nil()];

    // Verify that the deletion was propagated to the PasswordStore.
    GREYAssertEqual(0, [PasswordSettingsAppInterface passwordStoreResultsCount],
                    @"Stored password was not removed from PasswordStore.");

    // Finally, verify that the Add button is visible and enabled, because there
    // are no other password entries left for deletion via the "Edit" mode.
    [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
        assertWithMatcher:grey_allOf(grey_enabled(), grey_sufficientlyVisible(),
                                     nil)];

    [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
        performAction:grey_tap()];
}

// Checks that the "Add" button is not shown on Edit.
- (void)testAddButtonDisabledInEditMode {
  SavePasswordForm();
  OpenPasswordManager();

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      performAction:grey_tap()];

  // Verify that the dialog didn't show up after tapping the Add button.
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests the add password flow from the toolbar button.
// TODO(crbug.com/1411944): Flaky, please re-enable once fixed.
- (void)DISABLED_testAddNewPasswordCredential {
  OpenPasswordManager();

  // Press "Add".
  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(kDefaultSite)];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"new username")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  // The "Add" button is enabled after site and password have been entered.
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_enabled()];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      performAction:grey_tap()];

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      assertWithMatcher:grey_textFieldValue(@"new password")];
}

// Validates that the Password Manager UI is dismissed if local authentication
// fails while within the Add Password UI.
- (void)testAddNewPasswordWithFailedAuth {
  OpenPasswordManager();

  // Press "Add".
  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      performAction:grey_tap()];

  CheckVisibilityOfElement(/*matcher=*/AddPasswordSaveButton(),
                           /*is_visible=*/true);

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [PasswordSettingsAppInterface
      mockReauthenticationModuleShouldReturnSynchronously:NO];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Verify that Add Password UI is covered by Reauthentication UI until local
  // authentication is passed.
  CheckVisibilityOfElement(/*matcher=*/AddPasswordSaveButton(),
                           /*is_visible=*/false);
  CheckVisibilityOfElement(/*matcher=*/ReauthenticationController(),
                           /*is_visible=*/true);

  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];

  // Password Manager should be dismissed leaving the Settings UI visible.
  CheckVisibilityOfElement(/*matcher=*/AddPasswordSaveButton(),
                           /*is_visible=*/false);
  CheckVisibilityOfElement(/*matcher=*/ReauthenticationController(),
                           /*is_visible=*/false);
  CheckVisibilityOfElement(/*matcher=*/SettingsCollectionView(),
                           /*is_visible=*/true);
}

// Checks that entering too long note while adding passwords blocks the save
// button and displays a footer explanation.
- (void)testAddPasswordLayoutWithLongNotes {
  OpenPasswordManager();

  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      performAction:grey_tap()];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(kDefaultSite)];
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"new username")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  // Entering too long note results in "Add" password being disabled and footer
  // displayed.
  NSString* note = [@"" stringByPaddingToLength:1001
                                     withString:@"a"
                                startingAtIndex:0];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(note)];
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPasswordDetailsViewControllerID)]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  [[EarlGrey selectElementWithMatcher:TooLongNoteFooter()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Entering note with length close to the limit should result in displaying
  // footer only ("add" button should be enabled).
  note = [@"" stringByPaddingToLength:1000 withString:@"a" startingAtIndex:0];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(note)];
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:TooLongNoteFooter()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // For shorter notes there should be no footer and "add" button enabled.
  note = [@"" stringByPaddingToLength:100 withString:@"a" startingAtIndex:0];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(note)];
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:TooLongNoteFooter()]
      assertWithMatcher:grey_nil()];
}

// Tests that when a new credential is saved or an existing one is updated via
// the add credential flow, the VC auto scrolls to the newly created or the
// updated entry.
// TODO(crbug.com/1377079): Flaky, please re-enable once fixed.
- (void)DISABLED_testAutoScroll {
  for (int i = 0; i < 20; i++) {
    NSString* username = [NSString stringWithFormat:@"username %d", i];
    NSString* password = [NSString stringWithFormat:@"password %d", i];
    NSString* site = [NSString stringWithFormat:@"https://example%d.com", i];
    SavePasswordForm(password, username, site);
  }

  OpenPasswordManager();

  // Press "Add".
  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      performAction:grey_tap()];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(@"https://zexample.com")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"zconcrete username")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      performAction:grey_tap()];

  // The newly created credential exists.
  [[self interactionForSinglePasswordEntryWithDomain:@"zexample.com"]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
}

// Tests that adding new password credential where the username and website
// matches with an existing credential results in showing a section alert for
// the existing credential.
- (void)testAddNewDuplicatedPasswordCredential {
  SavePasswordForm();

  OpenPasswordManager();

  // Press "Add".
  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      performAction:grey_tap()];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(@"https://example.com")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"password")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(kDefaultUsername)];

  // Verify Save Button is not enabled.
  // The enabled state is set async after checking for credential duplication.
  // Waiting until the button is not enabled.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
        assertWithMatcher:grey_not(grey_enabled())
                    error:&error];
    return error == nil;
  };

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting Save Button to be disabled.");

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [[EarlGrey selectElementWithMatcher:DuplicateCredentialViewPasswordButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"new username")];

  [[EarlGrey selectElementWithMatcher:EditDoneButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      assertWithMatcher:grey_textFieldValue(@"new username")];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that save button in add password view remains disabled when we switch
// from invalid to valid input in any of the fields (website, password, note),
// when there are still other fields with invalid input.
- (void)testAddPasswordSaveButtonStateOnFieldChanges {
  OpenPasswordManager();
  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      performAction:grey_tap()];

  NSString* long_note = [@"" stringByPaddingToLength:1001
                                          withString:@"a"
                                     startingAtIndex:0];
  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"username")];

  // Make sure that switching from invalid to valid note doesn't enable the save
  // button when website is invalid.
  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(@"example")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"password")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(long_note)];
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(@"note")];
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Make sure that switching from invalid to valid note doesn't enable the save
  // button when password is invalid.
  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(kDefaultSite)];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(long_note)];
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(@"note")];
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Make sure that from invalid to valid website and password doesn't enable
  // the save button when note is too long.
  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(@"example")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"")];
  [[EarlGrey selectElementWithMatcher:PasswordDetailNote()]
      performAction:grey_replaceText(long_note)];
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(kDefaultSite)];
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"password")];
  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests that the duplicate credential section alert is shown when the user adds
// a credential that has the same website as that of an existing credential
// (does not contain username).
// TODO(crbug.com/1474949): Fix flaky test & re-enable.
#if TARGET_OS_SIMULATOR
#define MAYBE_testDuplicatedCredentialWithNoUsername \
  DISABLED_testDuplicatedCredentialWithNoUsername
#else
#define MAYBE_testDuplicatedCredentialWithNoUsername \
  testDuplicatedCredentialWithNoUsername
#endif
- (void)MAYBE_testDuplicatedCredentialWithNoUsername {
  OpenPasswordManager();

  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      performAction:grey_tap()];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(@"https://www.example.com")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"new password")];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      performAction:grey_tap()];

  // Add another credential.
  [[EarlGrey selectElementWithMatcher:AddPasswordButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(@"https://www.example.com")];

  // Test that the section alert for duplicated credential is shown.
  [[EarlGrey selectElementWithMatcher:DuplicateCredentialViewPasswordButton()]
      assertWithMatcher:grey_enabled()];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(@"new username")];

  // Wait until duplicated message disappearing animation is done.
  [ChromeEarlGreyUI waitForAppToIdle];

  // Test that the section alert for duplicated credential is removed.
  [[EarlGrey selectElementWithMatcher:DuplicateCredentialViewPasswordButton()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"znew password")];

  [[EarlGrey selectElementWithMatcher:AddPasswordSaveButton()]
      performAction:grey_tap()];

  [GetInteractionForPasswordEntry(@"example.com, 2 accounts")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"example.com, 2 accounts")
      performAction:grey_tap()];

  TapNavigationBarEditButton();

  [[EarlGrey selectElementWithMatcher:PasswordTextfieldForUsernameAndSites(
                                          @"new username",
                                          @"https://www.example.com/")]
      assertWithMatcher:grey_textFieldValue(@"znew password")];
}

// Tests that the error message is shown when the top-level domain is missing
// when adding a new credential.
- (void)testTLDMissingMessage {
  OpenPasswordManager();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // Press "Add".
  [[EarlGrey selectElementWithMatcher:AddPasswordToolbarButton()]
      performAction:grey_tap()];

  // Fill form.
  [[EarlGrey selectElementWithMatcher:AddPasswordWebsite()]
      performAction:grey_replaceText(@"example")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailPassword()]
      performAction:grey_replaceText(@"password")];

  [[EarlGrey selectElementWithMatcher:PasswordDetailUsername()]
      performAction:grey_replaceText(kDefaultUsername)];

  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSStringF(
                     IDS_IOS_SETTINGS_PASSWORDS_MISSING_TLD_DESCRIPTION,
                     u"example.com"))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that reauthentication is not required to show password when notes are
// enabled since the reauthentication happens before navigating to the details
// view in this scenario.
- (void)testShowHidePassword {
  SavePasswordForm();

  OpenPasswordManager();

  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      assertWithMatcher:grey_sufficientlyVisible()];
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];
  [GetInteractionForPasswordDetailItem(HidePasswordButton())
      assertWithMatcher:grey_sufficientlyVisible()];
  [GetInteractionForPasswordDetailItem(HidePasswordButton())
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the percentage of favicons for the password manager metric is
// logged properly when there are passwords with a favicon.
- (void)testLogFaviconsForPasswordsPercentageMetricWithPassword {
  // Sign-in and synced user.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncInitializedTimeout];

  // Add passwords for the user.
  SaveExamplePasswordForms();

  OpenPasswordManager();

  // Make sure the cell is loaded properly before tapping on it.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[self interactionForSinglePasswordEntryWithDomain:@"example12.com"]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting for the cell to load");

  [[self interactionForSinglePasswordEntryWithDomain:@"example12.com"]
      performAction:grey_tap()];

  // Metric: Percentage of favicons with image.
  // Verify that histogram is called.
  NSError* error = [MetricsAppInterface
      expectTotalCount:1
          forHistogram:@"IOS.PasswordManager.Favicons.Percentage"];
  if (error) {
    GREYFail([error description]);
  }
  // Verify the logged value of the histogram.
  error = [MetricsAppInterface
         expectSum:0
      forHistogram:@"IOS.PasswordManager.Favicons.Percentage"];
  if (error) {
    GREYFail([error description]);
  }
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the percentage of favicons for the password manager metric is
// logged properly when there are no password.
- (void)testLogFaviconsForPasswordsPercentageMetricNoPassword {
  OpenPasswordManager();

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Metric: Percentage of favicons with image.
  // This histogram is not logged.
  NSError* error = [MetricsAppInterface
      expectTotalCount:0
          forHistogram:@"IOS.PasswordManager.Favicons.Percentage"];
  if (error) {
    GREYFail([error description]);
  }
}

- (void)testOpenPasswordSettingsSubmenu {
  OpenPasswordManager();
  OpenSettingsSubmenu();

  CheckVisibilityOfElement(/*matcher=*/PasswordSettingsTableView(),
                           /*is_visible=*/true);
}

// Tests that the Password Manager UI is dismissed when local authentication
// fails while within the Password Settings UI.
- (void)testOpenPasswordSettingsSubmenuWithFailedAuth {
  OpenPasswordManager();
  OpenSettingsSubmenu();

  CheckVisibilityOfElement(/*matcher=*/PasswordSettingsTableView(),
                           /*is_visible=*/true);

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  [PasswordSettingsAppInterface
      mockReauthenticationModuleShouldReturnSynchronously:NO];

  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Password Settings shouldn't be visible until successful authentication.
  CheckVisibilityOfElement(/*matcher=*/PasswordSettingsTableView(),
                           /*is_visible=*/false);
  CheckVisibilityOfElement(/*matcher=*/ReauthenticationController(),
                           /*is_visible=*/true);

  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];

  // The Password Manager UI should be gone leaving the Settings UI visible.
  CheckVisibilityOfElement(/*matcher=*/PasswordSettingsTableView(),
                           /*is_visible=*/false);
  CheckVisibilityOfElement(/*matcher=*/PasswordsTableViewMatcher(),
                           /*is_visible=*/false);
  CheckVisibilityOfElement(/*matcher=*/ReauthenticationController(),
                           /*is_visible=*/false);
  CheckVisibilityOfElement(/*matcher=*/SettingsCollectionView(),
                           /*is_visible=*/true);
}

// Tests that the detail text in this row reflects the status of the system
// setting.
- (void)testPasswordsInOtherAppsItem {
  OpenPasswordManager();
  OpenSettingsSubmenu();

  id<GREYMatcher> onMatcher = grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_IOS_SETTING_ON)),
      grey_sufficientlyVisible(), nil);

  id<GREYMatcher> offMatcher = grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_IOS_SETTING_OFF)),
      grey_sufficientlyVisible(), nil);

  // No detail text should appear until the AutoFill status has been populated.
  [[EarlGrey selectElementWithMatcher:onMatcher] assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:offMatcher] assertWithMatcher:grey_nil()];

  [PasswordsInOtherAppsAppInterface startFakeManagerWithAutoFillStatus:NO];
  [[EarlGrey selectElementWithMatcher:offMatcher]
      assertWithMatcher:grey_notNil()];

  [PasswordsInOtherAppsAppInterface setAutoFillStatus:YES];
  [[EarlGrey selectElementWithMatcher:onMatcher]
      assertWithMatcher:grey_notNil()];
}

// Tests that the detail view is dismissed when the last password is deleted,
// but stays if there are still passwords on the page.
- (void)testPasswordsDeletionNavigation {
  // Save forms with the same origin to be deleted later.
  SavePasswordForm(/*password=*/@"password1",
                   /*username=*/@"user1",
                   /*origin=*/@"https://example1.com");
  SavePasswordForm(/*password=*/@"password2",
                   /*username=*/@"user2",
                   /*origin=*/@"https://example1.com");
  SavePasswordForm(/*password=*/@"password3",
                   /*username=*/@"user3",
                   /*origin=*/@"https://example3.com");

  OpenPasswordManager();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [GetInteractionForPasswordEntry(@"example1.com, 2 accounts")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"example1.com, 2 accounts")
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  // Delete first password.
  DeleteCredential(@"user1", @"https://example1.com/");

  // Check that the current view is still the password details since there is
  // still one more password left on the view.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kPasswordDetailsTableViewID)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting for the view to load");

  // Delete last password.
  DeleteCredential(@"user2", @"https://example1.com/");

  // Check that the current view is now the password manager since we deleted
  // the last password.
  condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };

  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Waiting for the view to load");

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

- (void)testAccountStorageSwitchShownIfSignedIn_SyncToSigninDisabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();

  // User should be opted in by default after sign-in.
  GREYElementInteraction* accountStorageSwitch =
      [EarlGrey selectElementWithMatcher:
                    chrome_test_util::TableViewSwitchCell(
                        kPasswordSettingsAccountStorageSwitchTableViewId,
                        /*is_toggled_on=*/YES)];
  [accountStorageSwitch assertWithMatcher:grey_sufficientlyVisible()];
  // The toggle text must contain the signed-in account.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSStringF(
                                   IDS_IOS_ACCOUNT_STORAGE_OPT_IN_SUBLABEL,
                                   base::SysNSStringToUTF16(
                                       fakeIdentity.userEmail)))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [accountStorageSwitch performAction:TurnTableViewSwitchOn(NO)];

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TableViewSwitchCell(
                     kPasswordSettingsAccountStorageSwitchTableViewId,
                     /*is_toggled_on=*/NO)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testAccountStorageSwitchHiddenIfSignedIn_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kPasswordSettingsAccountStorageSwitchTableViewId)]
      assertWithMatcher:grey_nil()];
}

- (void)testAccountStorageSwitchDisabledByPolicy_SyncToSigninDisabled {
  policy_test_utils::SetPolicy(std::string("[\"passwords\"]"),
                               policy::key::kSyncTypesListDisabled);

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();

  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TableViewSwitchCell(
                     kPasswordSettingsAccountStorageSwitchTableViewId,
                     /*is_toggled_on=*/NO,
                     /*is_enabled=*/NO)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testAccountStorageSwitchHiddenIfSignedOut {
  OpenPasswordManager();
  OpenSettingsSubmenu();

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kPasswordSettingsAccountStorageSwitchTableViewId)]
      assertWithMatcher:grey_nil()];
}

- (void)testAccountStorageSwitchHiddenIfSyncing {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];
  [ChromeEarlGrey waitForSyncFeatureEnabled:YES
                                syncTimeout:kSyncInitializedTimeout];

  OpenPasswordManager();
  OpenSettingsSubmenu();

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kPasswordSettingsAccountStorageSwitchTableViewId)]
      assertWithMatcher:grey_nil()];
}

- (void)testMovePasswordToAccount {
  SavePasswordForm(/*password=*/@"localPassword",
                   /*username=*/@"username",
                   /*origin=*/@"https://local.com");
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];
  OpenPasswordManager();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // `passwordMatcher` includes grey_sufficientlyVisible() because there are
  // other invisible cells when password details is closed later.
  id<GREYMatcher> passwordMatcher = grey_allOf(
      ButtonWithAccessibilityID(@"local.com"), grey_sufficientlyVisible(), nil);
  id<GREYMatcher> localIconMatcher =
      grey_allOf(grey_accessibilityID(kLocalOnlyPasswordIconID),
                 grey_ancestor(passwordMatcher), nil);
  [GetInteractionForListItem(localIconMatcher, kGREYDirectionDown)
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:passwordMatcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kMovePasswordToAccountButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kMovePasswordToAccountButtonID)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kMovePasswordToAccountButtonID)]
      assertWithMatcher:grey_notVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [GetInteractionForListItem(localIconMatcher, kGREYDirectionDown)
      assertWithMatcher:grey_notVisible()];
}

// Regression test for crbug.com/1431975. Similar to testMovePasswordToAccount
// above but the only open tab is an incognito one.
- (void)testMovePasswordToAccountWithOnlyIncognitoTabOpen {
  SavePasswordForm(/*password=*/@"localPassword",
                   /*username=*/@"username",
                   /*origin=*/@"https://local.com");
  [SigninEarlGreyUI signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]
                                enableSync:NO];

  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey closeAllNormalTabs];

  OpenPasswordManager();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // `passwordMatcher` includes grey_sufficientlyVisible() because there are
  // other invisible cells when password details is closed later.
  id<GREYMatcher> passwordMatcher = grey_allOf(
      ButtonWithAccessibilityID(@"local.com"), grey_sufficientlyVisible(), nil);
  id<GREYMatcher> localIconMatcher =
      grey_allOf(grey_accessibilityID(kLocalOnlyPasswordIconID),
                 grey_ancestor(passwordMatcher), nil);
  [GetInteractionForListItem(localIconMatcher, kGREYDirectionDown)
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:passwordMatcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kMovePasswordToAccountButtonID)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kMovePasswordToAccountButtonID)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kMovePasswordToAccountButtonID)]
      assertWithMatcher:grey_notVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [GetInteractionForListItem(localIconMatcher, kGREYDirectionDown)
      assertWithMatcher:grey_notVisible()];
}

// Tests that the save passwords in account section is hidden when syncing.
- (void)testSavePasswordsInAccountHiddenWhenSyncing {
  SavePasswordForm();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];

  OpenPasswordManager();
  OpenSettingsSubmenu();

  // Ensure module is hidden.
  CheckSavePasswordsInAccountSectionHidden();

  // Close password manager settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SettingsDoneButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

// Tests that the save passwords in account section is hidden when not
// signed-in.
- (void)testSavePasswordsInAccountHiddenWhenNotSignedIn {
  SavePasswordForm();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  OpenPasswordManager();
  OpenSettingsSubmenu();

  // Ensure module is hidden.
  CheckSavePasswordsInAccountSectionHidden();

  // Close password manager settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SettingsDoneButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

// Tests that the save passwords in account section is hidden when not opted-in
// for account storage.
- (void)testSavePasswordsInAccountHiddenWhenNotOptedInToAccountStorage {
  SavePasswordForm();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Set save passwords into account switch to off.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TableViewSwitchCell(
                     kPasswordSettingsAccountStorageSwitchTableViewId, YES)]
      performAction:TurnTableViewSwitchOn(NO)];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure module is now hidden.
  CheckSavePasswordsInAccountSectionHidden();
}

// Tests that the save passwords in account section is shown when the user is
// eligible.
- (void)testSavePasswordsInAccountShownWhenEligible {
  SavePasswordForm(@"passwordtest1", @"user1", @"https://test1.com");

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();

  // Ensure the move passwords to account section is shown.
  CheckSavePasswordsInAccountSectionVisible();

  // Close password manager settings menu.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SettingsDoneButton(),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

// Tests that the confirmation dialog contains the correct string for saving one
// distinct domain to the account.
- (void)testSavePasswordsInAccountOneDistinctDomain {
  SavePasswordForm(@"password1", @"user1", @"https://example1.com");

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on save passwords to account button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure the confirmation dialog appears with the correct patterned string.
  NSString* result = @"You can save your password for example1.com in your "
                     @"Google Account, foo1@gmail.com";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(result)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the confirmation dialog contains the correct string for saving two
// distinct domains to the account.
- (void)testSavePasswordsInAccountTwoDistinctDomains {
  SavePasswordForm(@"password1", @"user1", @"https://example1.com");
  SavePasswordForm(@"password1", @"user1", @"https://example2.com");

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on save passwords to account button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure the confirmation dialog appears with the correct patterned string.
  NSString* result = @"You can save your passwords for example1.com and "
                     @"example2.com in your Google Account, foo1@gmail.com";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(result)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the confirmation dialog contains the correct string for saving
// three distinct domains to the account.
- (void)testSavePasswordsInAccountThreeDistinctDomains {
  SavePasswordForm(@"password1", @"user1", @"https://example1.com");
  SavePasswordForm(@"password1", @"user1", @"https://example2.com");
  SavePasswordForm(@"password1", @"user1", @"https://example3.com");

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on save passwords to account button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure the confirmation dialog appears with the correct patterned string.
  NSString* result = @"You can save your passwords for example1.com, "
                     @"example2.com, and 1 other "
                     @"in your Google Account, foo1@gmail.com";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(result)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the confirmation dialog contains the correct string for saving
// four distinct domains to the account.
- (void)testSavePasswordsInAccountFourDistinctDomains {
  SavePasswordForm(@"password1", @"user1", @"https://example1.com");
  SavePasswordForm(@"password1", @"user1", @"https://example2.com");
  SavePasswordForm(@"password1", @"user1", @"https://example3.com");
  SavePasswordForm(@"password1", @"user1", @"https://example4.com");

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on save passwords to account button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure the confirmation dialog appears with the correct patterned string.
  NSString* result =
      @"You can save your passwords for example1.com, example2.com, and 2 "
      @"others in your Google Account, foo1@gmail.com";
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(result)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the local password is moved when accepting the confirmation
// dialog, and that the corresponding snackbar appears.
- (void)testSavePasswordsInAccountFlowCompletes {
  SavePasswordForm(@"password1", @"user1", @"https://example1.com");

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on save passwords to account button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on "Save in Account" (accept) button.
  [SaveInAccountConfirmationDialogButton() performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure that the save passwords to account module has disappeared.
  CheckSavePasswordsInAccountSectionHidden();

  // Ensure the correct snackbar appears.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityLabel(
              @"Password saved in your Google Account, foo1@gmail.com")];
}

// Tests that the local password is not moved when accepting the confirmation
// dialog since authentication failed.
- (void)testSavePasswordsInAccountFlowAuthFailed {
  SavePasswordForm(@"password1", @"user1", @"https://example1.com");

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on save passwords to account button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on "Save in Account" (accept) button.
  [SaveInAccountConfirmationDialogButton() performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure the save passwords to account section is still there.
  CheckSavePasswordsInAccountSectionVisible();
}

// Tests that the "set passcode" alert is shown if no authentication is set when
// user tries to save passwords in their account.
- (void)testSavePasswordsInAccountFlowNoAuthSetOnDevice {
  SavePasswordForm(@"password1", @"user1", @"https://example1.com");

  [PasswordSettingsAppInterface mockReauthenticationModuleCanAttempt:NO];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  OpenPasswordManager();
  OpenSettingsSubmenu();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on save passwords to account button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on "Save in Account" (accept) button.
  [SaveInAccountConfirmationDialogButton() performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure the "set passcode" alert is shown.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityLabel(l10n_util::GetNSString(
              IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SET_UP_SCREENLOCK_CONTENT))];
}

// Tests that the local passwords are correctly handled in the save
// passwords to account flow, and the correct snackbar appears.
- (void)testSavePasswordsInAccountFlowCompletesMovingPasswords {
  SavePasswordForm(@"password1", @"user1", @"https://example1.com");
  SavePasswordForm(@"password2", @"user1", @"https://example1.com");
  SavePasswordForm(@"password1", @"user1", @"https://example2.com");

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];
  SavePasswordForm(@"password1", @"user1", @"https://example1.com");

  OpenPasswordManager();
  OpenSettingsSubmenu();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on save passwords to account button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Tap on "Save in Account" (accept) button.
  [SaveInAccountConfirmationDialogButton() performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure that the save passwords to account module has disappeared.
  CheckSavePasswordsInAccountSectionHidden();

  // Ensure the correct snackbar appears.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityLabel(
              @"Passwords saved in your Google Account, foo1@gmail.com")];
}

// Checks opening the password manager with a successful reauthentication shows
// the Password Manager.
- (void)testOpenPasswordManagerWithSuccessfulAuth {
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  // Delay the auth result to be able to validate that the passwords are not
  // visible until the result is emitted.
  [PasswordSettingsAppInterface
      mockReauthenticationModuleShouldReturnSynchronously:NO];

  OpenPasswordManager();

  // Password Manager should be blocked until successful auth.
  [[EarlGrey selectElementWithMatcher:PasswordsTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Successful auth should remove blocking view and Password Manager should be
  // visible visible.
  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];
  [[EarlGrey selectElementWithMatcher:PasswordsTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Check password manager visit metric.
  CheckPasswordManagerVisitMetricCount(1);

  // Check Reauthentication UI metrics.
  CheckReauthenticationUIEventMetricTotalCount(2);
  CheckReauthenticationUIEventMetric(ReauthenticationEvent::kAttempt);
  CheckReauthenticationUIEventMetric(ReauthenticationEvent::kSuccess);
}

// Checks opening the password manager with a failed reauthentication does not
// show passwords and closes the Password Manager.
- (void)testOpenPasswordManagerWithFailedAuth {
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];
  // Delay the auth result to be able to validate that the passwords are not
  // visible until the result is emitted.
  [PasswordSettingsAppInterface
      mockReauthenticationModuleShouldReturnSynchronously:NO];

  OpenPasswordManager();

  // Password Manager should be blocked until successful auth.
  [[EarlGrey selectElementWithMatcher:PasswordsTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Failed auth should dismiss the Password Manager, the Settings menu is
  // displayed.
  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];
  CheckVisibilityOfElement(/*matcher=*/SettingsCollectionView(),
                           /*is_visible=*/true);

  // Check password manager visit metric.
  CheckPasswordManagerVisitMetricCount(0);

  // Check Reauthentication UI metrics.
  CheckReauthenticationUIEventMetricTotalCount(2);
  CheckReauthenticationUIEventMetric(ReauthenticationEvent::kAttempt);
  CheckReauthenticationUIEventMetric(ReauthenticationEvent::kFailure);
}

// Checks opening the password manager with no passcode does not show passwords
// and displays an alert prompting the user to set a passcode.
- (void)testOpenPasswordManagerWithWithoutPasscodeSet {
  [PasswordSettingsAppInterface mockReauthenticationModuleCanAttempt:NO];

  OpenPasswordManager();

  // Password Manager should be blocked.
  [[EarlGrey selectElementWithMatcher:PasswordsTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  if ([PasswordSettingsAppInterface isPasscodeSettingsAvailable]) {
    // Go to Settings should be present.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::AlertAction(
                                            @"Go to Settings")]
        assertWithMatcher:grey_notNil()];
  } else {
    // Dismiss the passcode alert, this should dismiss the Password Manager.
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                     IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE))]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
        performAction:grey_tap()];

    // Check for the Settings page after Password Manager is gone.
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SettingsCollectionView()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Check Reauthentication UI metrics.
  CheckReauthenticationUIEventMetricTotalCount(2);
  CheckReauthenticationUIEventMetric(ReauthenticationEvent::kAttempt);
  CheckReauthenticationUIEventMetric(ReauthenticationEvent::kMissingPasscode);

  // Check password manager visit metric.
  CheckPasswordManagerVisitMetricCount(0);
}

// Tests that password manager visit histogram is recorded after opening
// password manager without authentication required.
- (void)testPasswordManagerVisitMetricWithoutAuthRequired {
  OpenPasswordManager();

  CheckPasswordManagerVisitMetricCount(1);

  CheckReauthenticationUIEventMetricTotalCount(0);
}

// Tests that the Password Manager is opened is search mode when opened from the
// Search Passwords widget.
- (void)testOpenSearchPasswordsWidget {
  // Add a saved password to not get the Password Manager's empty state.
  SavePasswordForm();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [ChromeEarlGrey
      sceneOpenURL:
          GURL("chromewidgetkit://search-passwords-widget/search-passwords")];

  // The Password Manager should be visible behind the keyboard.
  GREYAssertTrue([EarlGrey isKeyboardShownWithError:nil],
                 @"Keyboard Should be Shown");
  [[EarlGrey selectElementWithMatcher:PasswordsTableViewMatcher()]
      assertWithMatcher:grey_minimumVisiblePercent(0.5)];

  // The search bar should be enabled.
  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      assertWithMatcher:grey_userInteractionEnabled()];

  // Dismiss the search controller and the Password Manager.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the indication to open the Password Manager in search mode does
// not persist after it was first opened. For example, the search bar shouldn't
// get automatically enabled when going back to the Password Manager when the
// Password Manager was initially opened with the Search Passwords widget.
- (void)testGoingBackAfterOpeningInSearchMode {
  // Add a saved password to not get the Password Manager's empty state.
  SavePasswordForm();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  // Open the Password Manager in search mode with the  Search Passwords widget.
  [ChromeEarlGrey
      sceneOpenURL:
          GURL("chromewidgetkit://search-passwords-widget/search-passwords")];

  // The search bar should be enabled.
  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      assertWithMatcher:grey_userInteractionEnabled()];

  // Dismiss the search controller.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(IDS_CANCEL)]
      performAction:grey_tap()];

  // Open password details.
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  // Navigate back to the Password Manager. The search bar should not be
  // enabled.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      assertWithMatcher:grey_userInteractionEnabled()];
}

// Tests that tapping the close button of the Password Manager widget promo
// removes the promo from the table view.
- (void)testClosingPasswordManagerWidgetPromo {
  // Add a saved password to not get the Password Manager's empty state.
  SavePasswordForm();

  OpenPasswordManager();

  // The Password Manager widget promo should be visible.
  CheckPasswordManagerWidgetPromoVisible();

  // Tap the promo's close button.
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromoCloseButton()]
      performAction:grey_tap()];

  // The Password Manager widget promo should now be gone.
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromo()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that tapping the more info button of the Password Manager widget
// promo displays the instructions on how to install the widget.
- (void)testOpeningPasswordManagerWidgetPromoInstructions {
  // Add a saved password to not get the Password Manager's empty state.
  SavePasswordForm();

  OpenPasswordManagerWidgetPromoInstructions();
}

// Tests the Password Manager UI is dismissed after a failed local
// authentication while in the Widget Promo Instructions page.
- (void)testOpeningPasswordManagerWidgetPromoInstructionsWithFailedAuth {
  // Add a saved password to not get the Password Manager's empty state.
  SavePasswordForm();

  OpenPasswordManagerWidgetPromoInstructions();

  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kFailure];

  // Settings UI should be covered by Password Manager UI.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      assertWithMatcher:grey_notVisible()];

  // Trigger local authentication by backgrounding the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Failed auth should dismiss the whole Password Manager leaving the Settings
  // UI visible.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the more info and close buttons of the Password Manager widget
// promo are disabled when the Password Manager is in edit mode.
- (void)testPasswordManagerWidgetPromoInEditMode {
  // Add a saved password to not get the Password Manager's empty state.
  SavePasswordForm();

  OpenPasswordManager();

  TapNavigationBarEditButton();

  // The Password Manager widget promo should be visible.
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromo()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // The close button should be disabled.
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromoCloseButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // The more info button should be disabled.
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromoMoreInfoButton(
                                          /*enabled=*/false)]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests that the Password Manager widget promo is as expected when
// transitioning between portrait and landscape modes. Also tests that the close
// button still works after the layout change.
- (void)testPasswordManagerWidgetPromoDeviceOrientation {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Landscape orientation doesn't change the look of "
                           @"the instruction view on iPads.");
  }

  // Add a saved password to not get the Password Manager's empty state.
  SavePasswordForm();

  OpenPasswordManager();

  // The Password Manager widget promo should be visible.
  CheckPasswordManagerWidgetPromoVisible();

  // The Password Manager widget promo's elements should be visible in landscape
  // mode.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  CheckPasswordManagerWidgetPromoVisible();

  // The promo's close button should still be tappable in landscape mode.
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromoCloseButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromo()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Password Manager widget promo's instruction screen can be
// dismissed by swipping it down and by tapping its close button.
- (void)testDismissPasswordManagerWidgetPromoInstructionsScreen {
  // Add a saved password to not get the Password Manager's empty state.
  SavePasswordForm();

  OpenPasswordManager();

  // Tap the promo's more info button.
  [[EarlGrey
      selectElementWithMatcher:PasswordManagerWidgetPromoMoreInfoButton()]
      performAction:grey_tap()];

  // The Password Manager widget promo's instructions should be visible.
  CheckPasswordManagerWidgetPromoInstructionScreenVisible();

  // Swipe down to dismiss the instructions screen.
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromoInstructions()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // The Password Manager widget promo should be visible.
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromo()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the promo's more info button.
  [[EarlGrey
      selectElementWithMatcher:PasswordManagerWidgetPromoMoreInfoButton()]
      performAction:grey_tap()];

  // The Password Manager widget promo's instructions should be visible.
  CheckPasswordManagerWidgetPromoInstructionScreenVisible();

  // Tap the instruction screen's close button.
  [[EarlGrey selectElementWithMatcher:
                 PasswordManagerWidgetPromoInstructionsCloseButton()]
      performAction:grey_tap()];

  // The Password Manager widget promo should be visible.
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromo()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the Password Manager widget promo's instruction screen is as
// expected when transitioning between portrait and landscape modes.
- (void)testPasswordManagerWidgetPromoInstructionsDeviceOrientation {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Landscape orientation doesn't change the look of "
                           @"the instruction view on iPads.");
  }

  // Add a saved password to not get the Password Manager's empty state.
  SavePasswordForm();

  OpenPasswordManager();

  // The Password Manager widget promo should be visible.
  [[EarlGrey selectElementWithMatcher:PasswordManagerWidgetPromo()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the promo's more info button.
  [[EarlGrey
      selectElementWithMatcher:PasswordManagerWidgetPromoMoreInfoButton()]
      performAction:grey_tap()];

  // The Password Manager widget promo's instructions should be visible with its
  // image.
  CheckPasswordManagerWidgetPromoInstructionScreenVisible();

  // The Password Manager widget promo's instructions should be visible with no
  // image in landscape mode.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
  CheckPasswordManagerWidgetPromoInstructionScreenVisible(
      /*image_hidden=*/true);

  // When going back to portrait mode, the Password Manager widget promo's
  // instructions should be visible with its image.
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  CheckPasswordManagerWidgetPromoInstructionScreenVisible();
}

// Checks password details page offers move to account option if the password is
// saved in the local store.
- (void)testMovePasswordToAccountStoreIfSignedIn_SyncToSigninEnabled {
  // Save form to be moved to account later.
  SavePasswordForm();

  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Open password details view for the saved password.
  OpenPasswordManager();
  [[self interactionForSinglePasswordEntryWithDomain:@"example.com"]
      performAction:grey_tap()];

  // Verify the locally-saved password details page has a move to account
  // option.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsMoveToAccountButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the move to account button to move the local password to account
  // store.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsMoveToAccountButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the password details page does not show the move to account
  // option anymore.
  [[EarlGrey selectElementWithMatcher:PasswordDetailsMoveToAccountButton()]
      assertWithMatcher:grey_notVisible()];
}

@end
