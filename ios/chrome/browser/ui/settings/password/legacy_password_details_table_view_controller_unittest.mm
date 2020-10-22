// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/legacy_password_details_table_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/app/password_test_util.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* const kSite = @"https://testorigin.com/";
NSString* const kUsername = @"testusername";
NSString* const kPassword = @"testpassword";

// Indices related to the layout for a non-blocked, non-federated password.
const int kSiteSection = 0;
const int kSiteItem = 0;
const int kCopySiteButtonItem = 1;

const int kUsernameSection = 1;
const int kUsernameItem = 0;
const int kCopyUsernameButtonItem = 1;

const int kPasswordSection = 2;
const int kPasswordItem = 0;
const int kCopyPasswordButtonItem = 1;
const int kShowHideButtonItem = 2;

const int kDeleteSection = 3;
const int kDeleteButtonItem = 0;

class LegacyPasswordDetailsTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  LegacyPasswordDetailsTableViewControllerTest() {
    origin_ = kSite;
    form_.username_value = base::SysNSStringToUTF16(kUsername);
    form_.password_value = base::SysNSStringToUTF16(kPassword);
    form_.signon_realm = base::SysNSStringToUTF8(origin_);
    form_.url = GURL(form_.signon_realm);
  }

  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    reauthentication_module_ = [[MockReauthenticationModule alloc] init];
    reauthentication_module_.expectedResult = ReauthenticationResult::kSuccess;
  }

  ChromeTableViewController* InstantiateController() override {
    return [[LegacyPasswordDetailsTableViewController alloc]
          initWithPasswordForm:form_
                      delegate:
                          OCMProtocolMock(@protocol(
                              LegacyPasswordDetailsTableViewControllerDelegate))
        reauthenticationModule:reauthentication_module_];
  }

  web::WebTaskEnvironment task_environment_;
  MockReauthenticationModule* reauthentication_module_;
  NSString* origin_;
  password_manager::PasswordForm form_;
};

TEST_F(LegacyPasswordDetailsTableViewControllerTest,
       TestInitialization_NormalPassword) {
  CreateController();
  CheckController();
  EXPECT_EQ(4, NumberOfSections());
  // Site section
  EXPECT_EQ(2, NumberOfItemsInSection(kSiteSection));
  CheckSectionHeaderWithId(IDS_IOS_SHOW_PASSWORD_VIEW_SITE, kSiteSection);
  TableViewTextItem* siteItem = GetTableViewItem(kSiteSection, kSiteItem);
  EXPECT_NSEQ(origin_, siteItem.text);
  EXPECT_FALSE(siteItem.masked);
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_SITE_COPY_BUTTON, kSiteSection,
                          kCopySiteButtonItem);
  // Username section
  EXPECT_EQ(2, NumberOfItemsInSection(kUsernameSection));
  CheckSectionHeaderWithId(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME,
                           kUsernameSection);
  TableViewTextItem* usernameItem =
      GetTableViewItem(kUsernameSection, kUsernameItem);
  EXPECT_NSEQ(kUsername, usernameItem.text);
  EXPECT_FALSE(usernameItem.masked);
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON,
                          kUsernameSection, kCopyUsernameButtonItem);
  // Password section
  EXPECT_EQ(3, NumberOfItemsInSection(kPasswordSection));
  CheckSectionHeaderWithId(IDS_IOS_SHOW_PASSWORD_VIEW_PASSWORD,
                           kPasswordSection);
  TableViewTextItem* passwordItem =
      GetTableViewItem(kPasswordSection, kPasswordItem);
  EXPECT_NSEQ(kPassword, passwordItem.text);
  EXPECT_TRUE(passwordItem.masked);
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_PASSWORD_COPY_BUTTON,
                          kPasswordSection, kCopyPasswordButtonItem);
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON,
                          kPasswordSection, kShowHideButtonItem);
  // Delete section
  EXPECT_EQ(1, NumberOfItemsInSection(kDeleteSection));
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON,
                          kDeleteSection, kDeleteButtonItem);
}

