// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"

#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_utils.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::PasswordForm;
using password_manager::TestPasswordStore;

// Test fixture for testing PasswordCheckupViewController class.
class PasswordCheckupViewControllerTest : public ChromeTableViewControllerTest {
 protected:
  PasswordCheckupViewControllerTest() = default;

  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<web::BrowserState,
                                                  TestPasswordStore>));
    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    CreateController();
  }

  TestPasswordStore& GetTestStore() {
    return *static_cast<TestPasswordStore*>(
        IOSChromePasswordStoreFactory::GetForBrowserState(
            browser_->GetBrowserState(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  ChromeTableViewController* InstantiateController() override {
    return [[PasswordCheckupViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
  }

  PasswordCheckupViewController* GetPasswordCheckupViewController() {
    return static_cast<PasswordCheckupViewController*>(controller());
  }

  // Changes the PasswordCheckupHomepageState.
  void ChangePasswordCheckupHomepageState(PasswordCheckupHomepageState state) {
    PasswordCheckupViewController* view_controller =
        GetPasswordCheckupViewController();

    password_manager::InsecurePasswordCounts counts = {};
    for (const auto& signon_realm_forms : GetTestStore().stored_passwords()) {
      for (const PasswordForm& form : signon_realm_forms.second) {
        CredentialUIEntry credential = CredentialUIEntry(form);
        if (credential.IsMuted()) {
          counts.dismissed_count++;
        } else if (IsCompromised(credential)) {
          counts.compromised_count++;
        }
        if (credential.IsReused()) {
          counts.reused_count++;
        }
        if (credential.IsWeak()) {
          counts.weak_count++;
        }
      }
    }

    [view_controller setPasswordCheckupHomepageState:state
                              insecurePasswordCounts:counts];
  }

  // Adds a form to the test password store.
  void AddPasswordForm(std::unique_ptr<password_manager::PasswordForm> form) {
    GetTestStore().AddLogin(*form);
    RunUntilIdle();
  }

  // Creates and adds a saved insecure password form.
  void AddSavedInsecureForm(InsecureType insecure_type,
                            bool is_muted = false,
                            std::string url = "http://www.example1.com/") {
    auto form = std::make_unique<password_manager::PasswordForm>();
    form->url = GURL(url);
    form->username_element = u"Email";
    form->username_value = u"test@egmail.com";
    form->password_element = u"Passwd";
    form->password_value = u"test";
    form->signon_realm = url;
    form->scheme = password_manager::PasswordForm::Scheme::kHtml;
    form->in_store = password_manager::PasswordForm::Store::kProfileStore;
    form->password_issues = {
        {insecure_type,
         password_manager::InsecurityMetadata(
             base::Time::Now(), password_manager::IsMuted(is_muted))}};
    AddPasswordForm(std::move(form));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
};

// Tests the running state of the Password Checkup homepage.
TEST_F(PasswordCheckupViewControllerTest, PasswordCheckupHomepageStateRunning) {
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateRunning);

  UIImageView* headerImageView =
      (UIImageView*)GetPasswordCheckupViewController()
          .tableView.tableHeaderView;
  EXPECT_NSEQ([UIImage imageNamed:@"password_checkup_header_loading"],
              headerImageView.image);
  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Tests the "done" state of the Password Checkup homepage with no insecure
// passwords.
TEST_F(PasswordCheckupViewControllerTest, PasswordCheckupHomepageStateSafe) {
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  UIImageView* headerImageView =
      (UIImageView*)GetPasswordCheckupViewController()
          .tableView.tableHeaderView;
  EXPECT_NSEQ([UIImage imageNamed:@"password_checkup_header_green"],
              headerImageView.image);
  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Tests the "done" state of the Password Checkup homepage with compromised
// passwords.
TEST_F(PasswordCheckupViewControllerTest,
       PasswordCheckupHomepageStateWithCompromisedPasswords) {
  AddSavedInsecureForm(InsecureType::kLeaked);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  UIImageView* headerImageView =
      (UIImageView*)GetPasswordCheckupViewController()
          .tableView.tableHeaderView;
  EXPECT_NSEQ([UIImage imageNamed:@"password_checkup_header_red"],
              headerImageView.image);
  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Tests the "done" state of the Password Checkup homepage with muted
// compromised passwords.
TEST_F(PasswordCheckupViewControllerTest,
       PasswordCheckupHomepageStateWithMutedCompromisedPasswords) {
  AddSavedInsecureForm(InsecureType::kLeaked, /*is_muted=*/true);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  UIImageView* headerImageView =
      (UIImageView*)GetPasswordCheckupViewController()
          .tableView.tableHeaderView;
  EXPECT_NSEQ([UIImage imageNamed:@"password_checkup_header_yellow"],
              headerImageView.image);
  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Tests the "done" state of the Password Checkup homepage with reused
// passwords.
TEST_F(PasswordCheckupViewControllerTest,
       PasswordCheckupHomepageStateWithReusedPasswords) {
  AddSavedInsecureForm(InsecureType::kReused);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  UIImageView* headerImageView =
      (UIImageView*)GetPasswordCheckupViewController()
          .tableView.tableHeaderView;
  EXPECT_NSEQ([UIImage imageNamed:@"password_checkup_header_yellow"],
              headerImageView.image);
  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}

// Tests the "done" state of the Password Checkup homepage with weak passwords.
TEST_F(PasswordCheckupViewControllerTest,
       PasswordCheckupHomepageStateWithWeakPasswords) {
  AddSavedInsecureForm(InsecureType::kWeak);
  ChangePasswordCheckupHomepageState(PasswordCheckupHomepageStateDone);

  UIImageView* headerImageView =
      (UIImageView*)GetPasswordCheckupViewController()
          .tableView.tableHeaderView;
  EXPECT_NSEQ([UIImage imageNamed:@"password_checkup_header_yellow"],
              headerImageView.image);
  [GetPasswordCheckupViewController() settingsWillBeDismissed];
}
