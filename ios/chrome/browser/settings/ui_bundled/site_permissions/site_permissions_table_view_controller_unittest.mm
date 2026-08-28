// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_site_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

class SitePermissionsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    return [[SitePermissionsTableViewController alloc] init];
  }

  SitePermissionsTableViewController* GetSitePermissionsTableViewController() {
    return base::apple::ObjCCastStrict<SitePermissionsTableViewController>(
        controller());
  }

  SitePermissionsSiteItem* CreateSiteItem(NSString* origin, NSString* title) {
    SitePermissionsSiteItem* item = [[SitePermissionsSiteItem alloc] init];
    item.origin = origin;
    item.formattedTitle = title;
    item.URL =
        [[CrURL alloc] initWithGURL:GURL(base::SysNSStringToUTF8(origin))];
    return item;
  }
};

// Test that the controller initializes with empty state.
TEST_F(SitePermissionsTableViewControllerTest, TestEmptyState) {
  CreateController();
  CheckController();
  CheckTitle(@"Site Permissions");

  EXPECT_EQ(NumberOfSections(), 0);
  EXPECT_NE(nil,
            GetSitePermissionsTableViewController().tableView.backgroundView);
}

// Test that populated items are loaded into the table model.
TEST_F(SitePermissionsTableViewControllerTest, TestPopulatedItems) {
  CreateController();
  CheckController();

  SitePermissionsSiteItem* item1 = CreateSiteItem(@"https://a.com", @"a.com");
  SitePermissionsSiteItem* item2 = CreateSiteItem(@"https://b.com", @"b.com");

  [GetSitePermissionsTableViewController()
      setSitePermissionsSiteItems:@[ item1, item2 ]];

  EXPECT_EQ(NumberOfSections(), 1);
  EXPECT_EQ(NumberOfItemsInSection(0), 2);

  TableViewURLItem* urlItem1 =
      base::apple::ObjCCastStrict<TableViewURLItem>(GetTableViewItem(0, 0));
  EXPECT_NSEQ(@"a.com", urlItem1.title);

  TableViewURLItem* urlItem2 =
      base::apple::ObjCCastStrict<TableViewURLItem>(GetTableViewItem(0, 1));
  EXPECT_NSEQ(@"b.com", urlItem2.title);
}

// Test that search filters the displayed site items.
TEST_F(SitePermissionsTableViewControllerTest, TestSearchFilter) {
  CreateController();
  CheckController();

  SitePermissionsSiteItem* item1 =
      CreateSiteItem(@"https://apple.com", @"apple.com");
  SitePermissionsSiteItem* item2 =
      CreateSiteItem(@"https://google.com", @"google.com");

  SitePermissionsTableViewController* tableController =
      GetSitePermissionsTableViewController();
  [tableController setSitePermissionsSiteItems:@[ item1, item2 ]];

  EXPECT_EQ(NumberOfItemsInSection(0), 2);

  UISearchController* searchController =
      tableController.navigationItem.searchController;
  searchController.searchBar.text = @"goog";
  [tableController updateSearchResultsForSearchController:searchController];

  EXPECT_EQ(NumberOfItemsInSection(0), 1);
  TableViewURLItem* urlItem =
      base::apple::ObjCCastStrict<TableViewURLItem>(GetTableViewItem(0, 0));
  EXPECT_NSEQ(@"google.com", urlItem.title);
}

// Test that selecting a row notifies the delegate.
TEST_F(SitePermissionsTableViewControllerTest, TestRowSelection) {
  CreateController();
  CheckController();

  SitePermissionsSiteItem* item1 = CreateSiteItem(@"https://a.com", @"a.com");

  SitePermissionsTableViewController* tableController =
      GetSitePermissionsTableViewController();
  [tableController setSitePermissionsSiteItems:@[ item1 ]];

  id delegateMock =
      OCMProtocolMock(@protocol(SitePermissionsTableViewControllerDelegate));
  tableController.delegate = delegateMock;

  OCMExpect([delegateMock sitePermissionsTableViewController:tableController
                                               didSelectSite:item1]);

  [tableController tableView:tableController.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];

  EXPECT_OCMOCK_VERIFY(delegateMock);
}
