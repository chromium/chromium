// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/i18n/time_formatting.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/password_manager/core/browser/passkey_credential.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/cells/table_view_stacked_details_item.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/credential_details.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_table_view_controller+Testing.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_details/password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/common/ui/reauthentication/mock_reauthentication_module.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using ::password_manager::CredentialUIEntry;
using ::password_manager::PasskeyCredential;
using ::password_manager::PasswordForm;

constexpr char kExampleCom[] = "http://www.example.com/";
constexpr char kAndroid[] = "android://hash@com.example.my.app";
constexpr char kUsername[] = "test@egmail.com";
constexpr char kDisplayName[] = "FirstName LastName";
constexpr char kPassword[] = "test";
constexpr char kNote[] = "note";

NSString* HTTPWebsite() {
  return base::SysUTF8ToNSString(kExampleCom);
}

NSString* Username() {
  return base::SysUTF8ToNSString(kUsername);
}

NSString* DisplayName() {
  return base::SysUTF8ToNSString(kDisplayName);
}

}  // namespace

// Test class that conforms to PasswordDetailsHanler in order to test the
// presenter methods are called correctly.
@interface FakePasswordDetailsHandler : NSObject <PasswordDetailsHandler>

@property(nonatomic, assign) BOOL deletionCalled;

@property(nonatomic, assign) BOOL deletionCalledOnCompromisedPassword;

@property(nonatomic, assign) BOOL editingCalled;

@property(nonatomic, assign) BOOL passwordCopiedByUserCalled;

@end

@implementation FakePasswordDetailsHandler

- (void)passwordDetailsTableViewControllerWasDismissed {
}

- (void)dismissPasswordDetailsTableViewController {
}

- (void)showCredentialDeleteDialogWithCredentialDetails:
            (CredentialDetails*)password
                                             anchorView:(UIView*)anchorView {
  self.deletionCalled = YES;
  self.deletionCalledOnCompromisedPassword = password.isCompromised;
}

- (void)moveCredentialToAccountStore:(CredentialDetails*)password
                          anchorView:(UIView*)anchorView
                     movedCompletion:(void (^)())movedCompletion {
}

- (void)showPasswordEditDialogWithOrigin:(NSString*)origin {
  self.editingCalled = YES;
}

- (void)onAllPasswordsDeleted {
}

- (void)onShareButtonPressed {
}

@end

// Test class that conforms to PasswordDetailsViewControllerDelegate in order
// to test the delegate methods are called correctly.
@interface FakePasswordDetailsDelegate
    : NSObject <PasswordDetailsTableViewControllerDelegate>

@property(nonatomic, strong) CredentialDetails* credential;

@property(nonatomic, assign) BOOL dismissWarningCalled;

@property(nonatomic, assign) BOOL restoreWarningCalled;

@end

@implementation FakePasswordDetailsDelegate

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
             didEditCredentialDetails:(CredentialDetails*)credential
                      withOldUsername:(NSString*)oldUsername
                   oldUserDisplayName:(NSString*)oldUserDisplayName
                          oldPassword:(NSString*)oldPassword
                              oldNote:(NSString*)oldNote {
  self.credential = credential;
}

- (void)didFinishEditingCredentialDetails {
}

- (BOOL)isUsernameReused:(NSString*)newUsername forDomain:(NSString*)domain {
  return NO;
}

- (void)dismissWarningForPassword:(CredentialDetails*)password {
  self.dismissWarningCalled = YES;
}

- (void)restoreWarningForCurrentPassword {
  self.restoreWarningCalled = YES;
}

@end

@interface FakeSnackbarImplementation : NSObject <SnackbarCommands>

@property(nonatomic, copy) NSString* snackbarMessage;

@end

@implementation FakeSnackbarImplementation

- (void)showSnackbarMessage:(SnackbarMessage*)message {
  self.snackbarMessage = message.title;
}

- (void)showSnackbarMessageOverBrowserToolbar:(SnackbarMessage*)message {
  self.snackbarMessage = message.title;
}

