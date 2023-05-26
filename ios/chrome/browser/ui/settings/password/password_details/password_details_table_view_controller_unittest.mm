// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"

#import <memory>

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/cells/table_view_stacked_details_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller+private.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr char kExampleCom[] = "http://www.example.com/";
constexpr char kAndroid[] = "android://hash@com.example.my.app";
constexpr char kUsername[] = "test@egmail.com";
constexpr char kPassword[] = "test";
constexpr char kNote[] = "note";
}

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

- (void)showPasscodeDialogForReason:(PasscodeDialogReason)reason {
}

- (void)showPasswordDeleteDialogWithPasswordDetails:(PasswordDetails*)password
                                         anchorView:(UIView*)anchorView {
  self.deletionCalled = YES;
  self.deletionCalledOnCompromisedPassword = password.isCompromised;
}

- (void)moveCredentialToAccountStore:(PasswordDetails*)password
                          anchorView:(UIView*)anchorView
                     movedCompletion:(void (^)())movedCompletion {
}

- (void)showPasswordEditDialogWithOrigin:(NSString*)origin {
  self.editingCalled = YES;
}

- (void)onPasswordCopiedByUser {
  self.passwordCopiedByUserCalled = YES;
}

- (void)onAllPasswordsDeleted {
}

@end

// Test class that conforms to PasswordDetailsViewControllerDelegate in order
// to test the delegate methods are called correctly.
@interface FakePasswordDetailsDelegate
    : NSObject <PasswordDetailsTableViewControllerDelegate>

@property(nonatomic, strong) PasswordDetails* password;

@property(nonatomic, assign) BOOL dismissWarningCalled;

@property(nonatomic, assign) BOOL restoreWarningCalled;

@end

@implementation FakePasswordDetailsDelegate

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password
                      withOldUsername:(NSString*)oldUsername
                          oldPassword:(NSString*)oldPassword
                              oldNote:(NSString*)oldNote {
  self.password = password;
}

- (void)didFinishEditingPasswordDetails {
}

- (BOOL)isUsernameReused:(NSString*)newUsername forDomain:(NSString*)domain {
  return NO;
}

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
                didAddPasswordDetails:(NSString*)username
                             password:(NSString*)password {
}

- (void)checkForDuplicates:(NSString*)username {
}

- (void)showExistingCredential:(NSString*)username {
}

- (void)didCancelAddPasswordDetails {
}

- (void)setWebsiteURL:(NSString*)website {
}

- (BOOL)isURLValid {
  return YES;
}

- (BOOL)isTLDMissing {
  return NO;
}

