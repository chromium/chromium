// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_table_view_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/strings/utf_string_conversions.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issue.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_presenter.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Creates a test password issue.
PasswordIssue* CreateTestPasswordIssue() {
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
  return [[PasswordIssue alloc]
                initWithCredential:password_manager::CredentialUIEntry(form)
      enableCompromisedDescription:NO];
}

// Creates a second test password issue.
PasswordIssue* CreateTestPasswordIssue2() {
  auto form = password_manager::PasswordForm();
  form.url = GURL("http://www.example2.com/accounts/LoginAuth");
  form.action = GURL("http://www.example2.com/accounts/Login");
  form.username_element = u"Email";
  form.username_value = u"test@egmail.com";
  form.password_element = u"Passwd";
  form.password_value = u"test";
  form.submit_element = u"signIn";
  form.signon_realm = "http://www.example2.com/";
  form.scheme = password_manager::PasswordForm::Scheme::kHtml;
  return [[PasswordIssue alloc]
                initWithCredential:password_manager::CredentialUIEntry(form)
      enableCompromisedDescription:NO];
}

// Text for testing the header of the page.
NSString* GetHeaderText() {
  return l10n_util::GetNSString(IDS_IOS_WEAK_PASSWORD_ISSUES_DESCRIPTION);
}

// URL for testing the header of the page.
CrURL* GetHeaderURL() {
  return [[CrURL alloc]
      initWithGURL:GURL(
                       password_manager::
                           kPasswordManagerHelpCenterCreateStrongPasswordsURL)];
}

}  // namespace

// Test class that conforms to PasswordIssuesPresenter in order to test the
// presenter methods are called correctly.
@interface FakePasswordIssuesPresenter : NSObject <PasswordIssuesPresenter>

@property(nonatomic) PasswordIssue* presentedPassword;

@property(nonatomic, assign) BOOL dismissedWarningsPresented;

@property(nonatomic, strong) CrURL* openedURL;

@property(nonatomic, assign) BOOL dismissalTriggered;

@end

@implementation FakePasswordIssuesPresenter

- (void)dismissPasswordIssuesTableViewController {
}

- (void)presentPasswordIssueDetails:(PasswordIssue*)password {
  _presentedPassword = password;
}

- (void)dismissAndOpenURL:(CrURL*)URL {
  _openedURL = URL;
}

- (void)presentDismissedCompromisedCredentials {
  _dismissedWarningsPresented = YES;
}

- (void)dismissAfterAllIssuesGone {
  _dismissalTriggered = YES;
}

@end

// Unit tests for PasswordIssuesTableViewController.
class PasswordIssuesTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  PasswordIssuesTableViewControllerTest() {
    presenter_ = [[FakePasswordIssuesPresenter alloc] init];
  }

  LegacyChromeTableViewController* InstantiateController() override {
    PasswordIssuesTableViewController* controller =
        [[PasswordIssuesTableViewController alloc]
            initWithWarningType:password_manager::WarningType::
                                    kCompromisedPasswordsWarning];
    controller.presenter = presenter_;
    return controller;
  }

  PasswordIssuesTableViewController* GetPasswordIssuesController() {
    return static_cast<PasswordIssuesTableViewController*>(controller());
  }

  // Adds password issue to the view controller.
  void AddPasswordIssue() {
    SetIssuesAndDismissedWarningsCount(@[ [[PasswordIssueGroup alloc]
        initWithHeaderText:nil
            passwordIssues:@[ CreateTestPasswordIssue() ]] ]);
  }

  // Passes the given PasswordIssues and text for dismissed warnings button to
  // the view controller.
  void SetIssuesAndDismissedWarningsCount(
      NSArray<PasswordIssueGroup*>* password_issue_groups,
      NSInteger dismissed_warnings_count = 0) {
    PasswordIssuesTableViewController* passwords_controller =
        static_cast<PasswordIssuesTableViewController*>(controller());
    [passwords_controller setPasswordIssues:password_issue_groups
                     dismissedWarningsCount:dismissed_warnings_count];
  }

  // Sets the PasswordPasswordIssuesTableViewController header.
  void SetHeader(NSString* header_text, CrURL* header_url) {
    PasswordIssuesTableViewController* passwords_controller =
        GetPasswordIssuesController();

    [passwords_controller setHeader:header_text URL:header_url];
  }

  // Verifies that a header with the given text and url is in the model.
  void CheckHeader(NSString* expected_text,
                   CrURL* expected_url = nil,
                   int section = 0) {
    PasswordIssuesTableViewController* passwords_controller =
        GetPasswordIssuesController();
    TableViewModel* model = passwords_controller.tableViewModel;

    TableViewLinkHeaderFooterItem* header =
        base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(
            [model headerForSectionIndex:section]);

    EXPECT_NSEQ(header.text, expected_text);
    if (expected_url != nil) {
      EXPECT_NSEQ(header.urls, @[ expected_url ]);
    } else {
      EXPECT_FALSE(header.urls.count);
    }
  }

  // Verifies that the item for the dismissed warnings button is in the model
  // with the right content.
  void CheckDismissedWarningsButton(NSInteger expected_count, int section) {
    TableViewMultiDetailTextItem* dismissed_warnings_button_item =
        GetTableViewItem(/*section=*/section, /*item=*/0);
    // Validate button text.
    EXPECT_NSEQ(@"Dismissed warnings", dismissed_warnings_button_item.text);
    // Validate count.
    EXPECT_NSEQ([@(expected_count) stringValue],
                dismissed_warnings_button_item.trailingDetailText);
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
  EXPECT_EQ(0, NumberOfSections());
}