- (void)showSnackbarMessage:(SnackbarMessage*)message
             withHapticType:(UINotificationFeedbackType)type {
  self.snackbarMessage = message.title;
}

- (void)showSnackbarMessage:(SnackbarMessage*)message
               bottomOffset:(CGFloat)offset {
  self.snackbarMessage = message.title;
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  self.snackbarMessage = messageText;
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
        buttonAccessibilityHint:(NSString*)buttonAccesibilityHint
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  self.snackbarMessage = messageText;
}

- (void)showSnackbarMessageAfterDismissingKeyboard:(NSString*)messageText {
}

- (void)dismissAllSnackbars {
}

@end

// Unit tests for PasswordIssuesTableViewController.
class PasswordDetailsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  PasswordDetailsTableViewControllerTest() {
    handler_ = [[FakePasswordDetailsHandler alloc] init];
    delegate_ = [[FakePasswordDetailsDelegate alloc] init];
    reauthentication_module_ = [[MockReauthenticationModule alloc] init];
    reauthentication_module_.expectedResult = ReauthenticationResult::kSuccess;
    snack_bar_ = [[FakeSnackbarImplementation alloc] init];
  }

  LegacyChromeTableViewController* InstantiateController() override {
    PasswordDetailsTableViewController* controller =
        [[PasswordDetailsTableViewController alloc] init];
    controller.handler = handler_;
    controller.delegate = delegate_;
    controller.reauthModule = reauthentication_module_;
    controller.snackbarHandler = snack_bar_;
    return controller;
  }

  void SetPassword(std::string website = kExampleCom,
                   std::string username = kUsername,
                   std::string password = kPassword,
                   std::string note = kNote,
                   bool is_compromised = false,
                   bool is_muted = false,
                   DetailsContext context = DetailsContext::kPasswordSettings) {
    std::vector<std::string> websites = {website};
    SetPassword(websites, username, password, note, is_compromised, is_muted,
                context);
  }

  void SetPassword(const std::vector<std::string>& websites,
                   std::string username = kUsername,
                   std::string password = kPassword,
                   std::string note = kNote,
                   bool is_compromised = false,
                   bool is_muted = false,
                   DetailsContext context = DetailsContext::kPasswordSettings) {
    std::vector<PasswordForm> forms;
    for (const std::string& website : websites) {
      auto form = PasswordForm();
      form.signon_realm = website;
      form.username_value = base::ASCIIToUTF16(username);
      form.password_value = base::ASCIIToUTF16(password);
      form.url = GURL(website);
      form.action = GURL(website + "/action");
      form.username_element = u"email";
      form.scheme = PasswordForm::Scheme::kHtml;
      form.notes = {password_manager::PasswordNote(base::ASCIIToUTF16(note),
                                                   base::Time::Now())};
      forms.push_back(std::move(form));
    }

    NSMutableArray<CredentialDetails*>* passwords = [NSMutableArray array];
    CredentialDetails* passwordDetails =
        [[CredentialDetails alloc] initWithCredential:CredentialUIEntry(forms)];
    passwordDetails.context = context;
    passwordDetails.compromised = is_compromised;
    passwordDetails.muted = is_muted;
    [passwords addObject:passwordDetails];
    [passwords_controller() setCredentials:passwords andTitle:nil];
  }

  void SetFederatedPassword() {
    SetCredentialType(CredentialTypeFederation);
    auto form = PasswordForm();
    form.username_value = u"test@egmail.com";
    form.url = GURL(u"http://www.example.com/");
    form.signon_realm = form.url.spec();
    form.federation_origin = url::SchemeHostPort(GURL(kExampleCom));
    NSMutableArray<CredentialDetails*>* passwords = [NSMutableArray array];
    CredentialDetails* password =
        [[CredentialDetails alloc] initWithCredential:CredentialUIEntry(form)];
    [passwords addObject:password];
    [passwords_controller() setCredentials:passwords andTitle:nil];
  }

  // Creates a passkey, adds it to the view controller and returns the passkey's
  // creation time.
  base::Time SetPasskey(std::string website = "www.example.com/",
                        std::string username = kUsername,
                        std::string display_name = kDisplayName,
                        base::Time creation_time = base::Time::Now()) {
    PasskeyCredential::Source source =
        password_manager::PasskeyCredential::Source::kGooglePasswordManager;
    PasskeyCredential::RpId rp_id(website);
    PasskeyCredential::CredentialId credential_id({'c', 'r', 'e', 'd', 'e', 'n',
                                                   't', 'i', 'a', 'l', '_', 'i',
                                                   'd', '_', '0', '1'});
    PasskeyCredential::UserId user_id({'u', 's', 'e', 'r', '_', 'i', 'd', '1'});
    PasskeyCredential::Username passkey_username(username);
    PasskeyCredential::DisplayName passkey_display_name(display_name);
    PasskeyCredential passkeyCredential(source, rp_id, credential_id, user_id,
                                        passkey_username, passkey_display_name,
                                        creation_time);
    NSMutableArray<CredentialDetails*>* passkeys = [NSMutableArray array];
    CredentialDetails* passkey = [[CredentialDetails alloc]
        initWithCredential:CredentialUIEntry(passkeyCredential)];
    [passkeys addObject:passkey];
    [passwords_controller() setCredentials:passkeys andTitle:nil];
    return creation_time;
  }

  void SetBlockedOrigin() {
    SetCredentialType(CredentialTypeBlocked);
    auto form = PasswordForm();
    form.url = GURL(kExampleCom);
    form.blocked_by_user = true;
    form.signon_realm = form.url.spec();
    NSMutableArray<CredentialDetails*>* passwords = [NSMutableArray array];
    CredentialDetails* password =
        [[CredentialDetails alloc] initWithCredential:CredentialUIEntry(form)];
    [passwords addObject:password];
    [passwords_controller() setCredentials:passwords andTitle:nil];
  }

  void CheckEditCellText(NSString* expected_text, int section, int item) {
    TableViewTextEditItem* cell =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(section, item));
    EXPECT_NSEQ(expected_text, cell.textFieldValue);
  }

  void CheckEditCellMultiLineText(NSString* expected_text,
                                  int section,
                                  int item) {
    TableViewMultiLineTextEditItem* cell =
        static_cast<TableViewMultiLineTextEditItem*>(
            GetTableViewItem(section, item));
    EXPECT_NSEQ(expected_text, cell.text);
  }

  void CheckStackedDetailsCellDetails(NSArray<NSString*>* expected_details,
                                      int section,
                                      int item) {
    TableViewStackedDetailsItem* cell_item =
        static_cast<TableViewStackedDetailsItem*>(
            GetTableViewItem(section, item));

    EXPECT_NSEQ(expected_details, cell_item.detailTexts);
  }

  void SetEditCellText(NSString* text, int section, int item) {
    TableViewTextEditItem* cell =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(section, item));
    cell.textFieldValue = text;
  }

  void SetEditCellMultiLineText(NSString* text, int section, int item) {
    TableViewMultiLineTextEditItem* cell =
        static_cast<TableViewMultiLineTextEditItem*>(
            GetTableViewItem(section, item));
    cell.text = text;
  }

  void CheckDetailItemTextWithId(int expected_detail_text_id,
                                 int section,
                                 int item) {
    SettingsImageDetailTextItem* cell =
        static_cast<SettingsImageDetailTextItem*>(
            GetTableViewItem(section, item));
    EXPECT_NSEQ(l10n_util::GetNSString(expected_detail_text_id),
                cell.detailText);
  }

  FakePasswordDetailsHandler* handler() { return handler_; }
  FakePasswordDetailsDelegate* delegate() { return delegate_; }
  MockReauthenticationModule* reauth() { return reauthentication_module_; }
  FakeSnackbarImplementation* snack_bar() {
    return (FakeSnackbarImplementation*)snack_bar_;
  }

  void CheckCopyWebsites(const std::vector<std::string>& websites,
                         NSString* expected_pasteboard,
                         NSString* expected_snackbar_message) {
    base::HistogramTester histogram_tester;
    SetPassword(websites);

    base::RunLoop run_loop;
    base::RunLoop* run_loop_ptr = &run_loop;

    PasswordDetailsTableViewController* password_details =
        base::apple::ObjCCastStrict<PasswordDetailsTableViewController>(
            controller());

    [password_details tableView:password_details.tableView
        didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];

    [password_details copyPasswordDetailsHelper:PasswordDetailsItemTypeWebsite
                                     completion:^{
                                       run_loop_ptr->Quit();
                                     }];

    run_loop.Run();

    UIPasteboard* general_pasteboard = [UIPasteboard generalPasteboard];
    EXPECT_NSEQ(expected_pasteboard, general_pasteboard.string);
    EXPECT_NSEQ(expected_snackbar_message, snack_bar().snackbarMessage);
  }

  void SetCredentialType(CredentialType credentialType) {
    credential_type_ = credentialType;
  }

  PasswordDetailsTableViewController* passwords_controller() {
    return static_cast<PasswordDetailsTableViewController*>(controller());
  }

 private:
  id snack_bar_;
  FakePasswordDetailsHandler* handler_ = nil;
  FakePasswordDetailsDelegate* delegate_ = nil;
  MockReauthenticationModule* reauthentication_module_ = nil;
  CredentialType credential_type_ = CredentialTypeRegularPassword;
  base::test::TaskEnvironment task_environment_;
};

