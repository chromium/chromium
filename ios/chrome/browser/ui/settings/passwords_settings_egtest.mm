// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <TargetConditionals.h>

#include <utility>

#include "base/callback.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/time/time.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/settings/reauthentication_module.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/password_test_util.h"
#include "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ios/web/public/test/earl_grey/web_view_actions.h"
#include "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#include "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// This test complements
// password_details_collection_view_controller_unittest.mm. Very simple
// integration tests and features which are not currently unittestable should
// go here, the rest into the unittest.

using autofill::PasswordForm;
using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SetUpAndReturnMockReauthenticationModule;
using chrome_test_util::SetUpAndReturnMockReauthenticationModuleForExport;
using web::test::ElementSelector;

namespace {

// How many points to scroll at a time when searching for an element. Setting it
// too low means searching takes too long and the test might time out. Setting
// it too high could result in scrolling way past the searched element.
constexpr int kScrollAmount = 150;

// Returns the GREYElementInteraction* for the item on the password list with
// the given |matcher|. It scrolls in |direction| if necessary to ensure that
// the matched item is interactable. The result can be used to perform user
// actions or checks.
GREYElementInteraction* GetInteractionForListItem(id<GREYMatcher> matcher,
                                                  GREYDirection direction) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(direction, kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"SavePasswordsCollectionViewController")];
}

// Returns the GREYElementInteraction* for the cell on the password list with
// the given |username|. It scrolls down if necessary to ensure that the matched
// cell is interactable. The result can be used to perform user actions or
// checks.
GREYElementInteraction* GetInteractionForPasswordEntry(NSString* username) {
  return GetInteractionForListItem(ButtonWithAccessibilityLabel(username),
                                   kGREYDirectionDown);
}

// Returns the GREYElementInteraction* for the item on the detail view
// identified with the given |matcher|. It scrolls down if necessary to ensure
// that the matched cell is interactable. The result can be used to perform
// user actions or checks.
GREYElementInteraction* GetInteractionForPasswordDetailItem(
    id<GREYMatcher> matcher) {
  return [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  kScrollAmount)
      onElementWithMatcher:grey_accessibilityID(
                               @"PasswordDetailsCollectionViewController")];
}

// Matcher for a UITextField inside a SettingsSearchCell.
id<GREYMatcher> SearchTextField() {
  return grey_accessibilityID(@"SettingsSearchCellTextField");
}

id<GREYMatcher> SiteHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_SITE)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

id<GREYMatcher> UsernameHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

id<GREYMatcher> PasswordHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

id<GREYMatcher> FederationHeader() {
  return grey_allOf(
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION)),
      grey_accessibilityTrait(UIAccessibilityTraitHeader), nullptr);
}

GREYLayoutConstraint* Below() {
  return [GREYLayoutConstraint
      layoutConstraintWithAttribute:kGREYLayoutAttributeTop
                          relatedBy:kGREYLayoutRelationGreaterThanOrEqual
               toReferenceAttribute:kGREYLayoutAttributeBottom
                         multiplier:1.0
                           constant:0.0];
}

// Matcher for the Copy site button in Password Details view.
id<GREYMatcher> CopySiteButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel(
          [NSString stringWithFormat:@"%@: %@",
                                     l10n_util::GetNSString(
                                         IDS_IOS_SHOW_PASSWORD_VIEW_SITE),
                                     l10n_util::GetNSString(
                                         IDS_IOS_SETTINGS_SITE_COPY_BUTTON)]),
      grey_interactable(), nullptr);
}

// Matcher for the Copy username button in Password Details view.
id<GREYMatcher> CopyUsernameButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel([NSString
          stringWithFormat:@"%@: %@",
                           l10n_util::GetNSString(
                               IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME),
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON)]),
      grey_interactable(), nullptr);
}

// Matcher for the Copy password button in Password Details view.
id<GREYMatcher> CopyPasswordButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabel([NSString
          stringWithFormat:@"%@: %@",
                           l10n_util::GetNSString(
                               IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD),
                           l10n_util::GetNSString(
                               IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON)]),
      grey_interactable(), nullptr);
}

// Matcher for the Show password button in Password Details view.
id<GREYMatcher> ShowPasswordButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON)),
                    grey_interactable(), nullptr);
}

// Matcher for the Delete button in Password Details view.
id<GREYMatcher> DeleteButton() {
  return grey_allOf(ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON)),
                    grey_interactable(), nullptr);
}

// Matcher for the Delete button in the list view, located at the bottom of the
// screen.
id<GREYMatcher> DeleteButtonAtBottom() {
  // Selecting the "Delete" button is tricky, because its text is defined in the
  // private part of MD components library. But it is the unique
  // almost-completely visible element which is aligned with the bottom edge of
  // the screen.
  GREYLayoutConstraint* equalBottom = [GREYLayoutConstraint
      layoutConstraintWithAttribute:kGREYLayoutAttributeBottom
                          relatedBy:kGREYLayoutRelationEqual
               toReferenceAttribute:kGREYLayoutAttributeBottom
                         multiplier:1.0
                           constant:0.0];
  id<GREYMatcher> wholeScreen =
      grey_accessibilityID(@"SavePasswordsCollectionViewController");
  return grey_allOf(grey_layout(@[ equalBottom ], wholeScreen),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_accessibilityElement(),
                    grey_minimumVisiblePercent(0.98), nil);
}

