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
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#include "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
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
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#include "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/sync_test_util.h"
#include "ios/chrome/test/scoped_key_window.h"
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

using password_manager::InsecureType;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;
using password_manager::MockBulkLeakCheckService;
using ::testing::Return;

// Declaration to conformance to SavePasswordsConsumerDelegate and keep tests in
// this file working.
@interface PasswordsTableViewController (Test) <PasswordsConsumer,
                                                UISearchBarDelegate,
                                                UISearchControllerDelegate>
- (void)updateExportPasswordsButton;
@end

// TODO(crbug.com/1324555): Remove this double and uses TestSyncUserSettings
@interface TestPasswordsMediator : PasswordsMediator

@property(nonatomic) OnDeviceEncryptionState encryptionState;

@end

@implementation TestPasswordsMediator

- (OnDeviceEncryptionState)onDeviceEncryptionState:
    (ChromeBrowserState*)browserState {
  return self.encryptionState;
}

@end

namespace {

typedef struct {
  bool password_check_enabled;
} PasswordCheckFeatureStatus;

class PasswordsTableViewControllerTest : public ChromeTableViewControllerTest {
 protected:
  PasswordsTableViewControllerTest() = default;

  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));
    builder.AddTestingFactory(
        IOSChromeBulkLeakCheckServiceFactory::GetInstance(),
        base::BindRepeating(base::BindLambdaForTesting([](web::BrowserState*) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<MockBulkLeakCheckService>());
        })));

    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    CreateController();

    ChromeBrowserState* browserState = browser_->GetBrowserState();
    mediator_ = [[TestPasswordsMediator alloc]
        initWithPasswordCheckManager:IOSChromePasswordCheckManagerFactory::
                                         GetForBrowserState(browserState)
                    syncSetupService:nil
                       faviconLoader:IOSChromeFaviconLoaderFactory::
                                         GetForBrowserState(browserState)
                     identityManager:IdentityManagerFactory::GetForBrowserState(
                                         browserState)
                         syncService:SyncServiceFactory::GetForBrowserState(
                                         browserState)];

    // Inject some fake passwords to pass the loading state.
    PasswordsTableViewController* passwords_controller =
        static_cast<PasswordsTableViewController*>(controller());
    passwords_controller.delegate = mediator_;
    mediator_.consumer = passwords_controller;
    [passwords_controller setPasswordsForms:{} blockedForms:{}];
  }

  int GetSectionIndex(PasswordSectionIdentifier section) {
    switch (section) {
      case SectionIdentifierSavePasswordsSwitch:
        return 0;
      case SectionIdentifierPasswordsInOtherApps:
        return 1;
      case SectionIdentifierPasswordCheck:
        return 2;
      case SectionIdentifierSavedPasswords:
        return 3;
      case SectionIdentifierBlocked:
        return 4;
      case SectionIdentifierExportPasswordsButton:
        return 4;
      case SectionIdentifierOnDeviceEncryption:
      default:
        // Currently not used in any test.
        // TODO(crbug.com/1323240)
        NOTREACHED();
        return -1;
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
    NSInteger count = 0;
    for (const auto& signon_realm_forms : GetTestStore().stored_passwords()) {
      count += base::ranges::count_if(signon_realm_forms.second,
                                      [](const PasswordForm& form) {
                                        return !form.password_issues.empty();
                                      });
    }

    [passwords_controller setPasswordCheckUIState:state
                 unmutedCompromisedPasswordsCount:count];
  }

  // Adds a form to PasswordsTableViewController.
  void AddPasswordForm(std::unique_ptr<password_manager::PasswordForm> form) {
    GetTestStore().AddLogin(*form);
    RunUntilIdle();
  }

  // Creates and adds a saved password form.  If `is_leaked` is true it marks
  // the credential as leaked.
  void AddSavedForm1(bool is_leaked = false) {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL("http://www.example.com/accounts/LoginAuth");
    form->action = GURL("http://www.example.com/accounts/Login");
    form->username_element = u"Email";
    form->username_value = u"test@egmail.com";
    form->password_element = u"Passwd";
    form->password_value = u"test";
    form->submit_element = u"signIn";
    form->signon_realm = "http://www.example.com/";
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->blocked_by_user = false;

    if (is_leaked) {
      form->password_issues = {
          {InsecureType::kLeaked,
           password_manager::InsecurityMetadata(
               base::Time::Now(), password_manager::IsMuted(false))}};
    }
    AddPasswordForm(std::move(form));
  }

  // Creates and adds a saved password form.
  void AddSavedForm2() {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL("http://www.example2.com/accounts/LoginAuth");
    form->action = GURL("http://www.example2.com/accounts/Login");
    form->username_element = u"Email";
    form->username_value = u"test@egmail.com";
    form->password_element = u"Passwd";
    form->password_value = u"test";
    form->submit_element = u"signIn";
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
    form->username_element = u"email";
    form->username_value = u"test@secret.com";
    form->password_element = u"password";
    form->password_value = u"cantsay";
    form->submit_element = u"signIn";
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
    form->username_element = u"email";
    form->username_value = u"test@secret2.com";
    form->password_element = u"password";
    form->password_value = u"cantsay";
    form->submit_element = u"signIn";
    form->signon_realm = "http://www.secret2.com/";
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->blocked_by_user = true;
    AddPasswordForm(std::move(form));
  }

  // Deletes the item at (row, section) and wait util idle.
  void deleteItemAndWait(int section, int row) {
    PasswordsTableViewController* passwords_controller =
        static_cast<PasswordsTableViewController*>(controller());
    [passwords_controller
        deleteItems:@[ [NSIndexPath indexPathForRow:row inSection:section] ]];
    RunUntilIdle();
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

  // Enables/Disables the edit mode based on |editing|.
  void SetEditing(bool editing) {
    PasswordsTableViewController* passwords_controller =
        static_cast<PasswordsTableViewController*>(controller());
    [passwords_controller setEditing:editing animated:NO];
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  TestPasswordsMediator* mediator_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
};

// Tests default case has no saved sites and no blocked sites.
TEST_F(PasswordsTableViewControllerTest, TestInitialization) {
  CheckController();
  EXPECT_EQ(3 + SectionsOffset(), NumberOfSections());
}

// Tests adding one item in saved password section.
TEST_F(PasswordsTableViewControllerTest, AddSavedPasswords) {
  AddSavedForm1();

  EXPECT_EQ(4 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));
}

// Tests adding one item in blocked password section.
TEST_F(PasswordsTableViewControllerTest, AddBlockedPasswords) {
  AddBlockedForm1();

  EXPECT_EQ(4 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1,
            NumberOfItemsInSection(GetSectionIndex(SectionIdentifierBlocked)));
}

// Tests adding one item in saved password section, and two items in blocked
// password section.
TEST_F(PasswordsTableViewControllerTest, AddSavedAndBlocked) {
  AddSavedForm1();
  AddBlockedForm1();
  AddBlockedForm2();

  // There should be two sections added.
  EXPECT_EQ(5 + SectionsOffset(), NumberOfSections());

  // There should be 1 row in saved password section.
  EXPECT_EQ(1, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));
  // There should be 2 rows in blocked password section.
  EXPECT_EQ(2,
            NumberOfItemsInSection(GetSectionIndex(SectionIdentifierBlocked)));
}