// Tests that password is displayed properly.
TEST_F(PasswordDetailsTableViewControllerTest, TestPassword) {
  SetPassword();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(4, NumberOfItemsInSection(0));
  CheckStackedDetailsCellDetails(@[ HTTPWebsite() ], 0, 0);
  CheckEditCellText(Username(), 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);
  CheckEditCellMultiLineText(@"note", 0, 3);
}

// Tests that passkey is displayed properly.
TEST_F(PasswordDetailsTableViewControllerTest, TestPasskey) {
  base::Time creation_time = SetPasskey();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(4, NumberOfItemsInSection(0));
  CheckStackedDetailsCellDetails(@[ @"https://www.example.com/" ], 0, 0);
  CheckEditCellText(DisplayName(), 0, 1);
  CheckEditCellText(Username(), 0, 2);
  CheckEditCellText(
      l10n_util::GetNSStringF(IDS_IOS_PASSKEY_CREATION_DATE,
                              base::TimeFormatShortDate(creation_time)),
      0, 3);
}

// Tests that correct metrics is reported after adding a note.
TEST_F(PasswordDetailsTableViewControllerTest, TestAddingPasswordWithNote) {
  base::HistogramTester histogram_tester;

  SetPassword(kExampleCom, kUsername, kPassword, /*note=*/"");
  [passwords_controller() editButtonPressed];
  EXPECT_TRUE(passwords_controller().tableView.editing);

  SetEditCellMultiLineText(@"note", 0, 3);
  [passwords_controller() editButtonPressed];

  EXPECT_FALSE(passwords_controller().tableView.editing);
  EXPECT_NSEQ(@"note", delegate().credential.note);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings2",
      password_manager::metrics_util::PasswordNoteAction::
          kNoteAddedInEditDialog,
      1);
}