TEST_F(LegacyPasswordDetailsTableViewControllerTest,
       TestInitialization_Blocked) {
  constexpr int kBlockedSiteSection = 0;
  constexpr int kBlockedSiteItem = 0;
  constexpr int kBlockedCopySiteButtonItem = 1;

  constexpr int kBlockedDeleteSection = 1;
  constexpr int kBlockedDeleteButtonItem = 0;

  form_.username_value.clear();
  form_.password_value.clear();
  form_.blocked_by_user = true;
  CreateController();
  CheckController();
  EXPECT_EQ(2, NumberOfSections());
  // Site section
  EXPECT_EQ(2, NumberOfItemsInSection(kBlockedSiteSection));
  CheckSectionHeaderWithId(IDS_IOS_SHOW_PASSWORD_VIEW_SITE,
                           kBlockedSiteSection);
  TableViewTextItem* siteItem =
      GetTableViewItem(kBlockedSiteSection, kBlockedSiteItem);
  EXPECT_NSEQ(origin_, siteItem.text);
  EXPECT_FALSE(siteItem.masked);
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_SITE_COPY_BUTTON,
                          kBlockedSiteSection, kBlockedCopySiteButtonItem);
  // Delete section
  EXPECT_EQ(1, NumberOfItemsInSection(kBlockedDeleteSection));
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON,
                          kBlockedDeleteSection, kBlockedDeleteButtonItem);
}

TEST_F(LegacyPasswordDetailsTableViewControllerTest,
       TestInitialization_Federated) {
  constexpr int kFederatedSiteSection = 0;
  constexpr int kFederatedSiteItem = 0;
  constexpr int kFederatedCopySiteButtonItem = 1;

  constexpr int kFederatedUsernameSection = 1;
  constexpr int kFederatedUsernameItem = 0;
  constexpr int kFederatedCopyUsernameButtonItem = 1;

  constexpr int kFederatedFederationSection = 2;
  constexpr int kFederatedFederationItem = 0;

  constexpr int kFederatedDeleteSection = 3;
  constexpr int kFederatedDeleteButtonItem = 0;

  form_.password_value.clear();
  form_.federation_origin =
      url::Origin::Create(GURL("https://famous.provider.net"));
  CreateController();
  CheckController();
  EXPECT_EQ(4, NumberOfSections());
  // Site section
  EXPECT_EQ(2, NumberOfItemsInSection(kFederatedSiteSection));
  CheckSectionHeaderWithId(IDS_IOS_SHOW_PASSWORD_VIEW_SITE,
                           kFederatedSiteSection);
  TableViewTextItem* siteItem =
      GetTableViewItem(kFederatedSiteSection, kFederatedSiteItem);
  EXPECT_NSEQ(origin_, siteItem.text);
  EXPECT_FALSE(siteItem.masked);
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_SITE_COPY_BUTTON,
                          kFederatedSiteSection, kFederatedCopySiteButtonItem);
  // Username section
  EXPECT_EQ(2, NumberOfItemsInSection(kFederatedUsernameSection));
  CheckSectionHeaderWithId(IDS_IOS_SHOW_PASSWORD_VIEW_USERNAME,
                           kFederatedUsernameSection);
  TableViewTextItem* usernameItem =
      GetTableViewItem(kFederatedUsernameSection, kFederatedUsernameItem);
  EXPECT_NSEQ(kUsername, usernameItem.text);
  EXPECT_FALSE(usernameItem.masked);
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_USERNAME_COPY_BUTTON,
                          kFederatedUsernameSection,
                          kFederatedCopyUsernameButtonItem);
  // Federated section
  EXPECT_EQ(1, NumberOfItemsInSection(kFederatedFederationSection));
  CheckSectionHeaderWithId(IDS_IOS_SHOW_PASSWORD_VIEW_FEDERATION,
                           kFederatedFederationSection);
  TableViewTextItem* federationItem =
      GetTableViewItem(kFederatedFederationSection, kFederatedFederationItem);
  EXPECT_NSEQ(@"famous.provider.net", federationItem.text);
  EXPECT_FALSE(federationItem.masked);
  // Delete section
  EXPECT_EQ(1, NumberOfItemsInSection(kFederatedDeleteSection));
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_PASSWORD_DELETE_BUTTON,
                          kFederatedDeleteSection, kFederatedDeleteButtonItem);
}

