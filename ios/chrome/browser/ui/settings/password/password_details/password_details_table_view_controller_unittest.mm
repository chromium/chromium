// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"

#import <memory>

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/password_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr char kExampleCom[] = "http://www.example.com/";
constexpr char kAndroid[] = "android://hash@com.example.my.app";
constexpr char kUsername[] = "test@egmail.com";
constexpr char kPassword[] = "test";
}

@interface PasswordDetailsTableViewController (Test)
- (void)copyPasswordDetails:(id)sender;
@end

// Test class that conforms to PasswordDetailsHanler in order to test the
// presenter methods are called correctly.
@interface FakePasswordDetailsHandler : NSObject <PasswordDetailsHandler>

@property(nonatomic, assign) BOOL deletionCalled;

@property(nonatomic, assign) BOOL deletionCalledOnCompromisedPassword;

@property(nonatomic, assign) BOOL editingCalled;

@end

@implementation FakePasswordDetailsHandler

- (void)passwordDetailsTableViewControllerDidDisappear {
}

- (void)showPasscodeDialog {
}

- (void)showPasswordDeleteDialogWithOrigin:(NSString*)origin
                       compromisedPassword:(BOOL)compromisedPassword {
  self.deletionCalled = YES;
  self.deletionCalledOnCompromisedPassword = compromisedPassword;
}

- (void)showPasswordEditDialogWithOrigin:(NSString*)origin {
  self.editingCalled = YES;
}

@end

// Test class that conforms to PasswordDetailsViewControllerDelegate in order to
// test the delegate methods are called correctly.
@interface FakePasswordDetailsDelegate
    : NSObject <PasswordDetailsTableViewControllerDelegate>

@property(nonatomic, strong) PasswordDetails* password;

@end

@implementation FakePasswordDetailsDelegate

- (void)passwordDetailsViewController:
            (PasswordDetailsTableViewController*)viewController
               didEditPasswordDetails:(PasswordDetails*)password
                      withOldUsername:(NSString*)oldUsername
                       andOldPassword:(NSString*)oldPassword {
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

@end

@interface FakeSnackbarImplementation : NSObject <SnackbarCommands>

@property(nonatomic, assign) NSString* snackbarMessage;

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
    handler_ = [[FakePasswordDetailsHandler alloc] init];
    delegate_ = [[FakePasswordDetailsDelegate alloc] init];
    reauthentication_module_ = [[MockReauthenticationModule alloc] init];
    reauthentication_module_.expectedResult = ReauthenticationResult::kSuccess;
    snack_bar_ = [[FakeSnackbarImplementation alloc] init];
  }

  ChromeTableViewController* InstantiateController() override {
    PasswordDetailsTableViewController* controller =
        [[PasswordDetailsTableViewController alloc]
            initWithSyncingUserEmail:syncing_user_email_];
    controller.handler = handler_;
    controller.delegate = delegate_;
    controller.reauthModule = reauthentication_module_;
    controller.snackbarCommandsHandler = snack_bar_;
    return controller;
  }

  void SetPassword(std::string website = kExampleCom,
                   std::string username = kUsername,
                   std::string password = kPassword,
                   bool isCompromised = false) {
    auto form = password_manager::PasswordForm();
    form.signon_realm = website;
    form.username_value = base::ASCIIToUTF16(username);
    form.password_value = base::ASCIIToUTF16(password);
    form.url = GURL(website);
    form.action = GURL(website + "/action");
    form.username_element = u"email";
    form.scheme = password_manager::PasswordForm::Scheme::kHtml;

    NSMutableArray<PasswordDetails*>* passwords = [NSMutableArray array];
    PasswordDetails* passwordDetails = [[PasswordDetails alloc]
        initWithCredential:password_manager::CredentialUIEntry(form)];
    passwordDetails.compromised = isCompromised;
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

  void SetEditCellText(NSString* text, int section, int item) {
    TableViewTextEditItem* cell =
        static_cast<TableViewTextEditItem*>(GetTableViewItem(section, item));
    cell.textFieldValue = text;
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

  void SetCredentialType(CredentialType credentialType) {
    credential_type_ = credentialType;
  }

  void SetUserSyncingEmail(NSString* syncing_user_email) {
    syncing_user_email_ = syncing_user_email;
  }

 private:
  id snack_bar_;
  FakePasswordDetailsHandler* handler_ = nil;
  FakePasswordDetailsDelegate* delegate_ = nil;
  MockReauthenticationModule* reauthentication_module_ = nil;
  CredentialType credential_type_ = CredentialTypeRegular;
  NSString* syncing_user_email_ = nil;
};

// Tests that password is displayed properly.
TEST_F(PasswordDetailsTableViewControllerTest, TestPassword) {
  SetPassword();
  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  CheckEditCellText(@"http://www.example.com/", 0, 0);
  CheckEditCellText(@"test@egmail.com", 1, 0);
  CheckEditCellText(kMaskedPassword, 1, 1);
}

// Tests that compromised password is displayed properly.
TEST_F(PasswordDetailsTableViewControllerTest, TestCompromisedPassword) {
  SetPassword(kExampleCom, kUsername, kPassword, true);
  EXPECT_EQ(3, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  EXPECT_EQ(2, NumberOfItemsInSection(2));
  CheckEditCellText(@"http://www.example.com/", 0, 0);
  CheckEditCellText(@"test@egmail.com", 1, 0);
  CheckEditCellText(kMaskedPassword, 1, 1);

  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSEnablePasswordManagerBrandingUpdate)) {
    CheckDetailItemTextWithId(
        IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED, 2, 0);
    CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 2, 1);
  } else {
    CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 2, 0);
    CheckDetailItemTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION,
                              2, 1);
  }
}

