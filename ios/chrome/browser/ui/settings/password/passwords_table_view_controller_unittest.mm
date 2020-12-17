// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/mock_bulk_leak_check_service.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/passwords/ios_chrome_bulk_leak_check_service_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#include "ios/chrome/browser/passwords/save_passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/password/passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/passwords_mediator.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#include "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::CompromiseType;
using password_manager::TestPasswordStore;
using password_manager::MockBulkLeakCheckService;
using ::testing::Return;

// Declaration to conformance to SavePasswordsConsumerDelegate and keep tests in
// this file working.
@interface PasswordsTableViewController (Test) <
    UISearchBarDelegate,
    PasswordsConsumer>
- (void)updateExportPasswordsButton;
@end

namespace {

typedef struct {
  bool password_check_enabled;
} PasswordCheckFeatureStatus;

enum PasswordsSections {
  SavePasswordsSwitch = 0,
  PasswordCheck,
  SavedPasswords,
  Blocked,
  ExportPasswordsButton,
};

class PasswordsTableViewControllerTest : public ChromeTableViewControllerTest {
 protected:
  PasswordsTableViewControllerTest() = default;

  void SetUp() override {
    browser_ = std::make_unique<TestBrowser>();
    ChromeTableViewControllerTest::SetUp();
    IOSChromePasswordStoreFactory::GetInstance()->SetTestingFactory(
        browser_->GetBrowserState(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));

    IOSChromeBulkLeakCheckServiceFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser_->GetBrowserState(),
            base::BindLambdaForTesting([](web::BrowserState*) {
              return std::unique_ptr<KeyedService>(
                  std::make_unique<MockBulkLeakCheckService>());
            }));

    CreateController();

    mediator_ = [[PasswordsMediator alloc]
        initWithPasswordStore:IOSChromePasswordStoreFactory::GetForBrowserState(
                                  browser_->GetBrowserState(),
                                  ServiceAccessType::EXPLICIT_ACCESS)
         passwordCheckManager:IOSChromePasswordCheckManagerFactory::
                                  GetForBrowserState(
                                      browser_->GetBrowserState())
                  authService:nil
                  syncService:nil];

    // Inject some fake passwords to pass the loading state.
    PasswordsTableViewController* passwords_controller =
        static_cast<PasswordsTableViewController*>(controller());
    passwords_controller.delegate = mediator_;
    mediator_.consumer = passwords_controller;
    [passwords_controller setPasswordsForms:{}];
  }

  int GetSectionIndex(PasswordsSections section) {
    switch (section) {
      case SavePasswordsSwitch:
      case PasswordCheck:
        return section;
      case SavedPasswords:
        return 2;
      case Blocked:
        return 3;
      case ExportPasswordsButton:
        return 3;
    }
  }

  int SectionsOffset() { return 1; }