// This is similar to grey_ancestor, but only limited to the immediate parent.
id<GREYMatcher> MatchParentWith(id<GREYMatcher> parentMatcher) {
  MatchesBlock matches = ^BOOL(id element) {
    id parent = [element isKindOfClass:[UIView class]]
                    ? [element superview]
                    : [element accessibilityContainer];
    return (parent && [parentMatcher matches:parent]);
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:[NSString stringWithFormat:@"parentThatMatches(%@)",
                                                       parentMatcher]];
  };
  return grey_allOf(
      grey_anyOf(grey_kindOfClass([UIView class]),
                 grey_respondsToSelector(@selector(accessibilityContainer)),
                 nil),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}

// Matches the pop-up (call-out) menu item with accessibility label equal to the
// translated string identified by |label|.
id<GREYMatcher> PopUpMenuItemWithLabel(int label) {
  // This is a hack relying on UIKit's internal structure. There are multiple
  // items with the label the test is looking for, because the menu items likely
  // have the same labels as the buttons for the same function. There is no easy
  // way to identify elements which are part of the pop-up, because the
  // associated classes are internal to UIKit. However, the pop-up items are
  // composed of a button-type element (without accessibility traits of a
  // button) owning a label, both with the same accessibility labels. This is
  // differentiating the pop-up items from the other buttons.
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(label)),
      MatchParentWith(grey_accessibilityLabel(l10n_util::GetNSString(label))),
      nullptr);
}

scoped_refptr<password_manager::PasswordStore> GetPasswordStore() {
  // ServiceAccessType governs behaviour in Incognito: only modifications with
  // EXPLICIT_ACCESS, which correspond to user's explicit gesture, succeed.
  // This test does not deal with Incognito, and should not run in Incognito
  // context. Therefore IMPLICIT_ACCESS is used to let the test fail if in
  // Incognito context.
  return IOSChromePasswordStoreFactory::GetForBrowserState(
      chrome_test_util::GetOriginalBrowserState(),
      ServiceAccessType::IMPLICIT_ACCESS);
}

// This class is used to obtain results from the PasswordStore and hence both
// check the success of store updates and ensure that store has finished
// processing.
class TestStoreConsumer : public password_manager::PasswordStoreConsumer {
 public:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> obtained) override {
    obtained_ = std::move(obtained);
  }

  const std::vector<autofill::PasswordForm>& GetStoreResults() {
    results_.clear();
    ResetObtained();
    GetPasswordStore()->GetAutofillableLogins(this);
    bool responded = base::test::ios::WaitUntilConditionOrTimeout(1.0, ^bool {
      return !AreObtainedReset();
    });
    GREYAssert(responded, @"Obtaining fillable items took too long.");
    AppendObtainedToResults();
    GetPasswordStore()->GetBlacklistLogins(this);
    responded = base::test::ios::WaitUntilConditionOrTimeout(1.0, ^bool {
      return !AreObtainedReset();
    });
    GREYAssert(responded, @"Obtaining blacklisted items took too long.");
    AppendObtainedToResults();
    return results_;
  }

 private:
  // Puts |obtained_| in a known state not corresponding to any PasswordStore
  // state.
  void ResetObtained() {
    obtained_.clear();
    obtained_.emplace_back(nullptr);
  }

  // Returns true if |obtained_| are in the reset state.
  bool AreObtainedReset() { return obtained_.size() == 1 && !obtained_[0]; }

  void AppendObtainedToResults() {
    for (const auto& source : obtained_) {
      results_.emplace_back(*source);
    }
    ResetObtained();
  }

  // Temporary cache of obtained store results.
  std::vector<std::unique_ptr<autofill::PasswordForm>> obtained_;

  // Combination of fillable and blacklisted credentials from the store.
  std::vector<autofill::PasswordForm> results_;
};

// Saves |form| to the password store and waits until the async processing is
// done.
void SaveToPasswordStore(const PasswordForm& form) {
  GetPasswordStore()->AddLogin(form);
  // Check the result and ensure PasswordStore processed this.
  TestStoreConsumer consumer;
  for (const auto& result : consumer.GetStoreResults()) {
    if (result == form)
      return;
  }
  GREYFail(@"Stored form was not found in the PasswordStore results.");
}

// Saves an example form in the store.
void SaveExamplePasswordForm() {
  PasswordForm example;
  example.username_value = base::ASCIIToUTF16("concrete username");
  example.password_value = base::ASCIIToUTF16("concrete password");
  example.origin = GURL("https://example.com");
  example.signon_realm = example.origin.spec();
  SaveToPasswordStore(example);
}