// Tests that password is shown/hidden.
TEST_F(PasswordDetailsTableViewControllerTest, TestShowHidePassword) {
  SetPassword();
  NSIndexPath* indexOfPassword;
  CheckEditCellText(kMaskedPassword, 1, 1);
  indexOfPassword = [NSIndexPath indexPathForRow:1 inSection:1];

  TableViewTextEditCell* textFieldCell =
      base::mac::ObjCCastStrict<TableViewTextEditCell>([controller()
                      tableView:controller().tableView
          cellForRowAtIndexPath:indexOfPassword]);
  EXPECT_TRUE(textFieldCell);
  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(@"test", 1, 1);

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW),
      reauth().localizedReasonForAuthentication);

  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(kMaskedPassword, 1, 1);
}

// Tests that passwords was not shown in case reauth failed.
TEST_F(PasswordDetailsTableViewControllerTest, TestShowPasswordReauthFailed) {
  SetPassword();

  CheckEditCellText(kMaskedPassword, 1, 1);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  NSIndexPath* indexOfPassword;
  indexOfPassword = [NSIndexPath indexPathForRow:1 inSection:1];

  TableViewTextEditCell* textFieldCell =
      base::mac::ObjCCastStrict<TableViewTextEditCell>([controller()
                      tableView:controller().tableView
          cellForRowAtIndexPath:indexOfPassword]);
  EXPECT_TRUE(textFieldCell);
  [textFieldCell.identifyingIconButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  CheckEditCellText(kMaskedPassword, 1, 1);
}

// Tests that password was revealed during editing.
TEST_F(PasswordDetailsTableViewControllerTest, TestPasswordShownDuringEditing) {
  SetPassword();

  CheckEditCellText(kMaskedPassword, 1, 1);

  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  EXPECT_TRUE(passwordDetails.tableView.editing);

  CheckEditCellText(@"test", 1, 1);

  [passwordDetails editButtonPressed];
  EXPECT_FALSE(passwordDetails.tableView.editing);
  CheckEditCellText(kMaskedPassword, 1, 1);
}

// Tests that editing mode was not entered because reauth failed.
TEST_F(PasswordDetailsTableViewControllerTest, TestEditingReauthFailed) {
  SetPassword();

  CheckEditCellText(kMaskedPassword, 1, 1);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  EXPECT_FALSE(passwordDetails.tableView.editing);
  CheckEditCellText(kMaskedPassword, 1, 1);
}

// Tests that delete button trigger showing password delete dialog.
TEST_F(PasswordDetailsTableViewControllerTest, TestPasswordDelete) {
  SetPassword();

  EXPECT_FALSE(handler().deletionCalled);
  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  [[UIApplication sharedApplication]
      sendAction:passwordDetails.deleteButton.action
              to:passwordDetails.deleteButton.target
            from:nil
        forEvent:nil];
  EXPECT_TRUE(handler().deletionCalled);
  EXPECT_FALSE(handler().deletionCalledOnCompromisedPassword);
}

// Tests compromised password deletion trigger showing password delete dialog.
TEST_F(PasswordDetailsTableViewControllerTest, TestCompromisedPasswordDelete) {
  SetPassword(kExampleCom, kUsername, kPassword, true);

  EXPECT_FALSE(handler().deletionCalled);
  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  [[UIApplication sharedApplication]
      sendAction:passwordDetails.deleteButton.action
              to:passwordDetails.deleteButton.target
            from:nil
        forEvent:nil];
  EXPECT_TRUE(handler().deletionCalled);
  EXPECT_TRUE(handler().deletionCalledOnCompromisedPassword);
}

// Tests password editing. User confirmed this action.
TEST_F(PasswordDetailsTableViewControllerTest, TestEditPasswordConfirmed) {
  SetPassword();

  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  EXPECT_FALSE(handler().editingCalled);
  EXPECT_FALSE(delegate().password);
  EXPECT_TRUE(passwordDetails.tableView.editing);

  SetEditCellText(@"new_password", 1, 1);

  [passwordDetails editButtonPressed];
  EXPECT_TRUE(handler().editingCalled);

  [passwordDetails passwordEditingConfirmed];
  EXPECT_TRUE(delegate().password);

  EXPECT_NSEQ(@"new_password", delegate().password.password);
  EXPECT_FALSE(passwordDetails.tableView.editing);
}

// Tests password editing. User cancelled this action.
TEST_F(PasswordDetailsTableViewControllerTest, TestEditPasswordCancel) {
  SetPassword();

  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  EXPECT_FALSE(delegate().password);
  EXPECT_TRUE(passwordDetails.tableView.editing);

  SetEditCellText(@"new_password", 1, 1);

  [passwordDetails editButtonPressed];
  EXPECT_FALSE(delegate().password);
  EXPECT_TRUE(passwordDetails.tableView.editing);
}

// Tests android compromised credential is displayed without change password
// button.
TEST_F(PasswordDetailsTableViewControllerTest,
       TestAndroidCompromisedCredential) {
  SetPassword(kAndroid, kUsername, kPassword, true);

  EXPECT_EQ(3, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  EXPECT_EQ(1, NumberOfItemsInSection(2));

  CheckEditCellText(@"com.example.my.app", 0, 0);
  CheckEditCellText(@"test@egmail.com", 1, 0);
  CheckEditCellText(kMaskedPassword, 1, 1);

  if (base::FeatureList::IsEnabled(
          password_manager::features::
              kIOSEnablePasswordManagerBrandingUpdate)) {
    CheckDetailItemTextWithId(
        IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION_BRANDED, 2, 0);
  } else {
    CheckDetailItemTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD_DESCRIPTION,
                              2, 0);
  }
}

// Tests federated credential is shown without password value and editing
// doesn't require reauth.
TEST_F(PasswordDetailsTableViewControllerTest, TestFederatedCredential) {
  SetFederatedPassword();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(2, NumberOfItemsInSection(1));

  CheckEditCellText(@"http://www.example.com/", 0, 0);
  CheckEditCellText(@"test@egmail.com", 1, 0);
  CheckEditCellText(@"www.example.com", 1, 1);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  EXPECT_TRUE(passwordDetails.tableView.editing);
}

// Tests blocked website is shown without password and username values and
// editing doesn't require reauth.
TEST_F(PasswordDetailsTableViewControllerTest, TestBlockedOrigin) {
  SetBlockedOrigin();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(0, NumberOfItemsInSection(1));

  CheckEditCellText(@"http://www.example.com/", 0, 0);

  reauth().expectedResult = ReauthenticationResult::kFailure;
  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());
  [passwordDetails editButtonPressed];
  EXPECT_TRUE(passwordDetails.tableView.editing);
}