// Tests the order in which the saved passwords are displayed.
TEST_F(PasswordsTableViewControllerTest, TestSavedPasswordsOrder) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kEnableFaviconForPasswords);

  AddSavedForm2();

  CheckURLCellTitleAndDetailText(
      @"example2.com", @"test@egmail.com",
      GetSectionIndex(SectionIdentifierSavedPasswords), 0);

  AddSavedForm1();
  CheckURLCellTitleAndDetailText(
      @"example.com", @"test@egmail.com",
      GetSectionIndex(SectionIdentifierSavedPasswords), 0);
  CheckURLCellTitleAndDetailText(
      @"example2.com", @"test@egmail.com",
      GetSectionIndex(SectionIdentifierSavedPasswords), 1);
}

// Tests the order in which the blocked passwords are displayed.
TEST_F(PasswordsTableViewControllerTest, TestBlockedPasswordsOrder) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kEnableFaviconForPasswords);

  AddBlockedForm2();
  CheckURLCellEmptyTitle(@"secret2.com",
                         GetSectionIndex(SectionIdentifierSavedPasswords), 0);

  AddBlockedForm1();
  CheckURLCellEmptyTitle(@"secret.com",
                         GetSectionIndex(SectionIdentifierSavedPasswords), 0);
  CheckURLCellEmptyTitle(@"secret2.com",
                         GetSectionIndex(SectionIdentifierSavedPasswords), 1);
}