// Tests that correct metrics is reported after editing a note.
TEST_F(PasswordDetailsTableViewControllerTest, TestEditingPasswordWithNote) {
  base::HistogramTester histogram_tester;

  SetPassword();
  [passwords_controller() editButtonPressed];
  EXPECT_TRUE(passwords_controller().tableView.editing);

  SetEditCellMultiLineText(@"new_note", 0, 3);
  [passwords_controller() editButtonPressed];

  EXPECT_FALSE(passwords_controller().tableView.editing);
  EXPECT_NSEQ(@"new_note", delegate().credential.note);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings2",
      password_manager::metrics_util::PasswordNoteAction::
          kNoteEditedInEditDialog,
      1);
}

// Tests that correct metrics is reported after editing a password without a
// note change.
TEST_F(PasswordDetailsTableViewControllerTest, TestRemovingPasswordWithNote) {
  base::HistogramTester histogram_tester;

  SetPassword();
  [passwords_controller() editButtonPressed];
  EXPECT_TRUE(passwords_controller().tableView.editing);

  SetEditCellMultiLineText(@"", 0, 3);
  [passwords_controller() editButtonPressed];

  EXPECT_FALSE(passwords_controller().tableView.editing);
  EXPECT_NSEQ(@"", delegate().credential.note);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings2",
      password_manager::metrics_util::PasswordNoteAction::
          kNoteRemovedInEditDialog,
      1);
}