// Saves two example forms in the store.
void SaveExamplePasswordForms() {
  PasswordForm example1;
  example1.username_value = base::ASCIIToUTF16("user1");
  example1.password_value = base::ASCIIToUTF16("password1");
  example1.origin = GURL("https://example11.com");
  example1.signon_realm = example1.origin.spec();
  SaveToPasswordStore(example1);

  PasswordForm example2;
  example2.username_value = base::ASCIIToUTF16("user2");
  example2.password_value = base::ASCIIToUTF16("password2");
  example2.origin = GURL("https://example12.com");
  example2.signon_realm = example2.origin.spec();
  SaveToPasswordStore(example2);
}

// Saves two example blacklisted forms in the store.
void SaveExampleBlacklistedForms() {
  PasswordForm blacklisted1;
  blacklisted1.origin = GURL("https://exclude1.com");
  blacklisted1.signon_realm = blacklisted1.origin.spec();
  blacklisted1.blacklisted_by_user = true;
  SaveToPasswordStore(blacklisted1);

  PasswordForm blacklisted2;
  blacklisted2.origin = GURL("https://exclude2.com");
  blacklisted2.signon_realm = blacklisted2.origin.spec();
  blacklisted2.blacklisted_by_user = true;
  SaveToPasswordStore(blacklisted2);
}

// Removes all credentials stored.
void ClearPasswordStore() {
  GetPasswordStore()->RemoveLoginsCreatedBetween(base::Time(), base::Time(),
                                                 base::Closure());
  TestStoreConsumer consumer;
  GREYAssert(consumer.GetStoreResults().empty(),
             @"PasswordStore was not cleared.");
}

// Opens the passwords page from the NTP. It requires no menus to be open.
void OpenPasswordSettings() {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsMenuPasswordsButton()];
  // The settings page requested results from PasswordStore. Make sure they
  // have already been delivered by posting a task to PasswordStore's
  // background task runner and waits until it is finished. Because the
  // background task runner is sequenced, this means that previously posted
  // tasks are also finished when this function exits.
  TestStoreConsumer consumer;
  consumer.GetStoreResults();
}

// Tap Edit in any settings view.
void TapEdit() {
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      performAction:grey_tap()];
}

// Creates a PasswordForm with |index| being part of the username, password,
// origin and realm.
PasswordForm CreateSampleFormWithIndex(int index) {
  PasswordForm form;
  form.username_value =
      base::ASCIIToUTF16(base::StringPrintf("concrete username %03d", index));
  form.password_value =
      base::ASCIIToUTF16(base::StringPrintf("concrete password %03d", index));
  form.origin = GURL(base::StringPrintf("https://www%03d.example.com", index));
  form.signon_realm = form.origin.spec();
  return form;
}

}  // namespace

// Various tests for the Save Passwords section of the settings.
@interface PasswordsSettingsTestCase : ChromeTestCase
@end

@implementation PasswordsSettingsTestCase

- (void)tearDown {
  // Snackbars triggered by tests stay up for a limited time even if the
  // settings get closed. Ensure that they are closed to avoid interference with
  // other tests.
  [MDCSnackbarManager
      dismissAndCallCompletionBlocksWithCategory:@"PasswordsSnackbarCategory"];

  ClearPasswordStore();

  [super tearDown];
}

// Verifies the UI elements are accessible on the Passwords page.
- (void)testAccessibilityOnPasswords {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();
  chrome_test_util::VerifyAccessibilityForCurrentScreen();

  TapEdit();
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Inspect "password details" view.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to copy a password provide appropriate feedback,
// both when reauthentication succeeds and when it fails.
- (void)testCopyPasswordToast {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();

  // Check the snackbar in case of successful reauthentication.
  mock_reauthentication_module.shouldSucceed = YES;
  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      performAction:grey_tap()];

  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE);
  // The tap checks the existence of the snackbar and also closes it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbarLabel)]
      performAction:grey_tap()];

  // Check the snackbar in case of failed reauthentication.
  mock_reauthentication_module.shouldSucceed = NO;
  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      performAction:grey_tap()];

  snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE);
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

// Checks that an attempt to show a password provides an appropriate feedback
// when reauthentication succeeds.
- (void)testShowPasswordToastAuthSucceeded {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();

  // Check the snackbar in case of successful reauthentication.
  mock_reauthentication_module.shouldSucceed = YES;
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];

  // Check that the password is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"concrete password")]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that an attempt to show a password provides an appropriate feedback