- (void)dismissWarningForPassword:(PasswordDetails*)password {
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

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message {
}

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
             withHapticType:(UINotificationFeedbackType)type {
}

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
               bottomOffset:(CGFloat)offset {
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  self.snackbarMessage = messageText;
}

@end

// Unit tests for PasswordIssuesTableViewController.
class PasswordDetailsTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  PasswordDetailsTableViewControllerTest() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kPasswordsGrouping);
    handler_ = [[FakePasswordDetailsHandler alloc] init];
    delegate_ = [[FakePasswordDetailsDelegate alloc] init];
    reauthentication_module_ = [[MockReauthenticationModule alloc] init];
    reauthentication_module_.expectedResult = ReauthenticationResult::kSuccess;
    snack_bar_ = [[FakeSnackbarImplementation alloc] init];
  }

  ChromeTableViewController* InstantiateController() override {
    PasswordDetailsTableViewController* controller =
        [[PasswordDetailsTableViewController alloc] init];
    controller.handler = handler_;
    controller.delegate = delegate_;
    controller.reauthModule = reauthentication_module_;
    controller.snackbarCommandsHandler = snack_bar_;
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
    std::vector<password_manager::PasswordForm> forms;
    for (const auto& website : websites) {
      auto form = password_manager::PasswordForm();
      form.signon_realm = website;
      form.username_value = base::ASCIIToUTF16(username);
      form.password_value = base::ASCIIToUTF16(password);
      form.url = GURL(website);
      form.action = GURL(website + "/action");
      form.username_element = u"email";
      form.scheme = password_manager::PasswordForm::Scheme::kHtml;
      form.notes = {password_manager::PasswordNote(base::ASCIIToUTF16(note),
                                                   base::Time::Now())};
      forms.push_back(std::move(form));
    }

    NSMutableArray<PasswordDetails*>* passwords = [NSMutableArray array];
    PasswordDetails* passwordDetails = [[PasswordDetails alloc]
        initWithCredential:password_manager::CredentialUIEntry(forms)];
    passwordDetails.context = context;
    passwordDetails.compromised = is_compromised;
    passwordDetails.muted = is_muted;
    [passwords addObject:passwordDetails];

    PasswordDetailsTableViewController* passwords_controller =
        static_cast<PasswordDetailsTableViewController*>(controller());
    [passwords_controller setPasswords:passwords andTitle:nil];
  }

  void SetFederatedPassword() {
    SetCredentialType(CredentialTypeFederation);
    auto form = password_manager::PasswordForm();
    form.username_value = u"test@egmail.com";
    form.url = GURL(u"http://www.example.com/");
    form.signon_realm = form.url.spec();
    form.federation_origin =
        url::Origin::Create(GURL("http://www.example.com/"));
    NSMutableArray<PasswordDetails*>* passwords = [NSMutableArray array];
    PasswordDetails* password = [[PasswordDetails alloc]
        initWithCredential:password_manager::CredentialUIEntry(form)];
    [passwords addObject:password];
    PasswordDetailsTableViewController* passwords_controller =
        static_cast<PasswordDetailsTableViewController*>(controller());
    [passwords_controller setPasswords:passwords andTitle:nil];
  }

  void SetBlockedOrigin() {
    SetCredentialType(CredentialTypeBlocked);
    auto form = password_manager::PasswordForm();
    form.url = GURL("http://www.example.com/");
    form.blocked_by_user = true;
    form.signon_realm = form.url.spec();
    NSMutableArray<PasswordDetails*>* passwords = [NSMutableArray array];
    PasswordDetails* password = [[PasswordDetails alloc]
        initWithCredential:password_manager::CredentialUIEntry(form)];
    [passwords addObject:password];
    PasswordDetailsTableViewController* passwords_controller =
        static_cast<PasswordDetailsTableViewController*>(controller());
    [passwords_controller setPasswords:passwords andTitle:nil];
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

    EXPECT_TRUE([expected_details isEqualToArray:cell_item.detailTexts]);
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

    PasswordDetailsTableViewController* password_details =
        base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
            controller());

    [password_details tableView:password_details.tableView
        didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
    UIMenuController* menu = [UIMenuController sharedMenuController];
    EXPECT_EQ(1u, menu.menuItems.count);
    [password_details copyPasswordDetails:menu];

    UIPasteboard* general_pasteboard = [UIPasteboard generalPasteboard];
    EXPECT_NSEQ(expected_pasteboard, general_pasteboard.string);
    EXPECT_NSEQ(expected_snackbar_message, snack_bar().snackbarMessage);
    // Verify that the error histogram was emitted to the success bucket.
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", false, 1);
  }

  void SetCredentialType(CredentialType credentialType) {
    credential_type_ = credentialType;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  id snack_bar_;
  FakePasswordDetailsHandler* handler_ = nil;
  FakePasswordDetailsDelegate* delegate_ = nil;
  MockReauthenticationModule* reauthentication_module_ = nil;
  CredentialType credential_type_ = CredentialTypeRegular;
};

class PasswordGroupingTest : public ::testing::WithParamInterface<bool>,
                             public PasswordDetailsTableViewControllerTest {
 protected:
  PasswordGroupingTest() {
    feature_list_.InitWithFeatureState(
        password_manager::features::kPasswordsGrouping, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that password is displayed properly.
TEST_F(PasswordDetailsTableViewControllerTest, TestPassword) {
  SetPassword();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));
  CheckStackedDetailsCellDetails(@[ @"http://www.example.com/" ], 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that password with note is displayed properly.
TEST_F(PasswordDetailsTableViewControllerTest, TestPasswordWithNote) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);

  SetPassword();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(4, NumberOfItemsInSection(0));

  CheckStackedDetailsCellDetails(@[ @"http://www.example.com/" ], 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);
  CheckEditCellMultiLineText(@"note", 0, 3);
}

// Tests that correct metrics is reported after adding a note.
TEST_F(PasswordDetailsTableViewControllerTest, TestAddingPasswordWithNote) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
  base::HistogramTester histogram_tester;

  SetPassword(kExampleCom, kUsername, kPassword, /*note=*/"");
  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  EXPECT_TRUE(passwordDetails.tableView.editing);

  SetEditCellMultiLineText(@"note", 0, 3);
  [passwordDetails editButtonPressed];

  EXPECT_FALSE(passwordDetails.tableView.editing);
  EXPECT_NSEQ(@"note", delegate().password.note);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings2",
      password_manager::metrics_util::PasswordNoteAction::
          kNoteAddedInEditDialog,
      1);
}