// Tests that correct metrics is reported after removing a note.
TEST_F(PasswordDetailsTableViewControllerTest,
       TestEditingPasswordWithoutNoteChange) {
  base::HistogramTester histogram_tester;

  SetPassword();
  [passwords_controller() editButtonPressed];
  EXPECT_TRUE(passwords_controller().tableView.editing);

  SetEditCellText(@"new_password", 0, 2);
  [passwords_controller() editButtonPressed];
  [passwords_controller() passwordEditingConfirmed];

  EXPECT_FALSE(passwords_controller().tableView.editing);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings2",
      password_manager::metrics_util::PasswordNoteAction::kNoteNotChanged, 1);
}

// Tests that compromised password is displayed properly.
TEST_F(PasswordDetailsTableViewControllerTest, TestCompromisedPassword) {
  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/true);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(7, NumberOfItemsInSection(0));
  CheckStackedDetailsCellDetails(@[ HTTPWebsite() ], 0, 0);
  CheckEditCellText(Username(), 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);
  CheckEditCellMultiLineText(@"note", 0, 3);

  CheckDetailItemTextWithId(
      IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED, 0, 4);
  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 0, 5);

  CheckTextCellTextWithId(IDS_IOS_DISMISS_WARNING, 0, 6);
}

// Tests that muted compromised password is displayed properly.
TEST_F(PasswordDetailsTableViewControllerTest, TestMutedCompromisedPassword) {
  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/false, /*is_muted=*/true,
              DetailsContext::kDismissedWarnings);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(7, NumberOfItemsInSection(0));
  CheckStackedDetailsCellDetails(@[ HTTPWebsite() ], 0, 0);
  CheckEditCellText(Username(), 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);
  CheckEditCellMultiLineText(@"note", 0, 3);

  CheckDetailItemTextWithId(
      IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED, 0, 4);
  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 0, 5);

  CheckTextCellTextWithId(IDS_IOS_RESTORE_WARNING, 0, 6);
}

// Tests the “Change Password on Website” button.
TEST_F(PasswordDetailsTableViewControllerTest, TestChangePasswordOnWebsite) {
  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/true);
  id applicationCommandsMock = OCMProtocolMock(@protocol(ApplicationCommands));
  passwords_controller().applicationHandler = applicationCommandsMock;

  TableViewModel* model = passwords_controller().tableViewModel;
  NSIndexPath* indexPath =
      [model indexPathForItemType:PasswordDetailsItemTypeChangePasswordButton];

  OCMExpect([applicationCommandsMock
      closePresentedViewsAndOpenURL:[OCMArg checkWithBlock:^BOOL(id value) {
        // This block verifies that the closePresentedViewsAndOpenURL
        // function is called with a URL argument which matches the initial URL
        // passed to the password form above. Information may have been appended
        // to the URL argument, so we only make sure it includes the initial
        // URL.
        return base::Contains(((OpenNewTabCommand*)value).URL.spec(),
                              kExampleCom);
      }]]);
  [passwords_controller() tableView:passwords_controller().tableView
            didSelectRowAtIndexPath:indexPath];
  EXPECT_OCMOCK_VERIFY(applicationCommandsMock);
}