// Tests the order in which the saved passwords are displayed.
// TODO(crbug.com/1300569): Remove this when kEnableFaviconForPasswords flag is
// removed.
TEST_F(PasswordsTableViewControllerTest, TestSavedPasswordsOrderLegacy) {
  AddSavedForm2();

  CheckTextCellTextAndDetailText(
      @"example2.com", @"test@egmail.com",
      GetSectionIndex(SectionIdentifierSavedPasswords), 0);

  AddSavedForm1();
  CheckTextCellTextAndDetailText(
      @"example.com", @"test@egmail.com",
      GetSectionIndex(SectionIdentifierSavedPasswords), 0);
  CheckTextCellTextAndDetailText(
      @"example2.com", @"test@egmail.com",
      GetSectionIndex(SectionIdentifierSavedPasswords), 1);
}

// Tests the order in which the blocked passwords are displayed.
// TODO(crbug.com/1300569): Remove this when kEnableFaviconForPasswords flag is
// removed.
TEST_F(PasswordsTableViewControllerTest, TestBlockedPasswordsOrderLegacy) {
  AddBlockedForm2();
  CheckTextCellText(@"secret2.com",
                    GetSectionIndex(SectionIdentifierSavedPasswords), 0);

  AddBlockedForm1();
  CheckTextCellText(@"secret.com",
                    GetSectionIndex(SectionIdentifierSavedPasswords), 0);
  CheckTextCellText(@"secret2.com",
                    GetSectionIndex(SectionIdentifierSavedPasswords), 1);
}

// Tests displaying passwords in the saved passwords section when there are
// duplicates in the password store.
TEST_F(PasswordsTableViewControllerTest, AddSavedDuplicates) {
  AddSavedForm1();
  AddSavedForm1();

  EXPECT_EQ(4 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));
}

// Tests displaying passwords in the blocked passwords section when there
// are duplicates in the password store.
TEST_F(PasswordsTableViewControllerTest, AddBlockedDuplicates) {
  AddBlockedForm1();
  AddBlockedForm1();

  EXPECT_EQ(4 + SectionsOffset(), NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));
}

// Tests deleting items from saved passwords and blocked passwords sections.
TEST_F(PasswordsTableViewControllerTest, DeleteItems) {
  AddSavedForm1();
  AddBlockedForm1();
  AddBlockedForm2();
  ASSERT_EQ(6, NumberOfSections());

  // Delete item in save passwords section.
  deleteItemAndWait(GetSectionIndex(SectionIdentifierSavedPasswords), 0);
  EXPECT_EQ(5, NumberOfSections());

  // Section 2 should now be the blocked passwords section, and should still
  // have both its items.
  EXPECT_EQ(2, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));

  // Delete item in blocked passwords section.
  deleteItemAndWait(GetSectionIndex(SectionIdentifierSavedPasswords), 0);
  EXPECT_EQ(1, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));

  // There should be no password sections remaining and no search bar.
  deleteItemAndWait(GetSectionIndex(SectionIdentifierSavedPasswords), 0);
  EXPECT_EQ(4, NumberOfSections());
}

// Tests deleting items from saved passwords and blocked passwords sections
// when there are duplicates in the store.
TEST_F(PasswordsTableViewControllerTest, DeleteItemsWithDuplicates) {
  AddSavedForm1();
  AddSavedForm1();
  AddBlockedForm1();
  AddBlockedForm1();
  AddBlockedForm2();
  ASSERT_EQ(6, NumberOfSections());

  // Delete item in save passwords section.
  deleteItemAndWait(GetSectionIndex(SectionIdentifierSavedPasswords), 0);
  EXPECT_EQ(5, NumberOfSections());

  // Section 2 should now be the blocked passwords section, and should still
  // have both its items.
  EXPECT_EQ(
      2, NumberOfItemsInSection(GetSectionIndex(SectionIdentifierBlocked) - 1));

  // Delete item in blocked passwords section.
  deleteItemAndWait(GetSectionIndex(SectionIdentifierBlocked) - 1, 0);
  EXPECT_EQ(
      1, NumberOfItemsInSection(GetSectionIndex(SectionIdentifierBlocked) - 1));

  // There should be no password sections remaining and no search bar.
  deleteItemAndWait(GetSectionIndex(SectionIdentifierBlocked) - 1, 0);
  EXPECT_EQ(4, NumberOfSections());
}