// when reauthentication fails.
- (void)testShowPasswordToastAuthFailed {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();

  // Check the snackbar in case of failed reauthentication.
  mock_reauthentication_module.shouldSucceed = NO;
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];

  // Check that the password is not displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(@"concrete password")]
      assertWithMatcher:grey_nil()];

  // Note that there is supposed to be no message (cf. the case of the copy
  // button, which does need one). The reason is that the password not being
  // shown is enough of a message that the action failed.

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
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(CopyUsernameButton())
      performAction:grey_tap()];
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
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(CopySiteButton())
      performAction:grey_tap()];
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE);
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
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      performAction:grey_tap()];

  // Tap the alert's Delete button to confirm. Check accessibilityTrait to
  // differentiate against the above DeleteButton()-matching element, which is
  // has UIAccessibilityTraitSelected.
  // TODO(crbug.com/751311): Revisit and check if there is a better solution to
  // match the Delete button.
  id<GREYMatcher> deleteConfirmationButton = grey_allOf(
      ButtonWithAccessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_DELETION)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected)), nil);
  [[EarlGrey selectElementWithMatcher:deleteConfirmationButton]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Check that the current view is now the list view, by locating the header
  // of the list of passwords.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SETTINGS_PASSWORDS_SAVED_HEADING)),
                            grey_accessibilityTrait(UIAccessibilityTraitHeader),
                            nullptr)] assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  TestStoreConsumer consumer;
  GREYAssert(consumer.GetStoreResults().empty(),
             @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Edit button is visible and disabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];

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
  SaveExamplePasswordForm();
  // Save duplicate of the previously saved form to be deleted at the same time.
  // This entry is considered duplicated because it maps to the same sort key
  // as the previous one.
  PasswordForm exampleDuplicate;
  exampleDuplicate.username_value = base::ASCIIToUTF16("concrete username");
  exampleDuplicate.password_value = base::ASCIIToUTF16("concrete password");
  exampleDuplicate.origin = GURL("https://example.com/example");
  exampleDuplicate.signon_realm = exampleDuplicate.origin.spec();
  SaveToPasswordStore(exampleDuplicate);

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      performAction:grey_tap()];

  // Tap the alert's Delete button to confirm. Check accessibilityTrait to
  // differentiate against the above DeleteButton()-matching element, which is
  // has UIAccessibilityTraitSelected.
  // TODO(crbug.com/751311): Revisit and check if there is a better solution to
  // match the Delete button.
  id<GREYMatcher> deleteConfirmationButton = grey_allOf(
      ButtonWithAccessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_DELETION)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected)), nil);
  [[EarlGrey selectElementWithMatcher:deleteConfirmationButton]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Check that the current view is now the list view, by locating the header
  // of the list of passwords.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SETTINGS_PASSWORDS_SAVED_HEADING)),
                            grey_accessibilityTrait(UIAccessibilityTraitHeader),
                            nullptr)] assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  TestStoreConsumer consumer;
  GREYAssert(consumer.GetStoreResults().empty(),
             @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Edit button is visible and disabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a blacklisted form from password details view goes
// back to the list-of-passwords view which doesn't display that form anymore.
- (void)testBlacklistedFormDeletionInDetailView {
  // Save blacklisted form to be deleted later.
  PasswordForm blacklisted;
  blacklisted.origin = GURL("https://blacklisted.com");
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.blacklisted_by_user = true;
  SaveToPasswordStore(blacklisted);

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"blacklisted.com") performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      performAction:grey_tap()];

  // Tap the alert's Delete button to confirm. Check accessibilityTrait to
  // differentiate against the above DeleteButton()-matching element, which is
  // has UIAccessibilityTraitSelected.
  // TODO(crbug.com/751311): Revisit and check if there is a better solution to
  // match the Delete button.
  id<GREYMatcher> deleteConfirmationButton = grey_allOf(
      ButtonWithAccessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_CONFIRM_PASSWORD_DELETION)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected)), nil);
  [[EarlGrey selectElementWithMatcher:deleteConfirmationButton]
      performAction:grey_tap()];

  // Wait until the alert and the detail view are dismissed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Check that the current view is now the list view, by locating the header
  // of the list of passwords.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SETTINGS_PASSWORDS_EXCEPTIONS_HEADING)),
                            grey_accessibilityTrait(UIAccessibilityTraitHeader),
                            nullptr)] assertWithMatcher:grey_notNil()];

  // Verify that the deletion was propagated to the PasswordStore.
  TestStoreConsumer consumer;
  GREYAssert(consumer.GetStoreResults().empty(),
             @"Stored password was not removed from PasswordStore.");

  // Also verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"secret.com")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Finally, verify that the Edit button is visible and disabled, because there
  // are no other password entries left for deletion via the "Edit" mode.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a password from password details can be cancelled.
