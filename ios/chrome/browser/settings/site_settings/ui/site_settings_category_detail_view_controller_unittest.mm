// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/content_settings/core/common/content_settings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_mutator.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_site_exception.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_disclosure_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"
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
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that the view controller initializes with the default settings section
// and hides exception sections when empty.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestModelInitializationHidesEmptySections) {
  CreateController();
  CheckController();

  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PERMISSIONS_MICROPHONE),
              GetController().title);
  // Only 1 section initially: Default Setting.
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));

  TableViewDetailIconItem* askItem =
      static_cast<TableViewDetailIconItem*>(GetTableViewItem(0, 0));
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_MICROPHONE_ASK_TITLE),
      askItem.text);
  EXPECT_EQ(UITableViewCellAccessoryNone, askItem.accessoryType);

  TableViewDetailIconItem* blockItem =
      static_cast<TableViewDetailIconItem*>(GetTableViewItem(0, 1));
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_MICROPHONE_BLOCK_TITLE),
      blockItem.text);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_MICROPHONE_BLOCK_SUBTITLE),
      blockItem.detailText);
  EXPECT_EQ(UITableViewCellAccessoryNone, blockItem.accessoryType);
}

// Tests updating the default setting toggles checkmarks.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestUpdateDefaultSettingCheckmark) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* view_controller = GetController();
  [view_controller setDefaultSetting:CONTENT_SETTING_ASK];

  TableViewDetailIconItem* askItem =
      static_cast<TableViewDetailIconItem*>(GetTableViewItem(0, 0));
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, askItem.accessoryType);

  TableViewDetailIconItem* blockItem =
      static_cast<TableViewDetailIconItem*>(GetTableViewItem(0, 1));
  EXPECT_EQ(UITableViewCellAccessoryNone, blockItem.accessoryType);

  [view_controller setDefaultSetting:CONTENT_SETTING_BLOCK];
  EXPECT_EQ(UITableViewCellAccessoryNone, askItem.accessoryType);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, blockItem.accessoryType);
}

// Tests that Allowed and Not Allowed sections appear when they contain entries.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestAllowedAndNotAllowedSections) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* view_controller = GetController();

  SiteSettingsSiteException* blockedSite =
      CreateSiteException(@"https://blocked.com", @"blocked.com");
  SiteSettingsSiteException* allowedSite =
      CreateSiteException(@"https://allowed.com", @"allowed.com");

  [view_controller setAllowedSites:@[ allowedSite ]
                   notAllowedSites:@[ blockedSite ]];

  // Sections: Default Setting, Allowed, Not Allowed.
  EXPECT_EQ(3, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  EXPECT_EQ(1, NumberOfItemsInSection(2));

  TableViewDisclosureHeaderFooterItem* allowedHeader =
      static_cast<TableViewDisclosureHeaderFooterItem*>(
          [view_controller.tableViewModel headerForSectionIndex:1]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_ALLOWED),
              allowedHeader.text);

  TableViewDisclosureHeaderFooterItem* notAllowedHeader =
      static_cast<TableViewDisclosureHeaderFooterItem*>(
          [view_controller.tableViewModel headerForSectionIndex:2]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_NOT_ALLOWED),
              notAllowedHeader.text);

  TableViewURLItem* allowedItem =
      static_cast<TableViewURLItem*>(GetTableViewItem(1, 0));
  EXPECT_EQ(GURL("https://allowed.com"), allowedItem.URL.gurl);
  EXPECT_EQ(nil, allowedItem.title);

  TableViewURLItem* notAllowedItem =
      static_cast<TableViewURLItem*>(GetTableViewItem(2, 0));
  EXPECT_EQ(GURL("https://blocked.com"), notAllowedItem.URL.gurl);
  EXPECT_EQ(nil, notAllowedItem.title);

  // Clearing lists should remove the sections again.
  [view_controller setAllowedSites:@[] notAllowedSites:@[]];
  EXPECT_EQ(1, NumberOfSections());
}