// Tests that correct metrics is reported after editing a note.
TEST_F(PasswordDetailsTableViewControllerTest, TestEditingPasswordWithNote) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
  base::HistogramTester histogram_tester;

  SetPassword();
  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  EXPECT_TRUE(passwordDetails.tableView.editing);

  SetEditCellMultiLineText(@"new_note", 0, 3);
  [passwordDetails editButtonPressed];

  EXPECT_FALSE(passwordDetails.tableView.editing);
  EXPECT_NSEQ(@"new_note", delegate().password.note);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings2",
      password_manager::metrics_util::PasswordNoteAction::
          kNoteEditedInEditDialog,
      1);
}

// Tests that correct metrics is reported after editing a password without a
// note change.
TEST_F(PasswordDetailsTableViewControllerTest, TestRemovingPasswordWithNote) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
  base::HistogramTester histogram_tester;

  SetPassword();
  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  EXPECT_TRUE(passwordDetails.tableView.editing);

  SetEditCellMultiLineText(@"", 0, 3);
  [passwordDetails editButtonPressed];

  EXPECT_FALSE(passwordDetails.tableView.editing);
  EXPECT_NSEQ(@"", delegate().password.note);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings2",
      password_manager::metrics_util::PasswordNoteAction::
          kNoteRemovedInEditDialog,
      1);
}

// Tests that correct metrics is reported after removing a note.
TEST_F(PasswordDetailsTableViewControllerTest,
       TestEditingPasswordWithoutNoteChange) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kPasswordNotesWithBackup);
  base::HistogramTester histogram_tester;

  SetPassword();
  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  EXPECT_TRUE(passwordDetails.tableView.editing);

  SetEditCellText(@"new_password", 0, 2);
  [passwordDetails editButtonPressed];
  [passwordDetails passwordEditingConfirmed];

  EXPECT_FALSE(passwordDetails.tableView.editing);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordNoteActionInSettings2",
      password_manager::metrics_util::PasswordNoteAction::kNoteNotChanged, 1);
}

// Tests that password is displayed properly with notes feature disabled.
TEST_F(PasswordDetailsTableViewControllerTest, TestPasswordWithNotesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(syncer::kPasswordNotesWithBackup);

  SetPassword();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));
  CheckStackedDetailsCellDetails(@[ @"http://www.example.com/" ], 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that a credential group is displayed properly.
TEST_F(PasswordDetailsTableViewControllerTest, TestMultipleWebsites) {
  std::vector<std::string> websites = {"http://www.example.com/",
                                       "http://example.com/"};
  SetPassword(websites);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));
  CheckStackedDetailsCellDetails(
      @[ @"http://www.example.com/", @"http://example.com/" ], 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that compromised password is displayed properly when
// kIOSPasswordCheckup feature is disabled.
TEST_F(PasswordDetailsTableViewControllerTest,
       TestCompromisedPasswordWithoutKIOSPasswordCheckup) {
  // Disable Password Checkup feature.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/true);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(5, NumberOfItemsInSection(0));
  CheckStackedDetailsCellDetails(@[ @"http://www.example.com/" ], 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);

  CheckDetailItemTextWithId(
      IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED, 0, 3);
  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 0, 4);
}

// Tests that compromised password is displayed properly when
// kIOSPasswordCheckup feature is enabled.
TEST_F(PasswordDetailsTableViewControllerTest,
       TestCompromisedPasswordWithKIOSPasswordCheckup) {
  // Enable Password Checkup feature.
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kIOSPasswordCheckup);

  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/true);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(6, NumberOfItemsInSection(0));
  CheckStackedDetailsCellDetails(@[ @"http://www.example.com/" ], 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);

  CheckDetailItemTextWithId(
      IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED, 0, 3);
  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 0, 4);

  CheckTextCellTextWithId(IDS_IOS_DISMISS_WARNING, 0, 5);
}

// Tests that muted compromised password is displayed properly.
// kIOSPasswordCheckup feature needs to be enabled.
TEST_F(PasswordDetailsTableViewControllerTest,
       TestMutedCompromisedPasswordWithKIOSPasswordCheckup) {
  // Enable Password Checkup feature.
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kIOSPasswordCheckup);

  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/false, /*is_muted=*/true,
              DetailsContext::kDismissedWarnings);
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(6, NumberOfItemsInSection(0));
  CheckStackedDetailsCellDetails(@[ @"http://www.example.com/" ], 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);

  CheckDetailItemTextWithId(
      IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED, 0, 3);
  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 0, 4);

  CheckTextCellTextWithId(IDS_IOS_RESTORE_WARNING, 0, 5);
}

