// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "testing/platform_test.h"

// Checks that key methods are called.
// TableViewItem can't easily be mocked via OCMock as one of the methods to
// mock returns a Class type.
@interface FakeTableViewItem : TableViewItem
@property(nonatomic, assign) BOOL configureCellCalled;
@end

@implementation FakeTableViewItem

@synthesize configureCellCalled = _configureCellCalled;

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  self.configureCellCalled = YES;
  [super configureCell:cell withStyler:styler];
}

@end

// Checks that key methods are called.
// TableViewHeaderFooterItem can't easily be mocked via OCMock as one of the
// methods to mock returns a Class type.
@interface FakeTableViewHeaderFooterItem : TableViewHeaderFooterItem
@property(nonatomic, assign) BOOL configureHeaderFooterViewCalled;
@end

@implementation FakeTableViewHeaderFooterItem

@synthesize configureHeaderFooterViewCalled = _configureHeaderFooterViewCalled;

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  self.configureHeaderFooterViewCalled = YES;
  [super configureHeaderFooterView:headerFooter withStyler:styler];
}

@end

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFoo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFooBar = kItemTypeEnumZero,
};

using LegacyChromeTableViewControllerTest = PlatformTest;

TEST_F(LegacyChromeTableViewControllerTest, CellForItemAtIndexPath) {
  LegacyChromeTableViewController* controller =
      [[LegacyChromeTableViewController alloc]
          initWithStyle:UITableViewStylePlain];
  [controller loadModel];

  [[controller tableViewModel] addSectionWithIdentifier:SectionIdentifierFoo];
  FakeTableViewItem* someItem =
      [[FakeTableViewItem alloc] initWithType:ItemTypeFooBar];
  [[controller tableViewModel] addItem:someItem
               toSectionWithIdentifier:SectionIdentifierFoo];

  ASSERT_EQ(NO, [someItem configureCellCalled]);
  [controller tableView:[controller tableView]
      cellForRowAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]];
  EXPECT_EQ(YES, [someItem configureCellCalled]);
}

TEST_F(LegacyChromeTableViewControllerTest, HeaderForItemAtSection) {
  LegacyChromeTableViewController* controller =
      [[LegacyChromeTableViewController alloc]
          initWithStyle:UITableViewStylePlain];
  [controller loadModel];

  [[controller tableViewModel] addSectionWithIdentifier:SectionIdentifierFoo];
  FakeTableViewHeaderFooterItem* headerItem =
      [[FakeTableViewHeaderFooterItem alloc] initWithType:ItemTypeFooBar];
  [[controller tableViewModel] setHeader:headerItem
                forSectionWithIdentifier:SectionIdentifierFoo];

  ASSERT_EQ(NO, [headerItem configureHeaderFooterViewCalled]);
  [controller tableView:[controller tableView] viewForHeaderInSection:0];
  EXPECT_EQ(YES, [headerItem configureHeaderFooterViewCalled]);
}

TEST_F(LegacyChromeTableViewControllerTest, FooterForItemAtSection) {
  LegacyChromeTableViewController* controller =
      [[LegacyChromeTableViewController alloc]
          initWithStyle:UITableViewStylePlain];
  [controller loadModel];

  [[controller tableViewModel] addSectionWithIdentifier:SectionIdentifierFoo];
  FakeTableViewHeaderFooterItem* footerItem =
      [[FakeTableViewHeaderFooterItem alloc] initWithType:ItemTypeFooBar];
  [[controller tableViewModel] setFooter:footerItem
                forSectionWithIdentifier:SectionIdentifierFoo];

  ASSERT_EQ(NO, [footerItem configureHeaderFooterViewCalled]);
  [controller tableView:[controller tableView] viewForFooterInSection:0];
  EXPECT_EQ(YES, [footerItem configureHeaderFooterViewCalled]);
}

}  // namespace