// Tests that selecting Ask or Block calls the mutator, while exception rows
// cannot be selected.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestTappingDefaultOptionsNotifiesMutator) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* view_controller = GetController();

  SiteSettingsSiteException* allowedSite =
      CreateSiteException(@"https://allowed.com", @"allowed.com");
  [view_controller setAllowedSites:@[ allowedSite ] notAllowedSites:@[]];

  NSIndexPath* askIndexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  EXPECT_NSEQ(
      askIndexPath,
      [view_controller.tableView.delegate tableView:view_controller.tableView
                           willSelectRowAtIndexPath:askIndexPath]);

  NSIndexPath* exceptionIndexPath = [NSIndexPath indexPathForRow:0 inSection:1];
  EXPECT_EQ(nil, [view_controller.tableView.delegate
                                    tableView:view_controller.tableView
                     willSelectRowAtIndexPath:exceptionIndexPath]);

  OCMExpect([mutator_ setDefaultSetting:CONTENT_SETTING_ASK]);
  [view_controller.tableView.delegate tableView:view_controller.tableView
                        didSelectRowAtIndexPath:askIndexPath];
  EXPECT_OCMOCK_VERIFY(mutator_);

  OCMExpect([mutator_ setDefaultSetting:CONTENT_SETTING_BLOCK]);
  [view_controller.tableView.delegate
                    tableView:view_controller.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:1 inSection:0]];
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Tests that trailing swipe actions are nil for non-site items and valid for
// site items, even when some sections are not present.
TEST_F(SiteSettingsCategoryDetailViewControllerTest, TestTrailingSwipeActions) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* view_controller = GetController();

  // With no exceptions, querying swipe actions for a default setting row should
  // return nil without crashing.
  UISwipeActionsConfiguration* defaultConfig =
      [view_controller.tableView.delegate tableView:view_controller.tableView
          trailingSwipeActionsConfigurationForRowAtIndexPath:
              [NSIndexPath indexPathForRow:0 inSection:0]];
  EXPECT_EQ(nil, defaultConfig);

  // Add an allowed site exception.
  SiteSettingsSiteException* allowedSite =
      CreateSiteException(@"https://allowed.com", @"allowed.com");
  [view_controller setAllowedSites:@[ allowedSite ] notAllowedSites:@[]];

  // The Allowed section is at section index 1.
  UISwipeActionsConfiguration* allowedConfig =
      [view_controller.tableView.delegate tableView:view_controller.tableView
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

  SiteSettingsCategoryDetailViewController* view_controller = GetController();

  SiteSettingsSiteException* blockedSite =
      CreateSiteException(@"https://blocked.com", @"blocked.com");
  SiteSettingsSiteException* allowedSite =
      CreateSiteException(@"https://allowed.com", @"allowed.com");

  [view_controller setAllowedSites:@[ allowedSite ]
                   notAllowedSites:@[ blockedSite ]];

  // Section 1: Allowed.
  UITableViewCell* allowedCell =
      [view_controller tableView:view_controller.tableView
           cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:1]];
  ASSERT_TRUE([allowedCell.accessoryView isKindOfClass:[UIButton class]]);
  UIButton* allowedButton = static_cast<UIButton*>(allowedCell.accessoryView);
  EXPECT_TRUE(allowedButton.showsMenuAsPrimaryAction);
  EXPECT_TRUE(allowedCell.isAccessibilityElement);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_ALLOWED),
              allowedCell.accessibilityValue);
  ASSERT_NE(nil, allowedButton.menu);
  ASSERT_EQ(2u, allowedButton.menu.children.count);

  UIAction* allowedMenuAllowAction =
      static_cast<UIAction*>(allowedButton.menu.children[0]);
  UIAction* allowedMenuBlockAction =
      static_cast<UIAction*>(allowedButton.menu.children[1]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_ALLOWED),
              allowedMenuAllowAction.title);
  EXPECT_EQ(UIMenuElementStateOn, allowedMenuAllowAction.state);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_NOT_ALLOWED),
              allowedMenuBlockAction.title);
  EXPECT_EQ(UIMenuElementStateOff, allowedMenuBlockAction.state);

  // Section 2: Not Allowed.
  UITableViewCell* notAllowedCell =
      [view_controller tableView:view_controller.tableView
           cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:2]];
  ASSERT_TRUE([notAllowedCell.accessoryView isKindOfClass:[UIButton class]]);
  UIButton* notAllowedButton =
      static_cast<UIButton*>(notAllowedCell.accessoryView);
  EXPECT_TRUE(notAllowedButton.showsMenuAsPrimaryAction);
  EXPECT_TRUE(notAllowedCell.isAccessibilityElement);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_NOT_ALLOWED),
              notAllowedCell.accessibilityValue);
  ASSERT_NE(nil, notAllowedButton.menu);
  ASSERT_EQ(2u, notAllowedButton.menu.children.count);

  UIAction* notAllowedMenuAllowAction =
      static_cast<UIAction*>(notAllowedButton.menu.children[0]);
  UIAction* notAllowedMenuBlockAction =
      static_cast<UIAction*>(notAllowedButton.menu.children[1]);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_ALLOWED),
              notAllowedMenuAllowAction.title);
  EXPECT_EQ(UIMenuElementStateOff, notAllowedMenuAllowAction.state);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SITE_SETTINGS_NOT_ALLOWED),
              notAllowedMenuBlockAction.title);
  EXPECT_EQ(UIMenuElementStateOn, notAllowedMenuBlockAction.state);

  // Verify selecting "Allowed" on the blocked site calls the mutator.
  OCMExpect([mutator_ setSetting:CONTENT_SETTING_ALLOW forSite:blockedSite]);
  [notAllowedMenuAllowAction performWithSender:notAllowedButton target:nil];
  EXPECT_OCMOCK_VERIFY(mutator_);

  // Verify selecting "Not Allowed" on the allowed site calls the mutator.
  OCMExpect([mutator_ setSetting:CONTENT_SETTING_BLOCK forSite:allowedSite]);
  [allowedMenuBlockAction performWithSender:allowedButton target:nil];
  EXPECT_OCMOCK_VERIFY(mutator_);
}