TEST_F(PasswordsTableViewControllerTest,
       TestExportButtonDisabledNoSavedPasswords) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton =
      GetTableViewItem(GetSectionIndex(SectionIdentifierSavedPasswords), 0);
  CheckTextCellTextWithId(IDS_IOS_EXPORT_PASSWORDS,
                          GetSectionIndex(SectionIdentifierSavedPasswords), 0);

  EXPECT_NSEQ([UIColor colorNamed:kTextSecondaryColor], exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);

  // Add blocked form.
  AddBlockedForm1();
  // The export button should still be disabled as exporting blocked forms
  // is not currently supported.
  EXPECT_NSEQ([UIColor colorNamed:kTextSecondaryColor], exportButton.textColor);
  EXPECT_TRUE(exportButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);
}

TEST_F(PasswordsTableViewControllerTest,
       TestExportButtonEnabledWithSavedPasswords) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  AddSavedForm1();
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton = GetTableViewItem(
      GetSectionIndex(SectionIdentifierExportPasswordsButton), 0);

  CheckTextCellTextWithId(
      IDS_IOS_EXPORT_PASSWORDS,
      GetSectionIndex(SectionIdentifierExportPasswordsButton), 0);

  EXPECT_NSEQ([UIColor colorNamed:kBlueColor], exportButton.textColor);
  EXPECT_FALSE(exportButton.accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

// Tests that adding "on device encryption" don’t break during search.
TEST_F(PasswordsTableViewControllerTest, TestOnDeviceEncryptionWhileSearching) {
  root_view_controller_ = [[UIViewController alloc] init];
  scoped_window_.Get().rootViewController = root_view_controller_;

  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());

  // Present the view controller.
  __block bool presentation_finished = NO;
  UINavigationController* navigation_controller =
      [[UINavigationController alloc]
          initWithRootViewController:passwords_controller];
  [root_view_controller_ presentViewController:navigation_controller
                                      animated:NO
                                    completion:^{
                                      presentation_finished = YES;
                                    }];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return presentation_finished;
      }));

  // Disable on device encryption to prepare the state.
  mediator_.encryptionState = OnDeviceEncryptionStateNotShown;
  [passwords_controller updateOnDeviceEncryptionSessionAndUpdateTableView];

  // start of the actual test.
  passwords_controller.navigationItem.searchController.active = YES;
  mediator_.encryptionState = OnDeviceEncryptionStateOptedIn;
  [passwords_controller updateOnDeviceEncryptionSessionAndUpdateTableView];

  passwords_controller.navigationItem.searchController.active = NO;
  // Dismiss |view_controller_| and waits for the dismissal to finish.
  __block bool dismissal_finished = NO;
  [root_view_controller_ dismissViewControllerAnimated:NO
                                            completion:^{
                                              dismissal_finished = YES;
                                            }];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return dismissal_finished;
      }));
}

