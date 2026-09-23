// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/common/content_settings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_mutator.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_site_exception.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {

SiteSettingsSiteException* CreateSiteException(NSString* origin,
                                               NSString* title) {
  SiteSettingsSiteException* exception =
      [[SiteSettingsSiteException alloc] init];
  exception.origin = origin;
  exception.formattedTitle = title;
  GURL url(base::SysNSStringToUTF8(origin));
  if (url.is_valid()) {
    exception.URL = [[CrURL alloc] initWithGURL:url];
  }
  return exception;
}

}  // namespace

class SiteSettingsCategoryDetailViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    mutator_ = OCMProtocolMock(@protocol(SiteSettingsCategoryDetailMutator));
    delegate_ = OCMProtocolMock(
        @protocol(SiteSettingsCategoryDetailViewControllerDelegate));
  }

  LegacyChromeTableViewController* InstantiateController() override {
    SiteSettingsCategoryDetailViewController* controller =
        [[SiteSettingsCategoryDetailViewController alloc]
            initWithCategory:SiteSettingsCategory::kMicrophone];
    controller.mutator = mutator_;
    controller.delegate = delegate_;
    return controller;
  }

  SiteSettingsCategoryDetailViewController* GetController() {
    return static_cast<SiteSettingsCategoryDetailViewController*>(controller());
  }

  id mutator_;
  id delegate_;
};

// Tests that the view controller initializes with the default settings section
// and hides exception sections when empty.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestModelInitializationHidesEmptySections) {
  CreateController();
  CheckController();

  EXPECT_NSEQ(@"Microphone", GetController().title);
  // Only 1 section initially: Default Setting.
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));

  TableViewDetailIconItem* askItem =
      static_cast<TableViewDetailIconItem*>(GetTableViewItem(0, 0));
  EXPECT_NSEQ(@"Sites can ask for your microphone", askItem.text);
  EXPECT_EQ(UITableViewCellAccessoryNone, askItem.accessoryType);

  TableViewDetailIconItem* blockItem =
      static_cast<TableViewDetailIconItem*>(GetTableViewItem(0, 1));
  EXPECT_NSEQ(@"Don't allow sites to use your microphone", blockItem.text);
  EXPECT_EQ(UITableViewCellAccessoryNone, blockItem.accessoryType);
}

// Tests updating the default setting toggles checkmarks.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestUpdateDefaultSettingCheckmark) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* vc = GetController();
  [vc setDefaultSetting:CONTENT_SETTING_ASK];

  TableViewDetailIconItem* askItem =
      static_cast<TableViewDetailIconItem*>(GetTableViewItem(0, 0));
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, askItem.accessoryType);

  TableViewDetailIconItem* blockItem =
      static_cast<TableViewDetailIconItem*>(GetTableViewItem(0, 1));
  EXPECT_EQ(UITableViewCellAccessoryNone, blockItem.accessoryType);

  [vc setDefaultSetting:CONTENT_SETTING_BLOCK];
  EXPECT_EQ(UITableViewCellAccessoryNone, askItem.accessoryType);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, blockItem.accessoryType);
}

// Tests that Allowed and Not Allowed sections appear when they contain entries.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestAllowedAndNotAllowedSections) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* vc = GetController();

  SiteSettingsSiteException* blockedSite =
      CreateSiteException(@"https://blocked.com", @"blocked.com");
  SiteSettingsSiteException* allowedSite =
      CreateSiteException(@"https://allowed.com", @"allowed.com");

  [vc setAllowedSites:@[ allowedSite ] notAllowedSites:@[ blockedSite ]];

  // Sections: Default Setting, Not Allowed, Allowed.
  EXPECT_EQ(3, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  EXPECT_EQ(1, NumberOfItemsInSection(2));

  TableViewURLItem* notAllowedItem =
      static_cast<TableViewURLItem*>(GetTableViewItem(1, 0));
  EXPECT_EQ(GURL("https://blocked.com"), notAllowedItem.URL.gurl);
  EXPECT_EQ(nil, notAllowedItem.title);

  TableViewURLItem* allowedItem =
      static_cast<TableViewURLItem*>(GetTableViewItem(2, 0));
  EXPECT_EQ(GURL("https://allowed.com"), allowedItem.URL.gurl);
  EXPECT_EQ(nil, allowedItem.title);

  // Clearing lists should remove the sections again.
  [vc setAllowedSites:@[] notAllowedSites:@[]];
  EXPECT_EQ(1, NumberOfSections());
}

