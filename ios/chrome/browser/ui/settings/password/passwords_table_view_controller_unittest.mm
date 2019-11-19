// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/passwords/save_passwords_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#include "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::MockPasswordStore;

// Declaration to conformance to SavePasswordsConsumerDelegate and keep tests in
// this file working.
@interface PasswordsTableViewController (Test) <UISearchBarDelegate,
                                                SavePasswordsConsumerDelegate>
- (void)updateExportPasswordsButton;
@end

namespace {

class PasswordsTableViewControllerTest : public ChromeTableViewControllerTest {
 protected:
  PasswordsTableViewControllerTest() = default;

  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    ChromeTableViewControllerTest::SetUp();
    IOSChromePasswordStoreFactory::GetInstance()->SetTestingFactory(
        chrome_browser_state_.get(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  MockPasswordStore>));
    CreateController();
  }

  MockPasswordStore& GetMockStore() {
    return *static_cast<MockPasswordStore*>(
        IOSChromePasswordStoreFactory::GetForBrowserState(
            chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  ChromeTableViewController* InstantiateController() override {
    return [[PasswordsTableViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  // Adds a form to PasswordsTableViewController.
  void AddPasswordForm(std::unique_ptr<autofill::PasswordForm> form) {
    PasswordsTableViewController* passwords_controller =
        static_cast<PasswordsTableViewController*>(controller());
    std::vector<std::unique_ptr<autofill::PasswordForm>> passwords;
    passwords.push_back(std::move(form));
    [passwords_controller onGetPasswordStoreResults:std::move(passwords)];
  }

  // Creates and adds a saved password form.
  void AddSavedForm1() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->origin = GURL("http://www.example.com/accounts/LoginAuth");
    form->action = GURL("http://www.example.com/accounts/Login");
    form->username_element = base::ASCIIToUTF16("Email");
    form->username_value = base::ASCIIToUTF16("test@egmail.com");
    form->password_element = base::ASCIIToUTF16("Passwd");
    form->password_value = base::ASCIIToUTF16("test");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.example.com/";
    form->preferred = false;
    form->scheme = autofill::PasswordForm::Scheme::kHtml;
    form->blacklisted_by_user = false;
    AddPasswordForm(std::move(form));
  }

  // Creates and adds a saved password form.
  void AddSavedForm2() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->origin = GURL("http://www.example2.com/accounts/LoginAuth");
    form->action = GURL("http://www.example2.com/accounts/Login");
    form->username_element = base::ASCIIToUTF16("Email");
    form->username_value = base::ASCIIToUTF16("test@egmail.com");
    form->password_element = base::ASCIIToUTF16("Passwd");
    form->password_value = base::ASCIIToUTF16("test");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.example2.com/";
    form->preferred = false;
    form->scheme = autofill::PasswordForm::Scheme::kHtml;
    form->blacklisted_by_user = false;
    AddPasswordForm(std::move(form));
  }

  // Creates and adds a blacklisted site form to never offer to save
  // user's password to those sites.
  void AddBlacklistedForm1() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->origin = GURL("http://www.secret.com/login");
    form->action = GURL("http://www.secret.com/action");
    form->username_element = base::ASCIIToUTF16("email");
    form->username_value = base::ASCIIToUTF16("test@secret.com");
    form->password_element = base::ASCIIToUTF16("password");
    form->password_value = base::ASCIIToUTF16("cantsay");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.secret.com/";
    form->preferred = false;
    form->scheme = autofill::PasswordForm::Scheme::kHtml;
    form->blacklisted_by_user = true;
    AddPasswordForm(std::move(form));
  }

  // Creates and adds another blacklisted site form to never offer to save
  // user's password to those sites.
  void AddBlacklistedForm2() {
    auto form = std::make_unique<autofill::PasswordForm>();
    form->origin = GURL("http://www.secret2.com/login");
    form->action = GURL("http://www.secret2.com/action");
    form->username_element = base::ASCIIToUTF16("email");
    form->username_value = base::ASCIIToUTF16("test@secret2.com");
    form->password_element = base::ASCIIToUTF16("password");
    form->password_value = base::ASCIIToUTF16("cantsay");
    form->submit_element = base::ASCIIToUTF16("signIn");
    form->signon_realm = "http://www.secret2.com/";
    form->preferred = false;
    form->scheme = autofill::PasswordForm::Scheme::kHtml;
    form->blacklisted_by_user = true;
    AddPasswordForm(std::move(form));
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

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests default case has no saved sites and no blacklisted sites.
TEST_F(PasswordsTableViewControllerTest, TestInitialization) {
  CheckController();
  EXPECT_EQ(2, NumberOfSections());
}

// Tests adding one item in saved password section.
TEST_F(PasswordsTableViewControllerTest, AddSavedPasswords) {
  AddSavedForm1();

  EXPECT_EQ(3, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Tests adding one item in blacklisted password section.
TEST_F(PasswordsTableViewControllerTest, AddBlacklistedPasswords) {
  AddBlacklistedForm1();

  EXPECT_EQ(3, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Tests adding one item in saved password section, and two items in blacklisted
// password section.
TEST_F(PasswordsTableViewControllerTest, AddSavedAndBlacklisted) {
  AddSavedForm1();
  AddBlacklistedForm1();
  AddBlacklistedForm2();

  // There should be two sections added.
  EXPECT_EQ(4, NumberOfSections());

  // There should be 1 row in saved password section.
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  // There should be 2 rows in blacklisted password section.
  EXPECT_EQ(2, NumberOfItemsInSection(2));
}

// Tests the order in which the saved passwords are displayed.
TEST_F(PasswordsTableViewControllerTest, TestSavedPasswordsOrder) {
  AddSavedForm2();

  CheckTextCellTextAndDetailText(@"example2.com", @"test@egmail.com", 1, 0);

  AddSavedForm1();
  CheckTextCellTextAndDetailText(@"example.com", @"test@egmail.com", 1, 0);
  CheckTextCellTextAndDetailText(@"example2.com", @"test@egmail.com", 1, 1);
}

// Tests the order in which the blacklisted passwords are displayed.
TEST_F(PasswordsTableViewControllerTest, TestBlacklistedPasswordsOrder) {
  AddBlacklistedForm2();
  CheckTextCellText(@"secret2.com", 1, 0);

  AddBlacklistedForm1();
  CheckTextCellText(@"secret.com", 1, 0);
  CheckTextCellText(@"secret2.com", 1, 1);
}

// Tests displaying passwords in the saved passwords section when there are
// duplicates in the password store.
TEST_F(PasswordsTableViewControllerTest, AddSavedDuplicates) {
  AddSavedForm1();
  AddSavedForm1();

  EXPECT_EQ(3, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Tests displaying passwords in the blacklisted passwords section when there
// are duplicates in the password store.
TEST_F(PasswordsTableViewControllerTest, AddBlacklistedDuplicates) {
  AddBlacklistedForm1();
  AddBlacklistedForm1();

  EXPECT_EQ(3, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(1));
}

// Tests deleting items from saved passwords and blacklisted passwords sections.
TEST_F(PasswordsTableViewControllerTest, DeleteItems) {
  AddSavedForm1();
  AddBlacklistedForm1();
  AddBlacklistedForm2();

  // Delete item in save passwords section.
  ASSERT_TRUE(deleteItemAndWait(1, 0, ^{
    return NumberOfSections() == 3;
  }));
  // Section 2 should now be the blacklisted passwords section, and should still
  // have both its items.
  EXPECT_EQ(2, NumberOfItemsInSection(1));

  // Delete item in blacklisted passwords section.
  ASSERT_TRUE(deleteItemAndWait(1, 0, ^{
    return NumberOfItemsInSection(1) == 1;
  }));
  // There should be no password sections remaining and no search bar.
  EXPECT_TRUE(deleteItemAndWait(1, 0, ^{
    return NumberOfSections() == 2;
  }));
}

// Tests deleting items from saved passwords and blacklisted passwords sections
// when there are duplicates in the store.
TEST_F(PasswordsTableViewControllerTest, DeleteItemsWithDuplicates) {
  AddSavedForm1();
  AddSavedForm1();
  AddBlacklistedForm1();
  AddBlacklistedForm1();
  AddBlacklistedForm2();

  // Delete item in save passwords section.
  ASSERT_TRUE(deleteItemAndWait(1, 0, ^{
    return NumberOfSections() == 3;
  }));
  // Section 2 should now be the blacklisted passwords section, and should still
  // have both its items.
  EXPECT_EQ(2, NumberOfItemsInSection(1));

  // Delete item in blacklisted passwords section.
  ASSERT_TRUE(deleteItemAndWait(1, 0, ^{
    return NumberOfItemsInSection(1) == 1;
  }));
  // There should be no password sections remaining and no search bar.
  EXPECT_TRUE(deleteItemAndWait(1, 0, ^{
    return NumberOfSections() == 2;
  }));
}

TEST_F(PasswordsTableViewControllerTest,
       TestExportButtonDisabledNoSavedPasswords) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton = GetTableViewItem(1, 0);
  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS, 1, 0);

  EXPECT_NSEQ(UIColor.cr_labelColor, exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);

  // Add blacklisted form.
  AddBlacklistedForm1();
  // The export button should still be disabled as exporting blacklisted forms
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

  TableViewDetailTextItem* exportButton = GetTableViewItem(2, 0);

  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS, 2, 0);

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

  TableViewDetailTextItem* exportButton = GetTableViewItem(2, 0);
  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS, 2, 0);

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

  TableViewDetailTextItem* exportButton = GetTableViewItem(2, 0);
  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS, 2, 0);

  [passwords_controller setEditing:YES animated:NO];
  [passwords_controller setEditing:NO animated:NO];

  EXPECT_NSEQ([UIColor colorNamed:kBlueColor], exportButton.textColor);
  EXPECT_FALSE(exportButton.accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

TEST_F(PasswordsTableViewControllerTest, PropagateDeletionToStore) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  autofill::PasswordForm form;
  form.origin = GURL("http://www.example.com/accounts/LoginAuth");
  form.action = GURL("http://www.example.com/accounts/Login");
  form.username_element = base::ASCIIToUTF16("Email");
  form.username_value = base::ASCIIToUTF16("test@egmail.com");
  form.password_element = base::ASCIIToUTF16("Passwd");
  form.password_value = base::ASCIIToUTF16("test");
  form.submit_element = base::ASCIIToUTF16("signIn");
  form.signon_realm = "http://www.example.com/";
  form.scheme = autofill::PasswordForm::Scheme::kHtml;
  form.blacklisted_by_user = false;

  AddPasswordForm(std::make_unique<autofill::PasswordForm>(form));

  EXPECT_CALL(GetMockStore(), RemoveLogin(form));
  [passwords_controller passwordDetailsTableViewController:nil
                                            deletePassword:form];
}

// Tests filtering of items.
TEST_F(PasswordsTableViewControllerTest, FilterItems) {
  AddSavedForm1();
  AddSavedForm2();
  AddBlacklistedForm1();
  AddBlacklistedForm2();

  EXPECT_EQ(4, NumberOfSections());

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
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  EXPECT_EQ(0, NumberOfItemsInSection(2));
  CheckTextCellTextAndDetailText(@"example.com", @"test@egmail.com", 1, 0);

  [passwords_controller searchBar:bar textDidChange:@"test@egmail.com"];
  // Only two items in saved passwords should remain.
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  EXPECT_EQ(0, NumberOfItemsInSection(2));
  CheckTextCellTextAndDetailText(@"example.com", @"test@egmail.com", 1, 0);
  CheckTextCellTextAndDetailText(@"example2.com", @"test@egmail.com", 1, 1);

  [passwords_controller searchBar:bar textDidChange:@"secret"];
  // Only two items in blacklisted should remain.
  EXPECT_EQ(0, NumberOfItemsInSection(1));
  EXPECT_EQ(2, NumberOfItemsInSection(2));
  CheckTextCellText(@"secret.com", 2, 0);
  CheckTextCellText(@"secret2.com", 2, 1);

  [passwords_controller searchBar:bar textDidChange:@""];
  // All items should be back.
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  EXPECT_EQ(2, NumberOfItemsInSection(2));
}

}  // namespace