// Tests the “Dismiss Warning” button.
TEST_F(PasswordDetailsTableViewControllerTest, TestDismissWarning) {
  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/true);
  EXPECT_FALSE(delegate().dismissWarningCalled);

  TableViewModel* model = passwords_controller().tableViewModel;
  NSIndexPath* indexPath =
      [model indexPathForItemType:PasswordDetailsItemTypeDismissWarningButton];
  [passwords_controller() tableView:passwords_controller().tableView
            didSelectRowAtIndexPath:indexPath];

  EXPECT_TRUE(delegate().dismissWarningCalled);
}

// Tests the “Restore Warning” button.
TEST_F(PasswordDetailsTableViewControllerTest, TestRestoreWarning) {
  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/false, /*is_muted=*/true,
              DetailsContext::kDismissedWarnings);
  EXPECT_FALSE(delegate().restoreWarningCalled);

  TableViewModel* model = passwords_controller().tableViewModel;
  NSIndexPath* indexPath =
      [model indexPathForItemType:PasswordDetailsItemTypeRestoreWarningButton];
  [passwords_controller() tableView:passwords_controller().tableView
            didSelectRowAtIndexPath:indexPath];

  EXPECT_TRUE(delegate().restoreWarningCalled);
}

// Tests that password is shown/hidden.
TEST_F(PasswordDetailsTableViewControllerTest, TestShowHidePassword) {
  SetPassword();
  NSIndexPath* indexOfPassword;
  CheckEditCellText(kMaskedPassword, 0, 2);
  indexOfPassword = [NSIndexPath indexPathForRow:2 inSection:0];

  TableViewTextEditCell* textFieldCell =
      base::apple::ObjCCastStrict<TableViewTextEditCell>([controller()
                      tableView:controller().tableView
          cellForRowAtIndexPath:indexOfPassword]);
  EXPECT_TRUE(textFieldCell);
  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(@"test", 0, 2);

  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that password was revealed during editing.
TEST_F(PasswordDetailsTableViewControllerTest, TestPasswordShownDuringEditing) {
  SetPassword();

  CheckEditCellText(kMaskedPassword, 0, 2);

  [passwords_controller() editButtonPressed];
  EXPECT_TRUE(passwords_controller().tableView.editing);

  CheckEditCellText(@"test", 0, 2);

  [passwords_controller() editButtonPressed];
  EXPECT_FALSE(passwords_controller().tableView.editing);
  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that delete button trigger showing password delete dialog.
TEST_F(PasswordDetailsTableViewControllerTest, TestPasswordDelete) {
  SetPassword();

  EXPECT_FALSE(handler().deletionCalled);
  [passwords_controller() editButtonPressed];
  [passwords_controller() tableView:passwords_controller().tableView
            didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:4
                                                       inSection:0]];
  EXPECT_TRUE(handler().deletionCalled);
  EXPECT_FALSE(handler().deletionCalledOnCompromisedPassword);
}

// Tests compromised password deletion trigger showing password delete dialog.
TEST_F(PasswordDetailsTableViewControllerTest, TestCompromisedPasswordDelete) {
  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/true);

  EXPECT_FALSE(handler().deletionCalled);
  [passwords_controller() editButtonPressed];
  [passwords_controller()
                    tableView:passwords_controller().tableView
      didSelectRowAtIndexPath:[NSIndexPath
                                  indexPathForRow:NumberOfItemsInSection(0) - 1
                                        inSection:0]];
  EXPECT_TRUE(handler().deletionCalled);
  EXPECT_TRUE(handler().deletionCalledOnCompromisedPassword);
}