// Tests entering edit mode, selecting multiple site exceptions, and bulk
// deleting them via the bottom toolbar Delete button.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestBulkDeleteSiteExceptions) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* view_controller = GetController();
  EXPECT_FALSE([view_controller shouldHideToolbar]);
  EXPECT_FALSE([view_controller shouldShowEditDoneButton]);
  EXPECT_FALSE([view_controller editButtonEnabled]);

  SiteSettingsSiteException* blockedSite =
      CreateSiteException(@"https://blocked.com", @"blocked.com");
  SiteSettingsSiteException* allowedSite =
      CreateSiteException(@"https://allowed.com", @"allowed.com");
  [view_controller setAllowedSites:@[ allowedSite ]
                   notAllowedSites:@[ blockedSite ]];

  EXPECT_TRUE([view_controller editButtonEnabled]);
  EXPECT_TRUE(view_controller.toolbarItems.lastObject.enabled);

  // Enter editing mode.
  [view_controller editButtonPressed];

  // Default setting rows (Section 0) are not editable or selectable in edit
  // mode.
  NSIndexPath* defaultRow = [NSIndexPath indexPathForRow:0 inSection:0];
  EXPECT_FALSE([view_controller tableView:view_controller.tableView
                    canEditRowAtIndexPath:defaultRow]);
  EXPECT_EQ(nil, [view_controller tableView:view_controller.tableView
                     willSelectRowAtIndexPath:defaultRow]);

  // Site exception rows (Section 1: Allowed and Section 2: Not Allowed) are
  // editable and selectable in edit mode.
  NSIndexPath* allowedRow = [NSIndexPath indexPathForRow:0 inSection:1];
  NSIndexPath* blockedRow = [NSIndexPath indexPathForRow:0 inSection:2];
  EXPECT_TRUE([view_controller tableView:view_controller.tableView
                   canEditRowAtIndexPath:allowedRow]);
  EXPECT_TRUE([view_controller tableView:view_controller.tableView
                   canEditRowAtIndexPath:blockedRow]);
  EXPECT_NSEQ(allowedRow, [view_controller tableView:view_controller.tableView
                              willSelectRowAtIndexPath:allowedRow]);
  EXPECT_NSEQ(blockedRow, [view_controller tableView:view_controller.tableView
                              willSelectRowAtIndexPath:blockedRow]);

  // Select both site rows.
  [view_controller.tableView
      selectRowAtIndexPath:allowedRow
                  animated:NO
            scrollPosition:UITableViewScrollPositionNone];
  [view_controller tableView:view_controller.tableView
      didSelectRowAtIndexPath:allowedRow];
  [view_controller.tableView
      selectRowAtIndexPath:blockedRow
                  animated:NO
            scrollPosition:UITableViewScrollPositionNone];
  [view_controller tableView:view_controller.tableView
      didSelectRowAtIndexPath:blockedRow];

  // Trigger Delete button action from the toolbar.
  UIBarButtonItem* deleteButton = view_controller.toolbarItems.firstObject;
  ASSERT_NE(nil, deleteButton);
  EXPECT_TRUE(deleteButton.enabled);

  NSArray<SiteSettingsSiteException*>* expectedSites =
      @[ allowedSite, blockedSite ];
  OCMExpect([mutator_ deleteSettingsForSites:expectedSites]);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [deleteButton.target performSelector:deleteButton.action
                            withObject:deleteButton];
