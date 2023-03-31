// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_table_view_controller.h"

#import <memory>

#import "base/strings/utf_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_presenter.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test class that conforms to PasswordIssuesPresenter in order to test the
// presenter methods are called correctly.
@interface FakePasswordIssuesPresenter : NSObject <PasswordIssuesPresenter>

@property(nonatomic) PasswordIssue* presentedPassword;

@property(nonatomic, assign) BOOL dismissedWarningsPresented;

@end

@implementation FakePasswordIssuesPresenter

- (void)dismissPasswordIssuesTableViewController {
}

- (void)presentPasswordIssueDetails:(PasswordIssue*)password {
  _presentedPassword = password;
}

- (void)dismissAndOpenURL:(CrURL*)URL {
  // TODO(crbug.com/1419986): Add unit test checking the right url was passed
  // after tapping the header's link.
}

- (void)presentDismissedCompromisedCredentials {
  _dismissedWarningsPresented = YES;
}

@end

// Unit tests for PasswordIssuesTableViewController.
class PasswordIssuesTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  PasswordIssuesTableViewControllerTest() {
    presenter_ = [[FakePasswordIssuesPresenter alloc] init];
  }

  ChromeTableViewController* InstantiateController() override {
    PasswordIssuesTableViewController* controller =
        [[PasswordIssuesTableViewController alloc]
            initWithStyle:UITableViewStylePlain];
    controller.presenter = presenter_;
    return controller;
  }

  // Adds password issue to the view controller.
  void AddPasswordIssue(NSString* dismissed_warnings_button_text = nil) {
    auto form = password_manager::PasswordForm();
    form.url = GURL("http://www.example.com/accounts/LoginAuth");
    form.action = GURL("http://www.example.com/accounts/Login");
    form.username_element = u"Email";
    form.username_value = u"test@egmail.com";
    form.password_element = u"Passwd";
    form.password_value = u"test";
    form.submit_element = u"signIn";
    form.signon_realm = "http://www.example.com/";
    form.scheme = password_manager::PasswordForm::Scheme::kHtml;
    PasswordIssue* password_issue = [[PasswordIssue alloc]
                  initWithCredential:password_manager::CredentialUIEntry(form)
        enableCompromisedDescription:NO];
    PasswordIssueGroup* issue_group =
        [[PasswordIssueGroup alloc] initWithHeaderText:nil
                                        passwordIssues:@[ password_issue ]];

    PasswordIssuesTableViewController* passwords_controller =
        static_cast<PasswordIssuesTableViewController*>(controller());
    [passwords_controller setPasswordIssues:@[ issue_group ]
                dismissedWarningsButtonText:dismissed_warnings_button_text];
  }

  FakePasswordIssuesPresenter* presenter() { return presenter_; }

 private:
  FakePasswordIssuesPresenter* presenter_;
};

// Tests PasswordIssuesViewController is set up with appropriate items
// and sections.
TEST_F(PasswordIssuesTableViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  EXPECT_EQ(1, NumberOfSections());

  EXPECT_EQ(0, NumberOfItemsInSection(0));
}

// Test verifies password issue is displayed correctly.
TEST_F(PasswordIssuesTableViewControllerTest, TestPasswordIssue) {
  CreateController();
  AddPasswordIssue();
  EXPECT_EQ(1, NumberOfSections());

  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckURLCellTitleAndDetailText(@"example.com", @"test@egmail.com", 0, 0);
}

// Test verifies tapping item triggers function in presenter.
TEST_F(PasswordIssuesTableViewControllerTest, TestPasswordIssueSelection) {
  CreateController();
  AddPasswordIssue();

  PasswordIssuesTableViewController* passwords_controller =
      static_cast<PasswordIssuesTableViewController*>(controller());

  EXPECT_FALSE(presenter().presentedPassword);
  [passwords_controller tableView:passwords_controller.tableView
          didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]];
  EXPECT_TRUE(presenter().presentedPassword);
  EXPECT_NSEQ(@"example.com", presenter().presentedPassword.website);
  EXPECT_NSEQ(@"test@egmail.com", presenter().presentedPassword.username);
}

// Test verifies tapping dismiss warnings button triggers function in presenter.
TEST_F(PasswordIssuesTableViewControllerTest, TestDismissWarningsTap) {
  base::test::ScopedFeatureList feature_list(
      password_manager::features::kIOSPasswordCheckup);

  CreateController();
  AddPasswordIssue(@"Dismiss Warnings (1)");

  PasswordIssuesTableViewController* passwords_controller =
      static_cast<PasswordIssuesTableViewController*>(controller());

  EXPECT_FALSE(presenter().dismissedWarningsPresented);
  [passwords_controller tableView:passwords_controller.tableView
          didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];
  EXPECT_TRUE(presenter().dismissedWarningsPresented);
}