struct SimplifyOriginTestData {
  GURL origin;
  NSString* expectedSimplifiedOrigin;
};

TEST_F(LegacyPasswordDetailsTableViewControllerTest, SimplifyOrigin) {
  SimplifyOriginTestData test_data[] = {
      {GURL("http://test.com/index.php"), @"test.com"},
      {GURL("https://example.com/index.php"), @"example.com"},
      {GURL("android://"
            "Qllt1FacrB0NYCeSFvmudHvssWBPFfC54EbtHTpFxukvw2wClI1rafcVB3kQOMxfJg"
            "xbVAkGXvC_A52kbPL1EQ==@com.parkingpanda.mobile/"),
       @"mobile.parkingpanda.com"}};

  for (const auto& data : test_data) {
    origin_ = base::SysUTF8ToNSString(data.origin.spec());
    form_.signon_realm = base::SysNSStringToUTF8(origin_);
    form_.url = GURL(form_.signon_realm);
    CreateController();
    EXPECT_NSEQ(data.expectedSimplifiedOrigin, controller().title)
        << " for origin " << data.origin;
    ResetController();
  }
}

TEST_F(LegacyPasswordDetailsTableViewControllerTest, CopySite) {
  CreateController();
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:kCopySiteButtonItem
                                                 inSection:kSiteSection]];
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(origin_, generalPasteboard.string);
}

TEST_F(LegacyPasswordDetailsTableViewControllerTest, CopyUsername) {
  CreateController();
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath
                                  indexPathForRow:kCopyUsernameButtonItem
                                        inSection:kUsernameSection]];
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(kUsername, generalPasteboard.string);
}

TEST_F(LegacyPasswordDetailsTableViewControllerTest, ShowPassword) {
  CreateController();
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:kShowHideButtonItem
                                                 inSection:kPasswordSection]];
  TableViewTextItem* passwordItem =
      GetTableViewItem(kPasswordSection, kPasswordItem);
  EXPECT_NSEQ(kPassword, passwordItem.text);
  EXPECT_FALSE(passwordItem.masked);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_SHOW),
      reauthentication_module_.localizedReasonForAuthentication);
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_PASSWORD_HIDE_BUTTON,
                          kPasswordSection, kShowHideButtonItem);
}

TEST_F(LegacyPasswordDetailsTableViewControllerTest, HidePassword) {
  CreateController();
  // First show the password.
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:kShowHideButtonItem
                                                 inSection:kPasswordSection]];
  // Then hide it.
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:kShowHideButtonItem
                                                 inSection:kPasswordSection]];
  TableViewTextItem* passwordItem =
      GetTableViewItem(kPasswordSection, kPasswordItem);
  EXPECT_NSEQ(kPassword, passwordItem.text);
  EXPECT_TRUE(passwordItem.masked);
  CheckTextCellTextWithId(IDS_IOS_SETTINGS_PASSWORD_SHOW_BUTTON,
                          kPasswordSection, kShowHideButtonItem);
}

TEST_F(LegacyPasswordDetailsTableViewControllerTest, CopyPassword) {
  CreateController();
  [controller() tableView:[controller() tableView]
      didSelectRowAtIndexPath:[NSIndexPath
                                  indexPathForRow:kCopyPasswordButtonItem
                                        inSection:kPasswordSection]];
  UIPasteboard* generalPasteboard = [UIPasteboard generalPasteboard];
  EXPECT_NSEQ(kPassword, generalPasteboard.string);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORD_REAUTH_REASON_COPY),
      reauthentication_module_.localizedReasonForAuthentication);
}

}  // namespace
