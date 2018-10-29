// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/save_passwords_collection_view_controller.h"

#include <memory>
#include <utility>

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
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/cells/settings_search_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/ui/settings/password_details_collection_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::MockPasswordStore;

// Declaration to conformance to SavePasswordsConsumerDelegate and keep tests in
// this file working.
@interface SavePasswordsCollectionViewController (
    Test)<SettingsSearchItemDelegate, SavePasswordsConsumerDelegate>
- (void)updateExportPasswordsButton;
@end

namespace {

typedef struct {
  bool export_enabled;
  int section_offset;
} ExportPasswordsFeatureStatus;

class SavePasswordsCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  SavePasswordsCollectionViewControllerTest() = default;

  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    CollectionViewControllerTest::SetUp();
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

  CollectionViewController* InstantiateController() override {
    return [[SavePasswordsCollectionViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  // Adds a form to SavePasswordsTableViewController.
  void AddPasswordForm(std::unique_ptr<autofill::PasswordForm> form) {
    SavePasswordsCollectionViewController* save_password_controller =
        static_cast<SavePasswordsCollectionViewController*>(controller());
    std::vector<std::unique_ptr<autofill::PasswordForm>> passwords;
    passwords.push_back(std::move(form));
    [save_password_controller onGetPasswordStoreResults:passwords];
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
    form->scheme = autofill::PasswordForm::SCHEME_HTML;
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
    form->scheme = autofill::PasswordForm::SCHEME_HTML;
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
    form->scheme = autofill::PasswordForm::SCHEME_HTML;
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
    form->scheme = autofill::PasswordForm::SCHEME_HTML;
    form->blacklisted_by_user = true;
    AddPasswordForm(std::move(form));
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

// Tests default case has no saved sites and no blacklisted sites.
TEST_F(SavePasswordsCollectionViewControllerTest, TestInitialization) {
  CheckController();
  EXPECT_EQ(3, NumberOfSections());
}

// Tests adding one item in saved password section.
TEST_F(SavePasswordsCollectionViewControllerTest, AddSavedPasswords) {
  AddSavedForm1();

  EXPECT_EQ(5, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(3));
}

// Tests adding one item in blacklisted password section.
TEST_F(SavePasswordsCollectionViewControllerTest, AddBlacklistedPasswords) {
  AddBlacklistedForm1();

  EXPECT_EQ(5, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(3));
}

// Tests adding one item in saved password section, and two items in blacklisted
// password section.
TEST_F(SavePasswordsCollectionViewControllerTest, AddSavedAndBlacklisted) {
  AddSavedForm1();
  AddBlacklistedForm1();
  AddBlacklistedForm2();

  // There should be two sections added.
  EXPECT_EQ(6, NumberOfSections());

  // There should be 1 row in saved password section.
  EXPECT_EQ(1, NumberOfItemsInSection(3));
  // There should be 2 rows in blacklisted password section.
  EXPECT_EQ(2, NumberOfItemsInSection(4));
}

// Tests the order in which the saved passwords are displayed.
TEST_F(SavePasswordsCollectionViewControllerTest, TestSavedPasswordsOrder) {
  AddSavedForm2();

  CheckTextCellTitleAndSubtitle(@"example2.com", @"test@egmail.com", 3, 0);

  AddSavedForm1();
  CheckTextCellTitleAndSubtitle(@"example.com", @"test@egmail.com", 3, 0);
  CheckTextCellTitleAndSubtitle(@"example2.com", @"test@egmail.com", 3, 1);
}

// Tests the order in which the blacklisted passwords are displayed.
TEST_F(SavePasswordsCollectionViewControllerTest,
       TestBlacklistedPasswordsOrder) {
  AddBlacklistedForm2();
  CheckTextCellTitle(@"secret2.com", 3, 0);

  AddBlacklistedForm1();
  CheckTextCellTitle(@"secret.com", 3, 0);
  CheckTextCellTitle(@"secret2.com", 3, 1);
}

// Tests displaying passwords in the saved passwords section when there are
// duplicates in the password store.
TEST_F(SavePasswordsCollectionViewControllerTest, AddSavedDuplicates) {
  AddSavedForm1();
  AddSavedForm1();

  EXPECT_EQ(5, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(3));
}

// Tests displaying passwords in the blacklisted passwords section when there
// are duplicates in the password store.
TEST_F(SavePasswordsCollectionViewControllerTest, AddBlacklistedDuplicates) {
  AddBlacklistedForm1();
  AddBlacklistedForm1();

  EXPECT_EQ(5, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(3));
}

// Tests deleting items from saved passwords and blacklisted passwords sections.
TEST_F(SavePasswordsCollectionViewControllerTest, DeleteItems) {
  AddSavedForm1();
  AddBlacklistedForm1();
  AddBlacklistedForm2();

  void (^deleteItemWithWait)(int, int) = ^(int i, int j) {
    __block BOOL completionCalled = NO;
    this->DeleteItem(i, j, ^{
      completionCalled = YES;
    });
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^bool() {
          return completionCalled;
        }));
  };

  // Delete item in save passwords section.
  deleteItemWithWait(3, 0);
  EXPECT_EQ(5, NumberOfSections());
  // Section 3 should now be the blacklisted passwords section, and should still
  // have both its items.
  EXPECT_EQ(2, NumberOfItemsInSection(3));

  // Delete item in blacklisted passwords section.
  deleteItemWithWait(3, 0);
  EXPECT_EQ(1, NumberOfItemsInSection(3));
  deleteItemWithWait(3, 0);
  // There should be no password sections remaining and no search bar.
  EXPECT_EQ(3, NumberOfSections());
}

// Tests deleting items from saved passwords and blacklisted passwords sections
// when there are duplicates in the store.
TEST_F(SavePasswordsCollectionViewControllerTest, DeleteItemsWithDuplicates) {
  AddSavedForm1();
  AddSavedForm1();
  AddBlacklistedForm1();
  AddBlacklistedForm1();
  AddBlacklistedForm2();

  void (^deleteItemWithWait)(int, int) = ^(int i, int j) {
    __block BOOL completionCalled = NO;
    this->DeleteItem(i, j, ^{
      completionCalled = YES;
    });
    EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^bool() {
          return completionCalled;
        }));
  };

  // Delete item in save passwords section.
  deleteItemWithWait(3, 0);
  EXPECT_EQ(5, NumberOfSections());
  // Section 3 should now be the blacklisted passwords section, and should still
  // have both its items.
  EXPECT_EQ(2, NumberOfItemsInSection(3));

  // Delete item in blacklisted passwords section.
  deleteItemWithWait(3, 0);
  EXPECT_EQ(1, NumberOfItemsInSection(3));
  deleteItemWithWait(3, 0);
  // There should be no password sections remaining and no search bar.
  EXPECT_EQ(3, NumberOfSections());
}

TEST_F(SavePasswordsCollectionViewControllerTest,
       TestExportButtonDisabledNoSavedPasswords) {
  SavePasswordsCollectionViewController* save_password_controller =
      static_cast<SavePasswordsCollectionViewController*>(controller());
  [save_password_controller updateExportPasswordsButton];

  SettingsTextItem* exportButton = GetCollectionViewItem(2, 0);
  CheckTextCellTitleWithId(IDS_IOS_EXPORT_PASSWORDS, 2, 0);

  UIColor* disabledColor = [[MDCPalette greyPalette] tint500];
  EXPECT_NSEQ(disabledColor, exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);

  // Add blacklisted form.
  AddBlacklistedForm1();
  // The export button should still be disabled as exporting blacklisted forms
  // is not currently supported.
  EXPECT_NSEQ(disabledColor, exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

TEST_F(SavePasswordsCollectionViewControllerTest,
       TestExportButtonEnabledWithSavedPasswords) {
  SavePasswordsCollectionViewController* save_password_controller =
      static_cast<SavePasswordsCollectionViewController*>(controller());
  AddSavedForm1();
  [save_password_controller updateExportPasswordsButton];

  SettingsTextItem* exportButton = GetCollectionViewItem(4, 0);

  CheckTextCellTitleWithId(IDS_IOS_EXPORT_PASSWORDS, 4, 0);
  EXPECT_NSEQ([[MDCPalette greyPalette] tint900], exportButton.textColor);
  EXPECT_FALSE(exportButton.accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

// Tests that the "Export Passwords..." button is greyed out in edit mode.
TEST_F(SavePasswordsCollectionViewControllerTest,
       TestExportButtonDisabledEditMode) {
  SavePasswordsCollectionViewController* save_password_controller =
      static_cast<SavePasswordsCollectionViewController*>(controller());
  AddSavedForm1();
  [save_password_controller updateExportPasswordsButton];

  SettingsTextItem* exportButton = GetCollectionViewItem(4, 0);
  CheckTextCellTitleWithId(IDS_IOS_EXPORT_PASSWORDS, 4, 0);

  [save_password_controller
      collectionViewWillBeginEditing:save_password_controller.collectionView];

  EXPECT_NSEQ([[MDCPalette greyPalette] tint500], exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

// Tests that the "Export Passwords..." button is enabled after exiting
// edit mode.
TEST_F(SavePasswordsCollectionViewControllerTest,
       TestExportButtonEnabledWhenEdittingFinished) {
  SavePasswordsCollectionViewController* save_password_controller =
      static_cast<SavePasswordsCollectionViewController*>(controller());
  AddSavedForm1();
  [save_password_controller updateExportPasswordsButton];

  SettingsTextItem* exportButton = GetCollectionViewItem(4, 0);
  CheckTextCellTitleWithId(IDS_IOS_EXPORT_PASSWORDS, 4, 0);

  [save_password_controller
      collectionViewWillBeginEditing:save_password_controller.collectionView];
  [save_password_controller
      collectionViewWillEndEditing:save_password_controller.collectionView];

  EXPECT_NSEQ([[MDCPalette greyPalette] tint900], exportButton.textColor);
  EXPECT_FALSE(exportButton.accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

TEST_F(SavePasswordsCollectionViewControllerTest, PropagateDeletionToStore) {
  SavePasswordsCollectionViewController* save_password_controller =
      static_cast<SavePasswordsCollectionViewController*>(controller());
  autofill::PasswordForm form;
  form.origin = GURL("http://www.example.com/accounts/LoginAuth");
  form.action = GURL("http://www.example.com/accounts/Login");
  form.username_element = base::ASCIIToUTF16("Email");
  form.username_value = base::ASCIIToUTF16("test@egmail.com");
  form.password_element = base::ASCIIToUTF16("Passwd");
  form.password_value = base::ASCIIToUTF16("test");
  form.submit_element = base::ASCIIToUTF16("signIn");
  form.signon_realm = "http://www.example.com/";
  form.scheme = autofill::PasswordForm::SCHEME_HTML;
  form.blacklisted_by_user = false;

  AddPasswordForm(std::make_unique<autofill::PasswordForm>(form));

  EXPECT_CALL(GetMockStore(), RemoveLogin(form));
  [save_password_controller deletePassword:form];
}

// Tests filtering of items.
TEST_F(SavePasswordsCollectionViewControllerTest, FilterItems) {
  AddSavedForm1();
  AddSavedForm2();
  AddBlacklistedForm1();
  AddBlacklistedForm2();

  EXPECT_EQ(6, NumberOfSections());

  SavePasswordsCollectionViewController* save_password_controller =
      static_cast<SavePasswordsCollectionViewController*>(controller());

  // Force the initial data to be rendered into view first, before doing any
  // new filtering (avoids mismatch when reloadSections is called).
  [save_password_controller didRequestSearchForTerm:@""];

  // Search item in save passwords section.
  [save_password_controller didRequestSearchForTerm:@"example.com"];
  // Only one item in saved passwords should remain.
  EXPECT_EQ(1, NumberOfItemsInSection(3));
  EXPECT_EQ(0, NumberOfItemsInSection(4));
  CheckTextCellTitleAndSubtitle(@"example.com", @"test@egmail.com", 3, 0);

  [save_password_controller didRequestSearchForTerm:@"test@egmail.com"];
  // Only two items in saved passwords should remain.
  EXPECT_EQ(2, NumberOfItemsInSection(3));
  EXPECT_EQ(0, NumberOfItemsInSection(4));
  CheckTextCellTitleAndSubtitle(@"example.com", @"test@egmail.com", 3, 0);
  CheckTextCellTitleAndSubtitle(@"example2.com", @"test@egmail.com", 3, 1);

  [save_password_controller didRequestSearchForTerm:@"secret"];
  // Only two items in blacklisted should remain.
  EXPECT_EQ(0, NumberOfItemsInSection(3));
  EXPECT_EQ(2, NumberOfItemsInSection(4));
  CheckTextCellTitle(@"secret.com", 4, 0);
  CheckTextCellTitle(@"secret2.com", 4, 1);

  [save_password_controller didRequestSearchForTerm:@""];
  // All items should be back.
  EXPECT_EQ(2, NumberOfItemsInSection(3));
  EXPECT_EQ(2, NumberOfItemsInSection(4));
}

}  // namespace