// Tests that selecting Ask or Block calls the mutator, while exception rows
// cannot be selected.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestTappingDefaultOptionsNotifiesMutator) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* vc = GetController();

  SiteSettingsSiteException* allowedSite =
      CreateSiteException(@"https://allowed.com", @"allowed.com");
  [vc setAllowedSites:@[ allowedSite ] notAllowedSites:@[]];

  NSIndexPath* askIndexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  EXPECT_NSEQ(askIndexPath, [vc.tableView.delegate tableView:vc.tableView
                                    willSelectRowAtIndexPath:askIndexPath]);

  NSIndexPath* exceptionIndexPath = [NSIndexPath indexPathForRow:0 inSection:1];
  EXPECT_EQ(nil, [vc.tableView.delegate tableView:vc.tableView
                         willSelectRowAtIndexPath:exceptionIndexPath]);

  OCMExpect([mutator_ setDefaultSetting:CONTENT_SETTING_ASK]);
  [vc.tableView.delegate tableView:vc.tableView
           didSelectRowAtIndexPath:askIndexPath];
  EXPECT_OCMOCK_VERIFY(mutator_);

  OCMExpect([mutator_ setDefaultSetting:CONTENT_SETTING_BLOCK]);
  [vc.tableView.delegate tableView:vc.tableView
           didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]];
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Tests that trailing swipe actions are nil for non-site items and valid for
// site items, even when some sections are not present.
TEST_F(SiteSettingsCategoryDetailViewControllerTest, TestTrailingSwipeActions) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* vc = GetController();

  // With no exceptions, querying swipe actions for a default setting row should
  // return nil without crashing.
  UISwipeActionsConfiguration* defaultConfig =
      [vc.tableView.delegate tableView:vc.tableView
          trailingSwipeActionsConfigurationForRowAtIndexPath:
              [NSIndexPath indexPathForRow:0 inSection:0]];
  EXPECT_EQ(nil, defaultConfig);

  // Add an allowed site exception.
  SiteSettingsSiteException* allowedSite =
      CreateSiteException(@"https://allowed.com", @"allowed.com");
  [vc setAllowedSites:@[ allowedSite ] notAllowedSites:@[]];

  // The Allowed section is at section index 1 (since Not Allowed is omitted).
  UISwipeActionsConfiguration* allowedConfig =
      [vc.tableView.delegate tableView:vc.tableView
          trailingSwipeActionsConfigurationForRowAtIndexPath:
              [NSIndexPath indexPathForRow:0 inSection:1]];
  ASSERT_NE(nil, allowedConfig);
  EXPECT_EQ(1u, allowedConfig.actions.count);
}

// Tests that site exception rows configure a popup menu accessory button that
// switches between Allow and Not Allowed via the mutator.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestSiteExceptionPopupMenu) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* vc = GetController();

  SiteSettingsSiteException* blockedSite =
      CreateSiteException(@"https://blocked.com", @"blocked.com");
  SiteSettingsSiteException* allowedSite =
      CreateSiteException(@"https://allowed.com", @"allowed.com");

  [vc setAllowedSites:@[ allowedSite ] notAllowedSites:@[ blockedSite ]];

  // Section 1: Not Allowed.
  UITableViewCell* notAllowedCell =
      [vc tableView:vc.tableView
          cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:1]];
  ASSERT_TRUE([notAllowedCell.accessoryView isKindOfClass:[UIButton class]]);
  UIButton* notAllowedButton =
      static_cast<UIButton*>(notAllowedCell.accessoryView);
  EXPECT_TRUE(notAllowedButton.showsMenuAsPrimaryAction);
  EXPECT_TRUE(notAllowedCell.isAccessibilityElement);
  EXPECT_NSEQ(@"Not Allowed", notAllowedCell.accessibilityValue);
  ASSERT_NE(nil, notAllowedButton.menu);
  ASSERT_EQ(2u, notAllowedButton.menu.children.count);

  UIAction* notAllowedMenuAllowAction =
      static_cast<UIAction*>(notAllowedButton.menu.children[0]);
  UIAction* notAllowedMenuBlockAction =
      static_cast<UIAction*>(notAllowedButton.menu.children[1]);
  EXPECT_NSEQ(@"Allowed", notAllowedMenuAllowAction.title);
  EXPECT_EQ(UIMenuElementStateOff, notAllowedMenuAllowAction.state);
  EXPECT_NSEQ(@"Not Allowed", notAllowedMenuBlockAction.title);
  EXPECT_EQ(UIMenuElementStateOn, notAllowedMenuBlockAction.state);

  // Section 2: Allowed.
  UITableViewCell* allowedCell = [vc tableView:vc.tableView
                         cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0
                                                                  inSection:2]];
  ASSERT_TRUE([allowedCell.accessoryView isKindOfClass:[UIButton class]]);
  UIButton* allowedButton = static_cast<UIButton*>(allowedCell.accessoryView);
  EXPECT_TRUE(allowedButton.showsMenuAsPrimaryAction);
  EXPECT_TRUE(allowedCell.isAccessibilityElement);
  EXPECT_NSEQ(@"Allowed", allowedCell.accessibilityValue);
  ASSERT_NE(nil, allowedButton.menu);
  ASSERT_EQ(2u, allowedButton.menu.children.count);

  UIAction* allowedMenuAllowAction =
      static_cast<UIAction*>(allowedButton.menu.children[0]);
  UIAction* allowedMenuBlockAction =
      static_cast<UIAction*>(allowedButton.menu.children[1]);
  EXPECT_NSEQ(@"Allowed", allowedMenuAllowAction.title);
  EXPECT_EQ(UIMenuElementStateOn, allowedMenuAllowAction.state);
  EXPECT_NSEQ(@"Not Allowed", allowedMenuBlockAction.title);
  EXPECT_EQ(UIMenuElementStateOff, allowedMenuBlockAction.state);

  // Verify selecting "Allow" on the blocked site calls the mutator.
  OCMExpect([mutator_ setSetting:CONTENT_SETTING_ALLOW forSite:blockedSite]);
  [notAllowedMenuAllowAction performWithSender:notAllowedButton target:nil];
  EXPECT_OCMOCK_VERIFY(mutator_);

  // Verify selecting "Not Allowed" on the allowed site calls the mutator.
  OCMExpect([mutator_ setSetting:CONTENT_SETTING_BLOCK forSite:allowedSite]);
  [allowedMenuBlockAction performWithSender:allowedButton target:nil];
  EXPECT_OCMOCK_VERIFY(mutator_);
}