- (void)testCancelDeletionInDetailView {
  // Save form to be deleted later.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      performAction:grey_tap()];

  // Tap the alert's Cancel button to cancel.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   ButtonWithAccessibilityLabel(
                                       l10n_util::GetNSString(
                                           IDS_IOS_CANCEL_PASSWORD_DELETION)),
                                   grey_interactable(), nullptr)]
      performAction:grey_tap()];

  // Check that the current view is still the detail view, by locating the Copy
  // button.
  [[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the deletion did not happen.
  TestStoreConsumer consumer;
  GREYAssertEqual(1u, consumer.GetStoreResults().size(),
                  @"Stored password was removed from PasswordStore.");

  // Go back to the list view and verify that the password is still in the
  // list.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that if the list view is in edit mode, the "Save Passwords" switch is
// disabled and the details password view is not accessible on tapping the
// entries.
- (void)testEditMode {
  // Save a form to have something to tap on.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  TapEdit();

  // Check that the "Save Passwords" switch is disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::LegacySettingsSwitchCell(
                                   @"savePasswordsItem_switch", YES, NO)]
      assertWithMatcher:grey_notNil()];

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Check that the current view is not the detail view, by failing to locate
  // the Copy button.
  [[EarlGrey selectElementWithMatcher:CopyPasswordButton()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that attempts to copy the site via the context menu item provide an
// appropriate feedback.
- (void)testCopySiteMenuItem {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the site cell to display the context menu.
  [GetInteractionForPasswordDetailItem(grey_accessibilityLabel(
      @"https://example.com/")) performAction:grey_tap()];

  // Tap the context menu item for copying.
  [[EarlGrey selectElementWithMatcher:PopUpMenuItemWithLabel(
                                          IDS_IOS_SETTINGS_SITE_COPY_MENU_ITEM)]
      performAction:grey_tap()];

  // Check the snackbar.
  NSString* snackbarLabel =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE);
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

// Checks that attempts to copy the username via the context menu item provide
// an appropriate feedback.
- (void)testCopyUsernameMenuItem {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the username cell to display the context menu.
  [GetInteractionForPasswordDetailItem(
      grey_accessibilityLabel(@"concrete username")) performAction:grey_tap()];

  // Tap the context menu item for copying.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_USERNAME_COPY_MENU_ITEM)]
      performAction:grey_tap()];

  // Check the snackbar.
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

// Checks that attempts to copy the password via the context menu item provide
// an appropriate feedback.
- (void)testCopyPasswordMenuItem {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the password cell to display the context menu.
  [GetInteractionForPasswordDetailItem(grey_text(@"•••••••••••••••••"))
      performAction:grey_tap()];

  // Make sure to capture the reauthentication module in a variable until the
  // end of the test, otherwise it might get deleted too soon and break the
  // functionality of copying and viewing passwords.
  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();
  mock_reauthentication_module.shouldSucceed = YES;

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

// Checks that attempts to show and hide the password via the context menu item
// provide an appropriate feedback.
- (void)testShowHidePasswordMenuItem {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  // Tap the password cell to display the context menu.
  [GetInteractionForPasswordDetailItem(grey_text(@"•••••••••••••••••"))
      performAction:grey_tap()];

  // Make sure to capture the reauthentication module in a variable until the
  // end of the test, otherwise it might get deleted too soon and break the
  // functionality of copying and viewing passwords.
  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();
  mock_reauthentication_module.shouldSucceed = YES;

  // Tap the context menu item for showing.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_PASSWORD_SHOW_MENU_ITEM)]
      performAction:grey_tap()];

  // Tap the password cell to display the context menu again, and to check that
  // the password was unmasked.
  [GetInteractionForPasswordDetailItem(
      grey_accessibilityLabel(@"concrete password")) performAction:grey_tap()];

  // Tap the context menu item for hiding.
  [[EarlGrey
      selectElementWithMatcher:PopUpMenuItemWithLabel(
                                   IDS_IOS_SETTINGS_PASSWORD_HIDE_MENU_ITEM)]
      performAction:grey_tap()];

  // Check that the password is masked again.
  [GetInteractionForPasswordDetailItem(grey_text(@"•••••••••••••••••"))
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that federated credentials have no password but show the federation.
- (void)testFederated {
  PasswordForm federated;
  federated.username_value = base::ASCIIToUTF16("federated username");
  federated.origin = GURL("https://example.com");
  federated.signon_realm = federated.origin.spec();
  federated.federation_origin =
      url::Origin::Create(GURL("https://famous.provider.net"));
  SaveToPasswordStore(federated);

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, federated username")
      performAction:grey_tap()];

  // Check that the Site, Username, Federation and Delete Saved Password
  // sections are there.
  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordDetailItem(UsernameHeader())
      assertWithMatcher:grey_notNil()];
  // For federation check both the section header and content.
  [GetInteractionForPasswordDetailItem(FederationHeader())
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordDetailItem(grey_text(@"famous.provider.net"))
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_notNil()];

  // Check that the password is not present.
  [GetInteractionForPasswordDetailItem(PasswordHeader())
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks the order of the elements in the detail view layout for a
// non-federated, non-blacklisted credential.
- (void)testLayoutNormal {
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  id<GREYMatcher> siteCell = grey_accessibilityLabel(@"https://example.com/");
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_layout(@[ Below() ], SiteHeader())];
  [GetInteractionForPasswordDetailItem(CopySiteButton())
      assertWithMatcher:grey_layout(@[ Below() ], siteCell)];

  [GetInteractionForPasswordDetailItem(UsernameHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopySiteButton())];
  id<GREYMatcher> usernameCell = grey_accessibilityLabel(@"concrete username");
  [GetInteractionForPasswordDetailItem(usernameCell)
      assertWithMatcher:grey_layout(@[ Below() ], UsernameHeader())];
  [GetInteractionForPasswordDetailItem(CopyUsernameButton())
      assertWithMatcher:grey_layout(@[ Below() ], usernameCell)];

  [GetInteractionForPasswordDetailItem(PasswordHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopyUsernameButton())];
  id<GREYMatcher> passwordCell = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL));
  [GetInteractionForPasswordDetailItem(passwordCell)
      assertWithMatcher:grey_layout(@[ Below() ], PasswordHeader())];
  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      assertWithMatcher:grey_layout(@[ Below() ], passwordCell)];
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      assertWithMatcher:grey_layout(@[ Below() ], CopyPasswordButton())];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_layout(@[ Below() ], ShowPasswordButton())];

  // Check that the federation block is not present. Match directly to also
  // catch the case where the block would be present but not currently visible
  // due to the scrolling state.
  [[EarlGrey selectElementWithMatcher:FederationHeader()]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks the order of the elements in the detail view layout for a blacklisted
// credential.
- (void)testLayoutBlacklisted {
  PasswordForm blacklisted;
  blacklisted.origin = GURL("https://example.com");
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.blacklisted_by_user = true;
  SaveToPasswordStore(blacklisted);

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com") performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  id<GREYMatcher> siteCell = grey_accessibilityLabel(@"https://example.com/");
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_layout(@[ Below() ], SiteHeader())];
  [GetInteractionForPasswordDetailItem(CopySiteButton())
      assertWithMatcher:grey_layout(@[ Below() ], siteCell)];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_layout(@[ Below() ], CopySiteButton())];

  // Check that the other blocks are not present. Match directly to also catch
  // the case where those blocks would be present but not currently visible due
  // to the scrolling state.
  [[EarlGrey selectElementWithMatcher:UsernameHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:PasswordHeader()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:FederationHeader()]
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
  PasswordForm federated;
  federated.username_value = base::ASCIIToUTF16("federated username");
  federated.origin = GURL("https://example.com");
  federated.signon_realm = federated.origin.spec();
  federated.federation_origin =
      url::Origin::Create(GURL("https://famous.provider.net"));
  SaveToPasswordStore(federated);

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, federated username")
      performAction:grey_tap()];

  [GetInteractionForPasswordDetailItem(SiteHeader())
      assertWithMatcher:grey_notNil()];
  id<GREYMatcher> siteCell = grey_accessibilityLabel(@"https://example.com/");
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_layout(@[ Below() ], SiteHeader())];
  [GetInteractionForPasswordDetailItem(CopySiteButton())
      assertWithMatcher:grey_layout(@[ Below() ], siteCell)];

  [GetInteractionForPasswordDetailItem(UsernameHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopySiteButton())];
  id<GREYMatcher> usernameCell = grey_accessibilityLabel(@"federated username");
  [GetInteractionForPasswordDetailItem(usernameCell)
      assertWithMatcher:grey_layout(@[ Below() ], UsernameHeader())];
  [GetInteractionForPasswordDetailItem(CopyUsernameButton())
      assertWithMatcher:grey_layout(@[ Below() ], usernameCell)];

  [GetInteractionForPasswordDetailItem(FederationHeader())
      assertWithMatcher:grey_layout(@[ Below() ], CopyUsernameButton())];
  id<GREYMatcher> federationCell = grey_text(@"famous.provider.net");
  [GetInteractionForPasswordDetailItem(federationCell)
      assertWithMatcher:grey_layout(@[ Below() ], FederationHeader())];

  [GetInteractionForPasswordDetailItem(DeleteButton())
      assertWithMatcher:grey_layout(@[ Below() ], federationCell)];

  // Check that the password is not present. Match directly to also catch the
  // case where the password header would be present but not currently visible
  // due to the scrolling state.
  [[EarlGrey selectElementWithMatcher:PasswordHeader()]
      assertWithMatcher:grey_nil()];

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
  SaveExamplePasswordForm();

  PasswordForm blacklisted;
  blacklisted.origin = GURL("https://blacklisted.com");
  blacklisted.signon_realm = blacklisted.origin.spec();
  blacklisted.blacklisted_by_user = true;
  SaveToPasswordStore(blacklisted);

  OpenPasswordSettings();

  // Toggle the "Save Passwords" control off and back on and check that stored
  // items are still present.
  constexpr BOOL kExpectedState[] = {YES, NO};
  for (BOOL expected_state : kExpectedState) {
    // Toggle the switch. It is located near the top, so if not interactable,
    // try scrolling up.
    [GetInteractionForListItem(chrome_test_util::LegacySettingsSwitchCell(
                                   @"savePasswordsItem_switch", expected_state),
                               kGREYDirectionUp)
        performAction:chrome_test_util::TurnSettingsSwitchOn(!expected_state)];
    // Check the stored items. Scroll down if needed.
    [GetInteractionForPasswordEntry(@"example.com, concrete username")
        assertWithMatcher:grey_notNil()];
    [GetInteractionForPasswordEntry(@"blacklisted.com")
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
  OpenPasswordSettings();

  // Toggle the "Save Passwords" control off and back on and check the
  // preferences.
  constexpr BOOL kExpectedState[] = {YES, NO};
  for (BOOL expected_initial_state : kExpectedState) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::LegacySettingsSwitchCell(
                                     @"savePasswordsItem_switch",
                                     expected_initial_state)]
        performAction:chrome_test_util::TurnSettingsSwitchOn(
                          !expected_initial_state)];
    ios::ChromeBrowserState* browserState =
        chrome_test_util::GetOriginalBrowserState();
    const bool expected_final_state = !expected_initial_state;
    GREYAssertEqual(expected_final_state,
                    browserState->GetPrefs()->GetBoolean(
                        password_manager::prefs::kCredentialsEnableService),
                    @"State of the UI toggle differs from real preferences.");
  }

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that deleting a password from the list view works.
- (void)testDeletionInListView {
  // Save a password to be deleted later.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  TapEdit();

  // Select password entry to be removed.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

  // Verify that the deletion was propagated to the PasswordStore.
  TestStoreConsumer consumer;
  GREYAssert(consumer.GetStoreResults().empty(),
             @"Stored password was not removed from PasswordStore.");
  // Verify that the removed password is no longer in the list.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that an attempt to copy a password provides appropriate feedback when
// reauthentication cannot be attempted.
- (void)testCopyPasswordToastNoReauth {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();

  mock_reauthentication_module.canAttempt = NO;
  [GetInteractionForPasswordDetailItem(CopyPasswordButton())
      performAction:grey_tap()];

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that an attempt to view a password provides appropriate feedback when
// reauthentication cannot be attempted.
- (void)testShowPasswordToastNoReauth {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModule();

  mock_reauthentication_module.canAttempt = NO;
  [GetInteractionForPasswordDetailItem(ShowPasswordButton())
      performAction:grey_tap()];

  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
  NSString* learnHow =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW);
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   learnHow)] performAction:grey_tap()];
  // Check the sub menu is closed due to the help article.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  GREYAssertTrue(error, @"The settings back button is still displayed");

  // Check the settings page is closed.
  error = nil;
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  GREYAssertTrue(error, @"The settings page is still displayed");
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
- (void)testManyPasswords {
  // Enough just to ensure filling more than one page on all devices.
  constexpr int kPasswordsCount = 15;

  // Send the passwords to the queue to be added to the PasswordStore.
  for (int i = 1; i <= kPasswordsCount; ++i) {
    GetPasswordStore()->AddLogin(CreateSampleFormWithIndex(i));
  }

  // Use TestStoreConsumer::GetStoreResults to wait for the background storing
  // task to complete and to verify that the passwords have been stored.
  TestStoreConsumer consumer;
  GREYAssertEqual(kPasswordsCount, consumer.GetStoreResults().size(),
                  @"Unexpected PasswordStore results.");

  OpenPasswordSettings();

  // Aim at an entry almost at the end of the list.
  constexpr int kRemoteIndex = kPasswordsCount - 2;
  // The scrolling in GetInteractionForPasswordEntry has too fine steps to
  // reach the desired part of the list quickly. The following gives it a head
  // start of almost the desired position, counting 30 points per entry and
  // aiming 3 entries before |kRemoteIndex|.
  constexpr int kJump = (kRemoteIndex - 3) * 30;
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   @"SavePasswordsCollectionViewController")]
      performAction:grey_scrollInDirection(kGREYDirectionDown, kJump)];
  [GetInteractionForPasswordEntry([NSString
      stringWithFormat:@"www%03d.example.com, concrete username %03d",
                       kRemoteIndex, kRemoteIndex]) performAction:grey_tap()];

  // Check that the detail view loaded correctly by verifying the site content.
  id<GREYMatcher> siteCell = grey_accessibilityLabel([NSString
      stringWithFormat:@"https://www%03d.example.com/", kRemoteIndex]);
  [GetInteractionForPasswordDetailItem(siteCell)
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks that if all passwords are deleted in the list view, the disabled Edit
// button replaces the Done button.
- (void)testEditButtonUpdateOnDeletion {
  // Save a password to be deleted later.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  TapEdit();

  // Select password entry to be removed.
  [GetInteractionForPasswordEntry(@"example.com, concrete username")
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

  // Verify that the Edit button is visible and disabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON)]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];

  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Opens a page with password input, focuses it, clocks "Show All" in the
// keyboard accessory and verifies that the password list is presented.
- (void)testOpenSettingsFromManualFallback {
  // Saving a form is needed for using the "password details" view.
  SaveExamplePasswordForm();

  const GURL kPasswordURL(web::test::HttpServer::MakeUrl("http://form/"));
  std::map<GURL, std::string> responses;
  responses[kPasswordURL] = "<input id='password' type='password'>";
  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:kPasswordURL];

  // Focus the password field.
  // Brings up the keyboard by tapping on one of the form's field.
  [[EarlGrey
      selectElementWithMatcher:web::WebViewInWebState(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:web::WebViewTapElement(
                        chrome_test_util::GetCurrentWebState(),
                        ElementSelector::ElementSelectorId("password"))];

  // Wait until the keyboard shows up before tapping.
  id<GREYMatcher> showAll = grey_allOf(
      grey_accessibilityLabel(@"Show All\u2026"), grey_interactable(), nil);
  GREYCondition* condition =
      [GREYCondition conditionWithName:@"Wait for the keyboard to show up."
                                 block:^BOOL {
                                   NSError* error = nil;
                                   [[EarlGrey selectElementWithMatcher:showAll]
                                       assertWithMatcher:grey_notNil()
                                                   error:&error];
                                   return (error == nil);
                                 }];
  GREYAssert(
      [condition waitWithTimeout:base::test::ios::kWaitForUIElementTimeout],
      @"No keyboard with 'Show All' button showed up.");
  [[EarlGrey selectElementWithMatcher:showAll] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          @"example.com, concrete username")]
      assertWithMatcher:grey_notNil()];
}

// Test export flow
- (void)testExportFlow {
  // Saving a form is needed for exporting passwords.
  SaveExamplePasswordForm();

  OpenPasswordSettings();

  MockReauthenticationModule* mock_reauthentication_module =
      SetUpAndReturnMockReauthenticationModuleForExport();
  mock_reauthentication_module.shouldSucceed = YES;

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_EXPORT_PASSWORDS)]
      performAction:grey_tap()];

  // Tap the alert's Export passwords... button to confirm. Check
  // accessibilityTrait to differentiate against the above matching element,
  // which has UIAccessibilityTraitSelected.
  // TODO(crbug.com/751311): Revisit and check if there is a better solution to
  // match the button.
  id<GREYMatcher> exportConfirmationButton = grey_allOf(
      ButtonWithAccessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitSelected)), nil);
  [[EarlGrey selectElementWithMatcher:exportConfirmationButton]
      performAction:grey_tap()];

  // Wait until the alerts are dismissed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Check that export button is disabled
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_EXPORT_PASSWORDS)]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];

  if (IsIPadIdiom()) {
    // Tap outside the activity view to dismiss it, because it is not
    // accompanied by a "Cancel" button on iPad.
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_EXPORT_PASSWORDS)] performAction:grey_tap()];
  } else {
    // Tap on the "Cancel" button accompanying the activity view to dismiss it.
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     ButtonWithAccessibilityLabel(@"Cancel"),
                                     grey_interactable(), nullptr)]
        performAction:grey_tap()];
  }

  // Wait until the activity view is dismissed.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Check that export button is re-enabled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_EXPORT_PASSWORDS)]
      assertWithMatcher:grey_not(grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled))];
}