// Test verifies password issue is displayed correctly.
TEST_F(PasswordIssuesTableViewControllerTest, TestPasswordIssue) {
  CreateController();
  AddPasswordIssue();
  EXPECT_EQ(1, NumberOfSections());

  EXPECT_EQ(2, NumberOfItemsInSection(0));
  CheckURLCellTitleAndDetailText(@"example.com", @"test@egmail.com", 0, 0);
  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 0, 1);
}

// Test verifies password issue groups are displayed correctly.
TEST_F(PasswordIssuesTableViewControllerTest, TestPasswordIssueGroup) {
  CreateController();

  // Add two groups with headers and two issues each.
  NSString* first_header_text = @"Group Header 1";
  NSString* second_header_text = @"Group Header 2";
  SetIssuesAndDismissedWarningsCount(@[
    [[PasswordIssueGroup alloc]
        initWithHeaderText:first_header_text
            passwordIssues:@[
              CreateTestPasswordIssue(), CreateTestPasswordIssue2()
            ]],
    [[PasswordIssueGroup alloc]
        initWithHeaderText:second_header_text
            passwordIssues:@[
              CreateTestPasswordIssue(), CreateTestPasswordIssue2()
            ]]
  ]);

  // Model should have one section for each issue.
  EXPECT_EQ(4, NumberOfSections());

  // Verify first issue group.

  // Verify header on top of first issue.
  CheckHeader(/*expected_text=*/first_header_text, /*url=*/nil, /*section=*/0);

  // Verify first issue.
  EXPECT_EQ(2, NumberOfItemsInSection(0));
  CheckURLCellTitleAndDetailText(@"example.com", @"test@egmail.com", 0, 0);
  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 0, 1);

  // Verify no header on top of second issue.
  CheckHeader(/*expected_text=*/nil, /*url=*/nil, /*section=*/1);

  // Verify second issue.
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  CheckURLCellTitleAndDetailText(@"example2.com", @"test@egmail.com", 1, 0);
  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 1, 1);

  // Verify second issue group.

  // Verify header on top of first issue.
  CheckHeader(/*expected_text=*/second_header_text, /*url=*/nil, /*section=*/2);

  // Verify first issue.
  EXPECT_EQ(2, NumberOfItemsInSection(3));
  CheckURLCellTitleAndDetailText(@"example.com", @"test@egmail.com", 2, 0);
  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 2, 1);

  // Verify no header on top of second issue.
  CheckHeader(/*expected_text=*/nil, /*url=*/nil, /*section=*/3);

  // Verify second issue.
  EXPECT_EQ(2, NumberOfItemsInSection(3));
  CheckURLCellTitleAndDetailText(@"example2.com", @"test@egmail.com", 3, 0);
  CheckTextCellTextWithId(IDS_IOS_CHANGE_COMPROMISED_PASSWORD, 3, 1);
}