// Tests that the "Export Passwords..." button is greyed out in edit mode.
TEST_F(PasswordsTableViewControllerTest, TestExportButtonDisabledEditMode) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  AddSavedForm1();
  [passwords_controller updateExportPasswordsButton];

  TableViewDetailTextItem* exportButton = GetTableViewItem(
      GetSectionIndex(SectionIdentifierExportPasswordsButton), 0);
  CheckTextCellTextWithId(
      IDS_IOS_EXPORT_PASSWORDS,
      GetSectionIndex(SectionIdentifierExportPasswordsButton), 0);

  [passwords_controller setEditing:YES animated:NO];

  EXPECT_NSEQ([UIColor colorNamed:kTextSecondaryColor], exportButton.textColor);
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

  TableViewDetailTextItem* exportButton = GetTableViewItem(
      GetSectionIndex(SectionIdentifierExportPasswordsButton), 0);
  CheckTextCellTextWithId(
      IDS_IOS_EXPORT_PASSWORDS,
      GetSectionIndex(SectionIdentifierExportPasswordsButton), 0);

  [passwords_controller setEditing:YES animated:NO];
  [passwords_controller setEditing:NO animated:NO];

  EXPECT_NSEQ([UIColor colorNamed:kBlueColor], exportButton.textColor);
  EXPECT_FALSE(exportButton.accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

// Tests that the "Check Now" button is greyed out in edit mode.
TEST_F(PasswordsTableViewControllerTest,
       TestCheckPasswordButtonDisabledEditMode) {
  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());
  AddSavedForm1();

  TableViewDetailTextItem* checkPasswordButton =
      GetTableViewItem(GetSectionIndex(SectionIdentifierPasswordCheck), 1);
  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(SectionIdentifierPasswordCheck), 1);

  [passwords_controller setEditing:YES animated:NO];

  EXPECT_NSEQ([UIColor colorNamed:kTextSecondaryColor],
              checkPasswordButton.textColor);
  EXPECT_TRUE(checkPasswordButton.accessibilityTraits &
              UIAccessibilityTraitNotEnabled);

  [passwords_controller setEditing:NO animated:NO];
  EXPECT_NSEQ([UIColor colorNamed:kBlueColor], checkPasswordButton.textColor);
  EXPECT_FALSE(checkPasswordButton.accessibilityTraits &
               UIAccessibilityTraitNotEnabled);
}

// Tests filtering of items.
TEST_F(PasswordsTableViewControllerTest, FilterItems) {
  AddSavedForm1();
  AddSavedForm2();
  AddBlockedForm1();
  AddBlockedForm2();

  EXPECT_EQ(5 + SectionsOffset(), NumberOfSections());

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
  EXPECT_EQ(1, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));
  EXPECT_EQ(0,
            NumberOfItemsInSection(GetSectionIndex(SectionIdentifierBlocked)));
  CheckTextCellTextAndDetailText(
      @"example.com", @"test@egmail.com",
      GetSectionIndex(SectionIdentifierSavedPasswords), 0);

  [passwords_controller searchBar:bar textDidChange:@"test@egmail.com"];
  // Only two items in saved passwords should remain.
  EXPECT_EQ(2, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));
  EXPECT_EQ(0,
            NumberOfItemsInSection(GetSectionIndex(SectionIdentifierBlocked)));
  CheckTextCellTextAndDetailText(
      @"example.com", @"test@egmail.com",
      GetSectionIndex(SectionIdentifierSavedPasswords), 0);
  CheckTextCellTextAndDetailText(
      @"example2.com", @"test@egmail.com",
      GetSectionIndex(SectionIdentifierSavedPasswords), 1);

  [passwords_controller searchBar:bar textDidChange:@"secret"];
  // Only two blocked items should remain.
  EXPECT_EQ(0, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));
  EXPECT_EQ(2,
            NumberOfItemsInSection(GetSectionIndex(SectionIdentifierBlocked)));
  CheckTextCellText(@"secret.com", GetSectionIndex(SectionIdentifierBlocked),
                    0);
  CheckTextCellText(@"secret2.com", GetSectionIndex(SectionIdentifierBlocked),
                    1);

  [passwords_controller searchBar:bar textDidChange:@""];
  // All items should be back.
  EXPECT_EQ(2, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));
  EXPECT_EQ(2,
            NumberOfItemsInSection(GetSectionIndex(SectionIdentifierBlocked)));
}

// Test verifies disabled state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateDisabled) {
  ChangePasswordCheckState(PasswordCheckStateDisabled);

  CheckDetailItemTextWithIds(
      IDS_IOS_CHECK_PASSWORDS, IDS_IOS_CHECK_PASSWORDS_DESCRIPTION,
      GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  EXPECT_FALSE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);

  SetEditing(true);
  EXPECT_FALSE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);
}