  TestPasswordStore& GetTestStore() {
    return *static_cast<TestPasswordStore*>(
        IOSChromePasswordStoreFactory::GetForBrowserState(
            browser_->GetBrowserState(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  MockBulkLeakCheckService& GetMockPasswordCheckService() {
    return *static_cast<MockBulkLeakCheckService*>(
        IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(
            browser_->GetBrowserState()));
  }

  ChromeTableViewController* InstantiateController() override {
    return
        [[PasswordsTableViewController alloc] initWithBrowser:browser_.get()];
  }

  void ChangePasswordCheckState(PasswordCheckUIState state) {
    PasswordsTableViewController* passwords_controller =
        static_cast<PasswordsTableViewController*>(controller());
    NSInteger count = GetTestStore().compromised_credentials().size();
    [passwords_controller setPasswordCheckUIState:state
                        compromisedPasswordsCount:count];
  }

  // Adds a form to PasswordsTableViewController.
  void AddPasswordForm(std::unique_ptr<password_manager::PasswordForm> form) {
    GetTestStore().AddLogin(*form);
    RunUntilIdle();
  }

  // Creates and adds a saved password form.
  void AddSavedForm1() {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL("http://www.example.com/accounts/LoginAuth");
    form->action = GURL("http://www.example.com/accounts/Login");
    form->username_element = base::ASCIIToUTF16("Email");
    form->username_value = base::ASCIIToUTF16("test@egmail.com");
    form->password_element = base::ASCIIToUTF16("Passwd");
    form->password_value = base::ASCIIToUTF16("test");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.example.com/";
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->blocked_by_user = false;
    AddPasswordForm(std::move(form));
  }

  // Creates and adds a saved password form.
  void AddSavedForm2() {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL("http://www.example2.com/accounts/LoginAuth");
    form->action = GURL("http://www.example2.com/accounts/Login");
    form->username_element = base::ASCIIToUTF16("Email");
    form->username_value = base::ASCIIToUTF16("test@egmail.com");
    form->password_element = base::ASCIIToUTF16("Passwd");
    form->password_value = base::ASCIIToUTF16("test");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.example2.com/";
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->blocked_by_user = false;
    AddPasswordForm(std::move(form));
  }

  // Creates and adds a blocked site form to never offer to save
  // user's password to those sites.
  void AddBlockedForm1() {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL("http://www.secret.com/login");
    form->action = GURL("http://www.secret.com/action");
    form->username_element = base::ASCIIToUTF16("email");
    form->username_value = base::ASCIIToUTF16("test@secret.com");
    form->password_element = base::ASCIIToUTF16("password");
    form->password_value = base::ASCIIToUTF16("cantsay");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.secret.com/";
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->blocked_by_user = true;
    AddPasswordForm(std::move(form));
  }

  // Creates and adds another blocked site form to never offer to save
  // user's password to those sites.
  void AddBlockedForm2() {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL("http://www.secret2.com/login");
    form->action = GURL("http://www.secret2.com/action");
    form->username_element = base::ASCIIToUTF16("email");
    form->username_value = base::ASCIIToUTF16("test@secret2.com");
    form->password_element = base::ASCIIToUTF16("password");
    form->password_value = base::ASCIIToUTF16("cantsay");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.secret2.com/";
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->blocked_by_user = true;
    AddPasswordForm(std::move(form));
  }

  password_manager::CompromisedCredentials MakeCompromised(
      base::StringPiece signon_realm,
      base::StringPiece username) {
    return password_manager::CompromisedCredentials(
        std::string(signon_realm), base::ASCIIToUTF16(username),
        base::Time::Now(), CompromiseType::kLeaked,
        password_manager::IsMuted(false));
  }

  void AddCompromisedCredential1() {
    GetTestStore().AddCompromisedCredentials(
        MakeCompromised("http://www.example.com/", "test@egmail.com"));
    RunUntilIdle();
  }

  // Deletes the item at (row, section) and wait util condition returns true or
  // timeout.
  bool deleteItemAndWait(int section, int row, ConditionBlock condition) {
    PasswordsTableViewController* passwords_controller =
        static_cast<PasswordsTableViewController*>(controller());
    [passwords_controller
        deleteItems:@[ [NSIndexPath indexPathForRow:row inSection:section] ]];
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, condition);
  }

  void CheckDetailItemTextWithPluralIds(int expected_text_id,
                                        int expected_detail_text_id,
                                        int count,
                                        int section,
                                        int item) {
    SettingsCheckItem* cell =
        static_cast<SettingsCheckItem*>(GetTableViewItem(section, item));
    EXPECT_NSEQ(l10n_util::GetNSString(expected_text_id), [cell text]);
    EXPECT_NSEQ(base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
                    IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT, count)),
                [cell detailText]);
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestBrowser> browser_;
  PasswordsMediator* mediator_;
};

// Tests default case has no saved sites and no blocked sites.
TEST_F(PasswordsTableViewControllerTest, TestInitialization) {
  CheckController();
  EXPECT_EQ(2 + SectionsOffset(), NumberOfSections());
}

// Tests adding one item in saved password section.
TEST_F(PasswordsTableViewControllerTest, AddSavedPasswords) {
  AddSavedForm1();

  EXPECT_EQ(3 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
}

// Tests adding one item in blocked password section.
TEST_F(PasswordsTableViewControllerTest, AddBlockedPasswords) {
  AddBlockedForm1();

  EXPECT_EQ(3 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(Blocked)));
}

// Tests adding one item in saved password section, and two items in blocked
// password section.
TEST_F(PasswordsTableViewControllerTest, AddSavedAndBlocked) {
  AddSavedForm1();
  AddBlockedForm1();
  AddBlockedForm2();

  // There should be two sections added.
  EXPECT_EQ(4 + SectionsOffset(), NumberOfSections());

  // There should be 1 row in saved password section.
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  // There should be 2 rows in blocked password section.
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(Blocked)));
}