// Test verifies tapping item triggers function in presenter.
TEST_F(PasswordIssuesTableViewControllerTest, TestPasswordIssueSelection) {
  CreateController();
  AddPasswordIssue();

  PasswordIssuesTableViewController* passwords_controller =
      GetPasswordIssuesController();

  EXPECT_FALSE(presenter().presentedPassword);
  [passwords_controller tableView:passwords_controller.tableView
          didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]];
  EXPECT_TRUE(presenter().presentedPassword);
  EXPECT_NSEQ(@"example.com", presenter().presentedPassword.website);
  EXPECT_NSEQ(@"test@egmail.com", presenter().presentedPassword.username);
}

// Test verifies tapping dismiss warnings button triggers function in presenter.
TEST_F(PasswordIssuesTableViewControllerTest, TestDismissWarningsTap) {
  CreateController();
  SetIssuesAndDismissedWarningsCount(
      @[ [[PasswordIssueGroup alloc]
          initWithHeaderText:nil
              passwordIssues:@[ CreateTestPasswordIssue() ]] ],
      1);

  CheckDismissedWarningsButton(/*expected_count=*/1, /*section=*/1);

  EXPECT_FALSE(presenter().dismissedWarningsPresented);

  PasswordIssuesTableViewController* passwords_controller =
      static_cast<PasswordIssuesTableViewController*>(controller());
  [passwords_controller tableView:passwords_controller.tableView
          didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];
  EXPECT_TRUE(presenter().dismissedWarningsPresented);
}

// Test verifies tapping change password button triggers function in presenter.
TEST_F(PasswordIssuesTableViewControllerTest, TestChangePasswordTap) {
  PasswordIssue* password_issue = CreateTestPasswordIssue();
  SetIssuesAndDismissedWarningsCount(
      @[ [[PasswordIssueGroup alloc] initWithHeaderText:nil
                                         passwordIssues:@[ password_issue ]] ]);

  PasswordIssuesTableViewController* passwords_controller =
      static_cast<PasswordIssuesTableViewController*>(controller());

  EXPECT_FALSE(presenter().openedURL);
  // Tap change website button.
  [passwords_controller tableView:passwords_controller.tableView
          didSelectRowAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:0]];
  EXPECT_NSEQ(presenter().openedURL, password_issue.changePasswordURL.value());
}

// Test verifies removing all issues and dismissed warnings triggers a dismissal
// in the presenter.
TEST_F(PasswordIssuesTableViewControllerTest, TestDismissAfterIssuesGone) {
  CreateController();
  AddPasswordIssue();

  EXPECT_FALSE(presenter().dismissalTriggered);

  // Simulate all content gone.
  SetIssuesAndDismissedWarningsCount(@[]);

  EXPECT_TRUE(presenter().dismissalTriggered);
}

// Test setting the header text and url adds the header item in the model.
TEST_F(PasswordIssuesTableViewControllerTest, TestSetHeader) {
  NSString* header_text = GetHeaderText();
  CrURL* header_url = GetHeaderURL();
  SetHeader(header_text, header_url);
  CheckHeader(header_text, header_url);
}

// Test verifies tapping the link in the header triggers function in presenter.
TEST_F(PasswordIssuesTableViewControllerTest, TestTapHeaderLink) {
  NSString* header_text = GetHeaderText();
  CrURL* header_url = GetHeaderURL();
  SetHeader(header_text, header_url);

  PasswordIssuesTableViewController* passwords_controller =
      GetPasswordIssuesController();

  TableViewLinkHeaderFooterView* header_view =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(
          [passwords_controller tableView:passwords_controller.tableView
                   viewForHeaderInSection:0]);

  // Verify header view has view controller as its delegate.
  // This guarantees taps on the link are forwarded to the view controller.
  EXPECT_NSEQ(header_view.delegate, passwords_controller);

  EXPECT_FALSE(presenter().openedURL);
  // Simulate tap in header link.
  [passwords_controller view:header_view didTapLinkURL:header_url];

  // Verify url is forwarded to presenter.
  EXPECT_NSEQ(presenter().openedURL, header_url);
}