// Tests password editing. User confirmed this action.
TEST_F(PasswordDetailsTableViewControllerTest, TestEditPasswordConfirmed) {
  SetPassword();

  [passwords_controller() editButtonPressed];
  EXPECT_FALSE(handler().editingCalled);
  EXPECT_FALSE(delegate().credential);
  EXPECT_TRUE(passwords_controller().tableView.editing);

  SetEditCellText(@"new_password", 0, 2);

  [passwords_controller() editButtonPressed];
  EXPECT_TRUE(handler().editingCalled);

  [passwords_controller() passwordEditingConfirmed];
  EXPECT_TRUE(delegate().credential);

  EXPECT_NSEQ(@"new_password", delegate().credential.password);
  EXPECT_FALSE(passwords_controller().tableView.editing);
}

// Tests password editing. User cancelled this action.
TEST_F(PasswordDetailsTableViewControllerTest, TestEditPasswordCancel) {
  SetPassword();

  [passwords_controller() editButtonPressed];
  EXPECT_FALSE(delegate().credential);
  EXPECT_TRUE(passwords_controller().tableView.editing);

  SetEditCellText(@"new_password", 0, 2);

  [passwords_controller() editButtonPressed];
  EXPECT_FALSE(delegate().credential);
  EXPECT_TRUE(passwords_controller().tableView.editing);
}

// Tests android compromised credential is displayed without change password
// button.
TEST_F(PasswordDetailsTableViewControllerTest,
       TestAndroidCompromisedCredential) {
  SetPassword(kAndroid, kUsername, kPassword, kNote, /*is_compromised=*/true);

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(6, NumberOfItemsInSection(0));

  CheckStackedDetailsCellDetails(@[ @"app.my.example.com" ], 0, 0);
  CheckEditCellText(Username(), 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);
  CheckEditCellMultiLineText(@"note", 0, 3);

  CheckDetailItemTextWithId(
      IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED, 0, 4);

  CheckTextCellTextWithId(IDS_IOS_DISMISS_WARNING, 0, 5);
}

// Tests federated credential is shown without password value and editing
// doesn't require reauth.
TEST_F(PasswordDetailsTableViewControllerTest, TestFederatedCredential) {
  SetFederatedPassword();

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));

  CheckStackedDetailsCellDetails(@[ HTTPWebsite() ], 0, 0);
  CheckEditCellText(Username(), 0, 1);
  CheckEditCellText(@"www.example.com", 0, 2);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  [passwords_controller() editButtonPressed];
  EXPECT_TRUE(passwords_controller().tableView.editing);
}

// Tests blocked website is shown without password and username values and
// editing doesn't require reauth.
TEST_F(PasswordDetailsTableViewControllerTest, TestBlockedOrigin) {
  SetBlockedOrigin();

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));

  CheckStackedDetailsCellDetails(@[ HTTPWebsite() ], 0, 0);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  [passwords_controller() editButtonPressed];
  EXPECT_TRUE(passwords_controller().tableView.editing);
}

// Tests copy website works as intended.
TEST_F(PasswordDetailsTableViewControllerTest, CopySite) {
  // TODO(crbug.com/440059889): Re-enable the test on iOS26.
  if (base::ios::IsRunningOnIOS26OrLater()) {
    return;
  }
  std::vector<std::string> websites = {kExampleCom};
  NSString* expected_pasteboard = HTTPWebsite();
  CheckCopyWebsites(
      websites, expected_pasteboard,
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITES_WERE_COPIED_MESSAGE));
}

// Tests copy multiple websites works as intended.
TEST_F(PasswordDetailsTableViewControllerTest, CopySites) {
  std::vector<std::string> websites = {kExampleCom, "http://example.com/"};
  CheckCopyWebsites(
      websites, @"http://www.example.com/ http://example.com/",
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITES_WERE_COPIED_MESSAGE));
}