// Test verifies default state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateDefault) {
  ChangePasswordCheckState(PasswordCheckStateDefault);

  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(SectionIdentifierPasswordCheck), 1);
  CheckDetailItemTextWithIds(
      IDS_IOS_CHECK_PASSWORDS, IDS_IOS_CHECK_PASSWORDS_DESCRIPTION,
      GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);

  SetEditing(true);
  EXPECT_FALSE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);
}

// Test verifies safe state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateSafe) {
  ChangePasswordCheckState(PasswordCheckStateSafe);

  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(SectionIdentifierPasswordCheck), 1);
  CheckDetailItemTextWithPluralIds(
      IDS_IOS_CHECK_PASSWORDS, IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT, 0,
      GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_TRUE(checkPassword.trailingImage);

  SetEditing(true);
  EXPECT_FALSE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_TRUE(checkPassword.trailingImage);
}

// Test verifies unsafe state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateUnSafe) {
  AddSavedForm1(/*has_password_issues=*/true);
  ChangePasswordCheckState(PasswordCheckStateUnSafe);

  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(SectionIdentifierPasswordCheck), 1);
  CheckDetailItemTextWithPluralIds(
      IDS_IOS_CHECK_PASSWORDS, IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT, 1,
      GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_TRUE(checkPassword.trailingImage);

  SetEditing(true);
  EXPECT_FALSE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_TRUE(checkPassword.trailingImage);
}

// Test verifies running state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateRunning) {
  ChangePasswordCheckState(PasswordCheckStateRunning);

  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(SectionIdentifierPasswordCheck), 1);
  CheckDetailItemTextWithIds(
      IDS_IOS_CHECK_PASSWORDS, IDS_IOS_CHECK_PASSWORDS_DESCRIPTION,
      GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_FALSE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);

  SetEditing(true);
  EXPECT_FALSE(checkPassword.enabled);
  EXPECT_FALSE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);
}

// Test verifies error state of password check cell.
TEST_F(PasswordsTableViewControllerTest, PasswordCheckStateError) {
  ChangePasswordCheckState(PasswordCheckStateError);

  CheckTextCellTextWithId(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON,
                          GetSectionIndex(SectionIdentifierPasswordCheck), 1);
  CheckDetailItemTextWithIds(
      IDS_IOS_CHECK_PASSWORDS, IDS_IOS_PASSWORD_CHECK_ERROR,
      GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  SettingsCheckItem* checkPassword =
      GetTableViewItem(GetSectionIndex(SectionIdentifierPasswordCheck), 0);
  EXPECT_TRUE(checkPassword.enabled);
  EXPECT_TRUE(checkPassword.indicatorHidden);
  EXPECT_FALSE(checkPassword.trailingImage);
  EXPECT_FALSE(checkPassword.infoButtonHidden);

  SetEditing(true);
  EXPECT_FALSE(checkPassword.enabled);
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

  [passwords_controller
                    tableView:passwords_controller.tableView
      didSelectRowAtIndexPath:
          [NSIndexPath indexPathForItem:1
                              inSection:GetSectionIndex(
                                            SectionIdentifierPasswordCheck)]];
}

// Test verifies tapping start triggers correct function in service.
TEST_F(PasswordsTableViewControllerTest, StartPasswordCheck) {
  AddSavedForm1();
  RunUntilIdle();

  PasswordsTableViewController* passwords_controller =
      static_cast<PasswordsTableViewController*>(controller());

  EXPECT_CALL(GetMockPasswordCheckService(), CheckUsernamePasswordPairs);

  [passwords_controller
                    tableView:passwords_controller.tableView
      didSelectRowAtIndexPath:
          [NSIndexPath indexPathForItem:1
                              inSection:GetSectionIndex(
                                            SectionIdentifierPasswordCheck)]];
}

// Test verifies changes to the password store are reflected on UI.
TEST_F(PasswordsTableViewControllerTest, PasswordStoreListener) {
  AddSavedForm1();
  EXPECT_EQ(1, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));
  AddSavedForm2();
  EXPECT_EQ(2, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));

  auto password =
      GetTestStore().stored_passwords().at("http://www.example.com/").at(0);
  GetTestStore().RemoveLogin(password);
  RunUntilIdle();
  EXPECT_EQ(1, NumberOfItemsInSection(
                   GetSectionIndex(SectionIdentifierSavedPasswords)));
}

}  // namespace