// Tests the “Change Password on Website” button.
TEST_P(PasswordGroupingTest, TestChangePasswordOnWebsite) {
  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/true);
  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  id applicationCommandsMock = OCMProtocolMock(@protocol(ApplicationCommands));
  password_details.applicationCommandsHandler = applicationCommandsMock;

  TableViewModel* model = password_details.tableViewModel;
  NSIndexPath* indexPath =
      [model indexPathForItemType:PasswordDetailsItemTypeChangePasswordButton];

  OCMExpect([applicationCommandsMock
      closeSettingsUIAndOpenURL:[OCMArg checkWithBlock:^BOOL(id value) {
        // This block verifies that the closeSettingsUIAndOpenURL function is
        // called with a URL argument which matches the initial URL passed to
        // the password form above. Information may have been appended to the
        // URL argument, so we only make sure it includes the initial URL.
        return (((OpenNewTabCommand*)value).URL.spec().find(kExampleCom) !=
                std::string::npos);
      }]]);
  [password_details tableView:password_details.tableView
      didSelectRowAtIndexPath:indexPath];
  EXPECT_OCMOCK_VERIFY(applicationCommandsMock);
}

// Tests the “Dismiss Warning” button. kIOSPasswordCheckup feature needs to be
// enabled.
TEST_F(PasswordDetailsTableViewControllerTest, TestDismissWarning) {
  // Enable Password Checkup feature.
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kIOSPasswordCheckup);

  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/true);
  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  EXPECT_FALSE(delegate().dismissWarningCalled);

  TableViewModel* model = password_details.tableViewModel;
  NSIndexPath* indexPath =
      [model indexPathForItemType:PasswordDetailsItemTypeDismissWarningButton];
  [password_details tableView:password_details.tableView
      didSelectRowAtIndexPath:indexPath];

  EXPECT_TRUE(delegate().dismissWarningCalled);
}

// Tests the “Restore Warning” button. kIOSPasswordCheckup feature needs to be
// enabled.
TEST_F(PasswordDetailsTableViewControllerTest, TestRestoreWarning) {
  // Enable Password Checkup feature.
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kIOSPasswordCheckup);

  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/false, /*is_muted=*/true,
              DetailsContext::kDismissedWarnings);
  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  EXPECT_FALSE(delegate().restoreWarningCalled);

  TableViewModel* model = password_details.tableViewModel;
  NSIndexPath* indexPath =
      [model indexPathForItemType:PasswordDetailsItemTypeRestoreWarningButton];
  [password_details tableView:password_details.tableView
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
      base::mac::ObjCCastStrict<TableViewTextEditCell>([controller()
                      tableView:controller().tableView
          cellForRowAtIndexPath:indexOfPassword]);
  EXPECT_TRUE(textFieldCell);
  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(@"test", 0, 2);

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW),
      reauth().localizedReasonForAuthentication);

  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that passwords was not shown in case reauth failed.