// Test that user can type text in search field and that it filters out the
// passwords and blacklisted items.
- (void)testSearchPasswords {
  SaveExamplePasswordForms();
  SaveExampleBlacklistedForms();

  OpenPasswordSettings();

  [GetInteractionForPasswordEntry(@"example11.com, user1")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"exclude1.com")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"exclude2.com")
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      performAction:grey_typeText(@"2")];

  [GetInteractionForPasswordEntry(@"example11.com, user1")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"exclude1.com")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"exclude2.com")
      assertWithMatcher:grey_notNil()];
}

// Test search and delete all passwords and blacklisted items.
- (void)testSearchAndDeleteAllPasswords {
  SaveExamplePasswordForms();
  SaveExampleBlacklistedForms();

  OpenPasswordSettings();

  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      performAction:grey_typeText(@"u\n")];

  TapEdit();

  // Select all.
  [GetInteractionForPasswordEntry(@"example11.com, user1")
      performAction:grey_tap()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      performAction:grey_tap()];
  [GetInteractionForPasswordEntry(@"exclude1.com") performAction:grey_tap()];
  [GetInteractionForPasswordEntry(@"exclude2.com") performAction:grey_tap()];

  // Delete them.
  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

  // All should be gone.
  [GetInteractionForPasswordEntry(@"example11.com, user1")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"exclude1.com")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"exclude2.com")
      assertWithMatcher:grey_nil()];
}