// Tests copy website works as intended.
TEST_F(PasswordDetailsTableViewControllerTest, CopySite) {
  base::HistogramTester histogram_tester;
  SetPassword();

  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  [passwordDetails tableView:passwordDetails.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  UIMenuController* menu = [UIMenuController sharedMenuController];
  EXPECT_EQ(1u, menu.menuItems.count);
  [passwordDetails copyPasswordDetails:menu];

  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(@"http://www.example.com/", generalPasteboard.string);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTINGS_SITE_WAS_COPIED_MESSAGE),
              snack_bar().snackbarMessage);
  // Verify that the error histogram was emitted to the success bucket.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", false, 1);
}

// Tests copy username works as intended.
TEST_F(PasswordDetailsTableViewControllerTest, CopyUsername) {
  base::HistogramTester histogram_tester;
  SetPassword();
  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  [passwordDetails tableView:passwordDetails.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:1]];

  UIMenuController* menu = [UIMenuController sharedMenuController];
  EXPECT_EQ(1u, menu.menuItems.count);
  [passwordDetails copyPasswordDetails:menu];

  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(@"test@egmail.com", generalPasteboard.string);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_USERNAME_WAS_COPIED_MESSAGE),
      snack_bar().snackbarMessage);

  // Verify that the error histogram was emitted to the success bucket.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", false, 1);
}