TEST_F(PasswordDetailsTableViewControllerTest, TestShowPasswordReauthFailed) {
  SetPassword();

  CheckEditCellText(kMaskedPassword, 0, 2);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  NSIndexPath* indexOfPassword;
  indexOfPassword = [NSIndexPath indexPathForRow:2 inSection:0];

  TableViewTextEditCell* textFieldCell =
      base::mac::ObjCCastStrict<TableViewTextEditCell>([controller()
                      tableView:controller().tableView
          cellForRowAtIndexPath:indexOfPassword]);
  EXPECT_TRUE(textFieldCell);
  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that password was revealed during editing.
TEST_F(PasswordDetailsTableViewControllerTest, TestPasswordShownDuringEditing) {
  SetPassword();

  CheckEditCellText(kMaskedPassword, 0, 2);

  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [password_details editButtonPressed];
  EXPECT_TRUE(password_details.tableView.editing);

  CheckEditCellText(@"test", 0, 2);

  [password_details editButtonPressed];
  EXPECT_FALSE(password_details.tableView.editing);
  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that editing mode was not entered because reauth failed.
TEST_F(PasswordDetailsTableViewControllerTest, TestEditingReauthFailed) {
  SetPassword();

  CheckEditCellText(kMaskedPassword, 0, 2);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [password_details editButtonPressed];
  EXPECT_FALSE(password_details.tableView.editing);
  CheckEditCellText(kMaskedPassword, 0, 2);
}

// Tests that delete button trigger showing password delete dialog.
TEST_F(PasswordDetailsTableViewControllerTest, TestPasswordDelete) {
  SetPassword();

  EXPECT_FALSE(handler().deletionCalled);
  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [password_details editButtonPressed];
  [password_details tableView:password_details.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:3 inSection:0]];
  EXPECT_TRUE(handler().deletionCalled);
  EXPECT_FALSE(handler().deletionCalledOnCompromisedPassword);
}

// Tests compromised password deletion trigger showing password delete dialog.
TEST_F(PasswordDetailsTableViewControllerTest, TestCompromisedPasswordDelete) {
  SetPassword(kExampleCom, kUsername, kPassword, kNote,
              /*is_compromised=*/true);

  EXPECT_FALSE(handler().deletionCalled);
  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [password_details editButtonPressed];
  [password_details tableView:password_details.tableView
      didSelectRowAtIndexPath:[NSIndexPath
                                  indexPathForRow:NumberOfItemsInSection(0) - 1
                                        inSection:0]];
  EXPECT_TRUE(handler().deletionCalled);
  EXPECT_TRUE(handler().deletionCalledOnCompromisedPassword);
}

// Tests password editing. User confirmed this action.
TEST_F(PasswordDetailsTableViewControllerTest, TestEditPasswordConfirmed) {
  SetPassword();

  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [password_details editButtonPressed];
  EXPECT_FALSE(handler().editingCalled);
  EXPECT_FALSE(delegate().password);
  EXPECT_TRUE(password_details.tableView.editing);

  SetEditCellText(@"new_password", 0, 2);

  [password_details editButtonPressed];
  EXPECT_TRUE(handler().editingCalled);

  [password_details passwordEditingConfirmed];
  EXPECT_TRUE(delegate().password);

  EXPECT_NSEQ(@"new_password", delegate().password.password);
  EXPECT_FALSE(password_details.tableView.editing);
}

// Tests password editing. User cancelled this action.
TEST_F(PasswordDetailsTableViewControllerTest, TestEditPasswordCancel) {
  SetPassword();

  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [password_details editButtonPressed];
  EXPECT_FALSE(delegate().password);
  EXPECT_TRUE(password_details.tableView.editing);

  SetEditCellText(@"new_password", 0, 2);

  [password_details editButtonPressed];
  EXPECT_FALSE(delegate().password);
  EXPECT_TRUE(password_details.tableView.editing);
}

// Tests android compromised credential is displayed without change password
// button when kIOSPasswordCheckup feature is disabled.
TEST_F(PasswordDetailsTableViewControllerTest,
       TestAndroidCompromisedCredentialWithoutKIOSPasswordCheckup) {
  // Disable Password Checkup feature.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kIOSPasswordCheckup);

  SetPassword(kAndroid, kUsername, kPassword, kNote, /*is_compromised=*/true);

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(4, NumberOfItemsInSection(0));

  CheckStackedDetailsCellDetails(@[ @"com.example.my.app" ], 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);

  CheckDetailItemTextWithId(
      IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED, 0, 3);
}

// Tests android compromised credential is displayed without change password
// button when kIOSPasswordCheckup feature is enabled.
TEST_F(PasswordDetailsTableViewControllerTest,
       TestAndroidCompromisedCredentialWithKIOSPasswordCheckup) {
  // Enable Password Checkup feature.
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kIOSPasswordCheckup);

  SetPassword(kAndroid, kUsername, kPassword, kNote, /*is_compromised=*/true);

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(5, NumberOfItemsInSection(0));

  CheckStackedDetailsCellDetails(@[ @"com.example.my.app" ], 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(kMaskedPassword, 0, 2);

  CheckDetailItemTextWithId(
      IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED, 0, 3);

  CheckTextCellTextWithId(IDS_IOS_DISMISS_WARNING, 0, 4);
}

// Tests federated credential is shown without password value and editing
// doesn't require reauth.
TEST_F(PasswordDetailsTableViewControllerTest, TestFederatedCredential) {
  SetFederatedPassword();

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));

  CheckStackedDetailsCellDetails(@[ @"http://www.example.com/" ], 0, 0);
  CheckEditCellText(@"test@egmail.com", 0, 1);
  CheckEditCellText(@"www.example.com", 0, 2);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [password_details editButtonPressed];
  EXPECT_TRUE(password_details.tableView.editing);
}