// Tests the order in which the saved passwords are displayed.
TEST_F(PasswordsTableViewControllerTest, TestSavedPasswordsOrder) {
  AddSavedForm2();

  CheckTextCellTextAndDetailText(@"example2.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 0);

  AddSavedForm1();
  CheckTextCellTextAndDetailText(@"example.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 0);
  CheckTextCellTextAndDetailText(@"example2.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 1);
}

// Tests the order in which the blocked passwords are displayed.
TEST_F(PasswordsTableViewControllerTest, TestBlockedPasswordsOrder) {
  AddBlockedForm2();
  CheckTextCellText(@"secret2.com", GetSectionIndex(SavedPasswords), 0);

  AddBlockedForm1();
  CheckTextCellText(@"secret.com", GetSectionIndex(SavedPasswords), 0);
  CheckTextCellText(@"secret2.com", GetSectionIndex(SavedPasswords), 1);
}

// Tests displaying passwords in the saved passwords section when there are
// duplicates in the password store.
TEST_F(PasswordsTableViewControllerTest, AddSavedDuplicates) {
  AddSavedForm1();
  AddSavedForm1();

  EXPECT_EQ(3 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
}

// Tests displaying passwords in the blocked passwords section when there
// are duplicates in the password store.
TEST_F(PasswordsTableViewControllerTest, AddBlockedDuplicates) {
  AddBlockedForm1();
  AddBlockedForm1();

  EXPECT_EQ(3 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
}

// Tests deleting items from saved passwords and blocked passwords sections.
TEST_F(PasswordsTableViewControllerTest, DeleteItems) {
  AddSavedForm1();
  AddBlockedForm1();
  AddBlockedForm2();

  // Delete item in save passwords section.
  ASSERT_TRUE(deleteItemAndWait(GetSectionIndex(SavedPasswords), 0, ^{
    return NumberOfSections() == (3 + SectionsOffset());
  }));
  // Section 2 should now be the blocked passwords section, and should still
  // have both its items.
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));

  // Delete item in blocked passwords section.
  ASSERT_TRUE(deleteItemAndWait(GetSectionIndex(SavedPasswords), 0, ^{
    return NumberOfItemsInSection(GetSectionIndex(SavedPasswords)) == 1;
  }));
  // There should be no password sections remaining and no search bar.
  EXPECT_TRUE(deleteItemAndWait(GetSectionIndex(SavedPasswords), 0, ^{
    return NumberOfSections() == (2 + +SectionsOffset());
  }));
}

// Tests deleting items from saved passwords and blocked passwords sections
// when there are duplicates in the store.
TEST_F(PasswordsTableViewControllerTest, DeleteItemsWithDuplicates) {
  AddSavedForm1();
  AddSavedForm1();
  AddBlockedForm1();
  AddBlockedForm1();
  AddBlockedForm2();

  // Delete item in save passwords section.
  ASSERT_TRUE(deleteItemAndWait(GetSectionIndex(SavedPasswords), 0, ^{
    return NumberOfSections() == (3 + SectionsOffset());
  }));
  // Section 2 should now be the blocked passwords section, and should still
  // have both its items.
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(Blocked) - 1));

  // Delete item in blocked passwords section.
  ASSERT_TRUE(deleteItemAndWait(GetSectionIndex(Blocked) - 1, 0, ^{
    return NumberOfItemsInSection(GetSectionIndex(Blocked) - 1) == 1;
  }));
  // There should be no password sections remaining and no search bar.
  EXPECT_TRUE(deleteItemAndWait(GetSectionIndex(Blocked) - 1, 0, ^{
    return NumberOfSections() == (2 + SectionsOffset());
  }));
}

TEST_F(PasswordsTableViewControllerTest,
       TestExportButtonDisabledNoSavedPasswords) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton =
      GetTableViewItem(GetSectionIndex(SavedPasswords), 0);
  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS,
                          GetSectionIndex(SavedPasswords), 0);

  EXPECT_NSEQ(UIColor.cr_labelColor, exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);

  // Add blocked form.
  AddBlockedForm1();
  // The export button should still be disabled as exporting blocked forms
  // is not currently supported.
  EXPECT_NSEQ(UIColor.cr_labelColor, exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

TEST_F(PasswordsTableViewControllerTest,
       TestExportButtonEnabledWithSavedPasswords) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  AddSavedForm1();
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton =
      GetTableViewItem(GetSectionIndex(ExportPasswordsButton), 0);

  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS,
                          GetSectionIndex(ExportPasswordsButton), 0);

  EXPECT_NSEQ([UIColor colorNamed:kBlueColor], exportButton.textColor);
  EXPECT_FALSE(exportButton.accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

// Tests that the "Export Passwords..." button is greyed out in edit mode.
TEST_F(PasswordsTableViewControllerTest, TestExportButtonDisabledEditMode) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  AddSavedForm1();
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton =
      GetTableViewItem(GetSectionIndex(ExportPasswordsButton), 0);
  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS,
                          GetSectionIndex(ExportPasswordsButton), 0);

  [passwords_controller setEditing:YES animated:NO];

  EXPECT_NSEQ(UIColor.cr_labelColor, exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

// Tests that the "Export Passwords..." button is enabled after exiting
// edit mode.
TEST_F(PasswordsTableViewControllerTest,
       TestExportButtonEnabledWhenEdittingFinished) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  AddSavedForm1();
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton =
      GetTableViewItem(GetSectionIndex(ExportPasswordsButton), 0);
  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS,
                          GetSectionIndex(ExportPasswordsButton), 0);

  [passwords_controller setEditing:YES animated:NO];
  [passwords_controller setEditing:NO animated:NO];

  EXPECT_NSEQ([UIColor colorNamed:kBlueColor], exportButton.textColor);
  EXPECT_FALSE(exportButton.accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

// Tests filtering of items.
TEST_F(PasswordsTableViewControllerTest, FilterItems) {
  AddSavedForm1();
  AddSavedForm2();
  AddBlockedForm1();
  AddBlockedForm2();

  EXPECT_EQ(4 + SectionsOffset(), NumberOfSections());

  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  UISearchBar* bar =
      passwords_controller.navigationItem.searchController.searchBar;

  // Force the initial data to be rendered into view first, before doing any
  // new filtering (avoids mismatch when reloadSections is called).
  [passwords_controller searchBar:bar textDidChange:@""];

  // Search item in save passwords section.
  [passwords_controller searchBar:bar textDidChange:@"example.com"];
  // Only one item in saved passwords should remain.
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  EXPECT_EQ(0, NumberOfItemsInSection(GetSectionIndex(Blocked)));
  CheckTextCellTextAndDetailText(@"example.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 0);

  [passwords_controller searchBar:bar textDidChange:@"test@egmail.com"];
  // Only two items in saved passwords should remain.
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  EXPECT_EQ(0, NumberOfItemsInSection(GetSectionIndex(Blocked)));
  CheckTextCellTextAndDetailText(@"example.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 0);
  CheckTextCellTextAndDetailText(@"example2.com", @"test@egmail.com",
                                 GetSectionIndex(SavedPasswords), 1);

  [passwords_controller searchBar:bar textDidChange:@"secret"];
  // Only two blocked items should remain.
  EXPECT_EQ(0, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(Blocked)));
  CheckTextCellText(@"secret.com", GetSectionIndex(Blocked), 0);
  CheckTextCellText(@"secret2.com", GetSectionIndex(Blocked), 1);

  [passwords_controller searchBar:bar textDidChange:@""];
  // All items should be back.
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(Blocked)));
}