#pragma clang diagnostic pop
  EXPECT_OCMOCK_VERIFY(mutator_);
  EXPECT_FALSE(view_controller.editing);
}

// Tests that the search controller is hidden when no site exceptions exist,
// shown when exceptions exist, and displays a scrim when focused with an empty
// query.
TEST_F(SiteSettingsCategoryDetailViewControllerTest,
       TestSearchVisibilityAndScrim) {
  CreateController();
  CheckController();

  SiteSettingsCategoryDetailViewController* view_controller = GetController();

  // With no site exceptions, the search bar should be hidden.
  EXPECT_EQ(nil, view_controller.navigationItem.searchController);

  // Adding a site exception reveals the search bar.
  SiteSettingsSiteException* allowedSite =
      CreateSiteException(@"https://allowed.com", @"allowed.com");
  [view_controller setAllowedSites:@[ allowedSite ] notAllowedSites:@[]];
  UISearchController* searchController =
      view_controller.navigationItem.searchController;
  ASSERT_NE(nil, searchController);

  // Presenting the search controller shows the scrim and hides the toolbar.
  [searchController.delegate willPresentSearchController:searchController];
  EXPECT_TRUE([view_controller shouldHideToolbar]);
  EXPECT_FALSE(view_controller.tableView.scrollEnabled);
  EXPECT_TRUE(view_controller.tableView.accessibilityElementsHidden);

  // Typing a search query hides the scrim and restores the toolbar.
  searchController.searchBar.text = @"allowed";
  [searchController.searchResultsUpdater
      updateSearchResultsForSearchController:searchController];
  EXPECT_FALSE([view_controller shouldHideToolbar]);
  EXPECT_TRUE([view_controller editButtonEnabled]);

  // Entering edit mode disables interaction on the search bar.
  [view_controller setEditing:YES animated:NO];
  EXPECT_FALSE(searchController.searchBar.userInteractionEnabled);
  [view_controller setEditing:NO animated:NO];
  EXPECT_TRUE(searchController.searchBar.userInteractionEnabled);

  // Removing all site exceptions hides the search bar again after the delay.
  searchController.searchBar.text = @"";
  [searchController.delegate didDismissSearchController:searchController];
  [view_controller setAllowedSites:@[] notAllowedSites:@[]];
  task_environment_.FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(nil, view_controller.navigationItem.searchController);
}