// Tests blocked website is shown without password and username values and
// editing doesn't require reauth.
TEST_F(PasswordDetailsTableViewControllerTest, TestBlockedOrigin) {
  SetBlockedOrigin();

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));

  CheckStackedDetailsCellDetails(@[ @"http://www.example.com/" ], 0, 0);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [password_details editButtonPressed];
  EXPECT_TRUE(password_details.tableView.editing);
}

// Tests copy website works as intended.
TEST_F(PasswordDetailsTableViewControllerTest, CopySite) {
  std::vector<std::string> websites = {"http://www.example.com/"};
  NSString* expected_pasteboard = @"http://www.example.com/";
  // Test without password grouping.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        password_manager::features::kPasswordsGrouping);
    CheckCopyWebsites(
        websites, expected_pasteboard,
        l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE));
  }
  // Test with password grouping.
  {
    base::test::ScopedFeatureList feature_list(
        password_manager::features::kPasswordsGrouping);
    CheckCopyWebsites(
        websites, expected_pasteboard,
        l10n_util::GetNSString(IDS_IOS_SETTINGS_SITES_WERE_COPIED_MESSAGE));
  }
}

// Tests copy multiple websites works as intended.
TEST_F(PasswordDetailsTableViewControllerTest, CopySites) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kPasswordsGrouping);

  std::vector<std::string> websites = {"http://www.example.com/",
                                       "http://example.com/"};
  CheckCopyWebsites(
      websites, @"http://www.example.com/ http://example.com/",
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SITES_WERE_COPIED_MESSAGE));
}

// Tests copy username works as intended.
TEST_F(PasswordDetailsTableViewControllerTest, CopyUsername) {
  base::HistogramTester histogram_tester;
  SetPassword();
  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  [password_details tableView:password_details.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]];

  UIMenuController* menu = [UIMenuController sharedMenuController];
  EXPECT_EQ(1u, menu.menuItems.count);
  [password_details copyPasswordDetails:menu];

  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(@"test@egmail.com", generalPasteboard.string);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE),
      snack_bar().snackbarMessage);

  // Verify that the error histogram was emitted to the success bucket.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", false, 1);

  EXPECT_FALSE(handler().passwordCopiedByUserCalled);
}

// Tests copy password works as intended when reauth was successful.
TEST_F(PasswordDetailsTableViewControllerTest, CopyPasswordSuccess) {
  base::HistogramTester histogram_tester;
  SetPassword();

  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  [password_details tableView:password_details.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:2 inSection:0]];

  UIMenuController* menu = [UIMenuController sharedMenuController];
  EXPECT_EQ(1u, menu.menuItems.count);
  [password_details copyPasswordDetails:menu];

  EXPECT_TRUE(handler().passwordCopiedByUserCalled);

  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(@"test", generalPasteboard.string);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_COPY),
      reauth().localizedReasonForAuthentication);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_COPIED_MESSAGE),
      snack_bar().snackbarMessage);
  // Verify that the error histogram was emitted to the success bucket.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", false, 1);
}

// Tests copy password works as intended.
TEST_F(PasswordDetailsTableViewControllerTest, CopyPasswordFail) {
  base::HistogramTester histogram_tester;
  SetPassword();

  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  reauth().expectedResult = ReauthenticationResult::kFailure;
  [password_details tableView:password_details.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:2 inSection:0]];

  UIMenuController* menu = [UIMenuController sharedMenuController];
  EXPECT_EQ(1u, menu.menuItems.count);
  [password_details copyPasswordDetails:menu];

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE),
      snack_bar().snackbarMessage);

  // Verify that the error histogram was emitted to the success bucket.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", false, 1);

  EXPECT_FALSE(handler().passwordCopiedByUserCalled);
}

// Tests error histogram is emitted when we fail copying a field.
TEST_F(PasswordDetailsTableViewControllerTest, CopyDetailsFailedEmitted) {
  base::HistogramTester histogram_tester;

  PasswordDetailsTableViewController* password_details =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  // When no menu controller is passed, there's no way of knowing which field
  // should be copied to the pasteboard and thus copying should fail.
  [password_details copyPasswordDetails:nil];

  // Verify that the error histogram was emitted to the failure bucket.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", true, 1);

  EXPECT_FALSE(handler().passwordCopiedByUserCalled);
}

INSTANTIATE_TEST_SUITE_P(PasswordDetailsTableViewControllerTest,
                         PasswordGroupingTest,
                         ::testing::Bool());