// Test verifies disabled state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateDisabled) {
  ChangePasswordCheckState(PasswordCheckStateDisabled);

  CheckDetailItemTextWithIds(IDS_IOS_CHECK_PASSWORDS,
                             IDS_IOS_CHECK_PASSWORDS_DESCRIPTION,
                             GetSectionIndex(PasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(PasswordCheck), 0);
  EXPECT_FALSE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);
}

// Test verifies default state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateDefault) {
  ChangePasswordCheckState(PasswordCheckStateDefault);

  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(PasswordCheck), 1);
  CheckDetailItemTextWithIds(IDS_IOS_CHECK_PASSWORDS,
                             IDS_IOS_CHECK_PASSWORDS_DESCRIPTION,
                             GetSectionIndex(PasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(PasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);
}

// Test verifies safe state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateSafe) {
  ChangePasswordCheckState(PasswordCheckStateSafe);

  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(PasswordCheck), 1);
  CheckDetailItemTextWithPluralIds(IDS_IOS_CHECK_PASSWORDS,
                                   IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT, 0,
                                   GetSectionIndex(PasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(PasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_TRUE(checkPassword.trailingImage);
}

// Test verifies unsafe state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateUnSafe) {
  AddSavedForm1();
  AddCompromisedCredential1();
  ChangePasswordCheckState(PasswordCheckStateUnSafe);

  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(PasswordCheck), 1);
  CheckDetailItemTextWithPluralIds(IDS_IOS_CHECK_PASSWORDS,
                                   IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT, 1,
                                   GetSectionIndex(PasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(PasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_TRUE(checkPassword.trailingImage);
}

// Test verifies running state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateRunning) {
  ChangePasswordCheckState(PasswordCheckStateRunning);

  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(PasswordCheck), 1);
  CheckDetailItemTextWithIds(IDS_IOS_CHECK_PASSWORDS,
                             IDS_IOS_CHECK_PASSWORDS_DESCRIPTION,
                             GetSectionIndex(PasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(PasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_FALSE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);
}

// Test verifies error state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateError) {
  ChangePasswordCheckState(PasswordCheckStateError);

  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(PasswordCheck), 1);
  CheckDetailItemTextWithIds(IDS_IOS_CHECK_PASSWORDS,
                             IDS_IOS_PASSWORD_CHECK_ERROR,
                             GetSectionIndex(PasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(PasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);
  EXPECT_FALSE(checkPassword.infoButtonHidden);
}

// Test verifies tapping start with no saved passwords has no effect.
TEST_F(PasswordsTableViewControllerTest, DisabledPasswordCheck) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());

  EXPECT_CALL(GetMockPasswordCheckService(), CheckUsernamePasswordPairs)
      .Times(0);
  EXPECT_CALL(GetMockPasswordCheckService(), Cancel).Times(0);

  [passwords_controller tableView:passwords_controller.tableView
          didSelectRowAtIndexPath:[NSIndexPath
                                      indexPathForItem:1
                                             inSection:GetSectionIndex(
                                                           PasswordCheck)]];
}

// Test verifies tapping start triggers correct function in service.
TEST_F(PasswordsTableViewControllerTest, StartPasswordCheck) {
  AddSavedForm1();
  RunUntilIdle();

  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());

  EXPECT_CALL(GetMockPasswordCheckService(), CheckUsernamePasswordPairs);

  [passwords_controller tableView:passwords_controller.tableView
          didSelectRowAtIndexPath:[NSIndexPath
                                      indexPathForItem:1
                                             inSection:GetSectionIndex(
                                                           PasswordCheck)]];
}

// Test verifies changes to the password store are reflected on UI.
TEST_F(PasswordsTableViewControllerTest, PasswordStoreListener) {
  AddSavedForm1();
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
  AddSavedForm2();
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));

  auto password =
      GetTestStore().stored_passwords().at("http://www.example.com/").at(0);
  GetTestStore().RemoveLogin(password);
  RunUntilIdle();
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
}

// Test verifies Passwords View Controller handles deletion of passwords.
TEST_F(PasswordsTableViewControllerTest, PasswordIssuesDeletion) {
  AddSavedForm1();
  AddSavedForm2();
  EXPECT_EQ(2, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));

  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());

  auto password =
      GetTestStore().stored_passwords().at("http://www.example.com/").at(0);
  [passwords_controller deletePasswordForm:password];
  EXPECT_EQ(1, NumberOfItemsInSection(GetSectionIndex(SavedPasswords)));
}

}  // namespace
