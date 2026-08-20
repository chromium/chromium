// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

// Checks that key methods are called.
// TableViewItem can't easily be mocked via OCMock as one of the methods to
// mock returns a Class type.
@interface FakeTableViewItem : TableViewItem
@property(nonatomic, assign) BOOL configureCellCalled;
@end

@implementation FakeTableViewItem

@synthesize configureCellCalled = _configureCellCalled;

- (void)configureCell:(LegacyTableViewCell*)cell {
  self.configureCellCalled = YES;
  [super configureCell:cell];
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

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter {
  self.configureHeaderFooterViewCalled = YES;
  [super configureHeaderFooterView:headerFooter];
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

// Tests that reconfigureCellsForItems: calls configureCell: on visible cells.
TEST_F(LegacyChromeTableViewControllerTest, ReconfigureCellsForItems) {
  LegacyChromeTableViewController* controller =
      [[LegacyChromeTableViewController alloc]
          initWithStyle:UITableViewStylePlain];
  [controller loadModel];

  [[controller tableViewModel] addSectionWithIdentifier:SectionIdentifierFoo];
  FakeTableViewItem* item =
      [[FakeTableViewItem alloc] initWithType:ItemTypeFooBar];
  [[controller tableViewModel] addItem:item
               toSectionWithIdentifier:SectionIdentifierFoo];

  ScopedKeyWindow scoped_key_window;
  [scoped_key_window.Get() setRootViewController:controller];
  [controller.tableView layoutIfNeeded];

  UITableViewCell* cell = [controller.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  ASSERT_TRUE(cell);
  EXPECT_TRUE([item configureCellCalled]);

  [item setConfigureCellCalled:NO];
  [controller reconfigureCellsForItems:@[ item ]];
  EXPECT_TRUE([item configureCellCalled]);
}

// Tests that reconfigureCellsForItems: updates accessibilityValue for switch
// items.
TEST_F(LegacyChromeTableViewControllerTest,
       ReconfigureSwitchItemAccessibilityValue) {
  LegacyChromeTableViewController* controller =
      [[LegacyChromeTableViewController alloc]
          initWithStyle:UITableViewStylePlain];
  [controller loadModel];

  [[controller tableViewModel] addSectionWithIdentifier:SectionIdentifierFoo];
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeFooBar];
  switchItem.text = @"Switch Setting";
  switchItem.on = NO;
  [[controller tableViewModel] addItem:switchItem
               toSectionWithIdentifier:SectionIdentifierFoo];

  ScopedKeyWindow scoped_key_window;
  [scoped_key_window.Get() setRootViewController:controller];
  [controller.tableView layoutIfNeeded];

  UITableViewCell* cell = [controller.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  ASSERT_TRUE(cell);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              cell.accessibilityValue);

  switchItem.on = YES;
  [controller reconfigureCellsForItems:@[ switchItem ]];
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              cell.accessibilityValue);
}

// Tests that toggling the underlying UISwitch view directly updates the cell's
// accessibilityValue dynamically.
TEST_F(LegacyChromeTableViewControllerTest,
       SwitchItemViewToggleAccessibilityValue) {
  LegacyChromeTableViewController* controller =
      [[LegacyChromeTableViewController alloc]
          initWithStyle:UITableViewStylePlain];
  [controller loadModel];

  [[controller tableViewModel] addSectionWithIdentifier:SectionIdentifierFoo];
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeFooBar];
  switchItem.text = @"Switch Setting";
  switchItem.on = YES;
  [[controller tableViewModel] addItem:switchItem
               toSectionWithIdentifier:SectionIdentifierFoo];

  ScopedKeyWindow scoped_key_window;
  [scoped_key_window.Get() setRootViewController:controller];
  [controller.tableView layoutIfNeeded];

  UITableViewCell* cell = [controller.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  ASSERT_TRUE(cell);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              cell.accessibilityValue);

  auto find_switch = [](UIView* view, auto& self_ref) -> UISwitch* {
    if ([view isKindOfClass:[UISwitch class]]) {
      return static_cast<UISwitch*>(view);
    }
    for (UIView* subview in view.subviews) {
      UISwitch* result = self_ref(subview, self_ref);
      if (result) {
        return result;
      }
    }
    return nil;
  };

  UISwitch* switchView = find_switch(cell, find_switch);
  ASSERT_TRUE(switchView);

  // Directly toggle switch view (simulating user or VoiceOver interaction).
  switchView.on = NO;
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              cell.accessibilityValue);

  switchView.on = YES;
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_ON),
              cell.accessibilityValue);
}

}  // namespace