// Test that user can't search passwords while in edit mode.
- (void)testCantSearchPasswordsWhileInEditMode {
  SaveExamplePasswordForms();

  OpenPasswordSettings();
  TapEdit();

  // Verify search bar is disabled.
  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Test that the user can edit a password that is part of search results.
- (void)testCanEditPasswordsFromASearch {
  SaveExamplePasswordForms();
  OpenPasswordSettings();

  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      performAction:grey_typeText(@"2")];

  TapEdit();

  // Select password entry to be edited.
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      performAction:grey_tap()];

  // Delete it
  [[EarlGrey selectElementWithMatcher:DeleteButtonAtBottom()]
      performAction:grey_tap()];

  // Filter results in nothing.
  [GetInteractionForPasswordEntry(@"example11.com, user1")
      assertWithMatcher:grey_nil()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      assertWithMatcher:grey_nil()];

  // Get out of edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Remove filter search term.
  [[EarlGrey selectElementWithMatcher:SearchTextField()]
      performAction:grey_clearText()];

  // Only password 1 should show.
  [GetInteractionForPasswordEntry(@"example11.com, user1")
      assertWithMatcher:grey_notNil()];
  [GetInteractionForPasswordEntry(@"example12.com, user2")
      assertWithMatcher:grey_nil()];
}

@end