// Tests copy username works as intended.
TEST_F(PasswordDetailsTableViewControllerTest, CopyUsername) {
  base::HistogramTester histogram_tester;
  SetPassword();

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  [passwords_controller() tableView:passwords_controller().tableView
            didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1
                                                       inSection:0]];
  [passwords_controller()
      copyPasswordDetailsHelper:PasswordDetailsItemTypeUsername
                     completion:^{
                       run_loop_ptr->Quit();
                     }];

  run_loop.Run();

  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(Username(), generalPasteboard.string);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE),
      snack_bar().snackbarMessage);

  EXPECT_FALSE(handler().passwordCopiedByUserCalled);
}

// Tests copy password works as intended when reauth was successful.
TEST_F(PasswordDetailsTableViewControllerTest, CopyPasswordSuccess) {
  base::HistogramTester histogram_tester;
  SetPassword();

  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;

  [passwords_controller() tableView:passwords_controller().tableView
            didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:2
                                                       inSection:0]];
  [passwords_controller()
      copyPasswordDetailsHelper:PasswordDetailsItemTypePassword
                     completion:^{
                       run_loop_ptr->Quit();
                     }];

  run_loop.Run();

  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(@"test", generalPasteboard.string);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE),
      snack_bar().snackbarMessage);
}

// Tests error histogram is emitted when we fail copying a field.
TEST_F(PasswordDetailsTableViewControllerTest, CopyDetailsFailedEmitted) {
  base::HistogramTester histogram_tester;

  [passwords_controller() copyPasswordDetailsHelper:NSIntegerMax
                                         completion:nil];

  EXPECT_FALSE(handler().passwordCopiedByUserCalled);
}

// Tests that hidden passkeys are ordered after non-hidden.
TEST_F(PasswordDetailsTableViewControllerTest, SortsCredentialsByHiddenState) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kCredentialProviderSignalAPI);

  PasskeyCredential::Source source =
      PasskeyCredential::Source::kGooglePasswordManager;
  PasskeyCredential::RpId rp_id("www.example.com");
  base::Time creation_time = base::Time::Now();
  base::Time last_used_time = base::Time::Now();
  PasskeyCredential hidden_passkey_credential(
      source, rp_id, PasskeyCredential::CredentialId({'c', 'r', 'e', 'd', '1'}),
      PasskeyCredential::UserId({'u', 's', 'e', 'r', '1'}),
      PasskeyCredential::Username("username1"),
      PasskeyCredential::DisplayName("display_name1"), creation_time,
      last_used_time,
      /*hidden=*/true);
  PasskeyCredential passkey_credential(
      source, rp_id, PasskeyCredential::CredentialId({'c', 'r', 'e', 'd', '2'}),
      PasskeyCredential::UserId({'u', 's', 'e', 'r', '2'}),
      PasskeyCredential::Username("username2"),
      PasskeyCredential::DisplayName("display_name2"), creation_time,
      last_used_time,
      /*hidden=*/false);

  NSArray<CredentialDetails*>* passkeys = @[
    [[CredentialDetails alloc]
        initWithCredential:CredentialUIEntry(hidden_passkey_credential)],
    [[CredentialDetails alloc]
        initWithCredential:CredentialUIEntry(passkey_credential)]
  ];
  [passwords_controller() setCredentials:passkeys andTitle:nil];

  EXPECT_EQ(NumberOfSections(), 2);
  CheckStackedDetailsCellDetails(@[ @"https://www.example.com/" ], 0, 0);
  CheckEditCellText(@"display_name2", 0, 1);
  CheckEditCellText(@"username2", 0, 2);
  CheckEditCellText(
      l10n_util::GetNSStringF(IDS_IOS_PASSKEY_CREATION_DATE,
                              base::TimeFormatShortDate(creation_time)),
      0, 3);
  CheckStackedDetailsCellDetails(@[ @"https://www.example.com/" ], 1, 0);
  CheckEditCellText(@"display_name1", 1, 1);
  CheckEditCellText(@"username1", 1, 2);
  CheckEditCellText(l10n_util::GetNSString(IDS_IOS_PASSKEY_DOES_NOT_WORK), 1,
                    3);
}