// Tests copy password works as intended when reauth was successful.
TEST_F(PasswordDetailsTableViewControllerTest, CopyPasswordSuccess) {
  base::HistogramTester histogram_tester;
  SetPassword();

  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  [passwordDetails tableView:passwordDetails.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:1]];

  UIMenuController* menu = [UIMenuController sharedMenuController];
  EXPECT_EQ(1u, menu.menuItems.count);
  [passwordDetails copyPasswordDetails:menu];

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

  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  reauth().expectedResult = ReauthenticationResult::kFailure;
  [passwordDetails tableView:passwordDetails.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:1]];

  UIMenuController* menu = [UIMenuController sharedMenuController];
  EXPECT_EQ(1u, menu.menuItems.count);
  [passwordDetails copyPasswordDetails:menu];

  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_WAS_NOT_COPIED_MESSAGE),
      snack_bar().snackbarMessage);

  // Verify that the error histogram was emitted to the success bucket.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", false, 1);
}

// Tests error histogram is emitted when we fail copying a field.
TEST_F(PasswordDetailsTableViewControllerTest, CopyDetailsFailedEmitted) {
  base::HistogramTester histogram_tester;

  PasswordDetailsTableViewController* passwordDetails =
      base::mac::ObjCCastStrict<PasswordDetailsTableViewController>(
          controller());

  // When no menu controller is passed, there's no way of knowing which field
  // should be copied to the pasteboard and thus copying should fail.
  [passwordDetails copyPasswordDetails:nil];

  // Verify that the error histogram was emitted to the failure bucket.
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.iOS.PasswordDetails.CopyDetailsFailed", true, 1);
}

// Tests that there are multiple sections in the edit view.
TEST_F(PasswordDetailsTableViewControllerTest, TestSectionsInEdit) {
  SetPassword();
  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  EXPECT_EQ(2, NumberOfItemsInSection(1));
}
