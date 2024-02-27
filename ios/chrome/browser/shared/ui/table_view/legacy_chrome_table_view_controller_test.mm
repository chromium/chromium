// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

// Add selectors to be tested in the helpers.
@interface TableViewItem (ItemAddition)
- (NSString*)text;
- (NSString*)detailText;
- (NSString*)displayedURL;
@end

LegacyChromeTableViewControllerTest::LegacyChromeTableViewControllerTest() {}

LegacyChromeTableViewControllerTest::~LegacyChromeTableViewControllerTest() {}

void LegacyChromeTableViewControllerTest::TearDown() {
  // Delete the controller before deleting other test variables, such as a
  // profile, to ensure things are cleaned up in the same order as in Chrome.
  controller_ = nil;
  BlockCleanupTest::TearDown();
}

void LegacyChromeTableViewControllerTest::CreateController() {
  DCHECK(!controller_);
  controller_ = InstantiateController();

  // Force the tableView to be built.
  EXPECT_TRUE([controller_ view]);
}

LegacyChromeTableViewController*
LegacyChromeTableViewControllerTest::controller() {
  if (!controller_) {
    CreateController();
  }
  return controller_;
}

void LegacyChromeTableViewControllerTest::ResetController() {
  controller_ = nil;
}

void LegacyChromeTableViewControllerTest::CheckController() {
  EXPECT_TRUE([controller_ view]);
  EXPECT_TRUE([controller_ tableView]);
  EXPECT_TRUE([controller_ tableViewModel]);
  EXPECT_EQ(controller_, [controller_ tableView].delegate);
}

int LegacyChromeTableViewControllerTest::NumberOfSections() {
  return [[controller_ tableViewModel] numberOfSections];
}

int LegacyChromeTableViewControllerTest::NumberOfItemsInSection(int section) {
  return [[controller_ tableViewModel] numberOfItemsInSection:section];
}

bool LegacyChromeTableViewControllerTest::HasTableViewItem(int section,
                                                           int item) {
  TableViewModel* model = [controller_ tableViewModel];
  NSIndexPath* index_path = [NSIndexPath indexPathForItem:item
                                                inSection:section];
  return [model hasItemAtIndexPath:index_path];
}

id LegacyChromeTableViewControllerTest::GetTableViewItem(int section,
                                                         int item) {
  TableViewModel* model = [controller_ tableViewModel];
  NSIndexPath* index_path = [NSIndexPath indexPathForItem:item
                                                inSection:section];
  TableViewItem* collection_view_item = [model hasItemAtIndexPath:index_path]
                                            ? [model itemAtIndexPath:index_path]
                                            : nil;
  EXPECT_TRUE(collection_view_item);
  return collection_view_item;
}

void LegacyChromeTableViewControllerTest::CheckTitle(NSString* expected_title) {
  EXPECT_NSEQ(expected_title, [controller_ title]);
}

void LegacyChromeTableViewControllerTest::CheckTitleWithId(
    int expected_title_id) {
  CheckTitle(l10n_util::GetNSString(expected_title_id));
}

void LegacyChromeTableViewControllerTest::CheckSectionHeader(
    NSString* expected_text,
    int section) {
  TableViewHeaderFooterItem* header =
      [[controller_ tableViewModel] headerForSectionIndex:section];
  ASSERT_TRUE([header respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_text, [(id)header text]);
}

void LegacyChromeTableViewControllerTest::CheckSectionHeaderWithId(
    int expected_text_id,
    int section) {
  CheckSectionHeader(l10n_util::GetNSString(expected_text_id), section);
}

void LegacyChromeTableViewControllerTest::CheckSectionFooter(
    NSString* expected_text,
    int section) {
  TableViewHeaderFooterItem* footer =
      [[controller_ tableViewModel] footerForSectionIndex:section];
  ASSERT_TRUE([footer respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_text, [(id)footer text]);
}

void LegacyChromeTableViewControllerTest::CheckSectionFooterWithId(
    int expected_text_id,
    int section) {
  CheckSectionFooter(l10n_util::GetNSString(expected_text_id), section);
}

void LegacyChromeTableViewControllerTest::CheckTextCellEnabled(
    BOOL expected_enabled,
    int section,
    int item) {
  id cell = GetTableViewItem(section, item);
  ASSERT_TRUE([cell respondsToSelector:@selector(isEnabled)]);
  EXPECT_EQ(expected_enabled, [cell isEnabled]);
}

void LegacyChromeTableViewControllerTest::CheckTextCellText(
    NSString* expected_text,
    int section,
    int item) {
  id cell = GetTableViewItem(section, item);
  ASSERT_TRUE([cell respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_text, [cell text]);
}

void LegacyChromeTableViewControllerTest::CheckTextCellTextWithId(
    int expected_text_id,
    int section,
    int item) {
  CheckTextCellText(l10n_util::GetNSString(expected_text_id), section, item);
}

void LegacyChromeTableViewControllerTest::CheckTextCellTextAndDetailText(
    NSString* expected_text,
    NSString* expected_detail_text,
    int section,
    int item) {
  id cell = GetTableViewItem(section, item);
  ASSERT_TRUE([cell respondsToSelector:@selector(text)]);
  ASSERT_TRUE([cell respondsToSelector:@selector(detailText)]);
  EXPECT_NSEQ(expected_text, [cell text]);
  EXPECT_NSEQ(expected_detail_text, [cell detailText]);
}

void LegacyChromeTableViewControllerTest::CheckURLCellTitleAndDetailText(
    NSString* expected_title,
    NSString* expected_detail_text,
    int section,
    int item) {
  id cell = GetTableViewItem(section, item);
  ASSERT_TRUE([cell respondsToSelector:@selector(title)]);
  ASSERT_TRUE([cell respondsToSelector:@selector(detailText)]);
  EXPECT_NSEQ(expected_title, [cell title]);
  EXPECT_NSEQ(expected_detail_text, [cell detailText]);
}

void LegacyChromeTableViewControllerTest::CheckURLCellTitle(
    NSString* expected_title,
    int section,
    int item) {
  id cell = GetTableViewItem(section, item);
  ASSERT_TRUE([cell respondsToSelector:@selector(title)]);
  EXPECT_NSEQ(expected_title, [cell title]);
}

void LegacyChromeTableViewControllerTest::CheckDetailItemTextWithIds(
    int expected_text_id,
    int expected_detail_text_id,
    int section_id,
    int item_id) {
  id item = GetTableViewItem(section_id, item_id);
  ASSERT_TRUE([item respondsToSelector:@selector(text)]);
  ASSERT_TRUE([item respondsToSelector:@selector(detailText)]);
  EXPECT_NSEQ(l10n_util::GetNSString(expected_text_id), [item text]);
  EXPECT_NSEQ(l10n_util::GetNSString(expected_detail_text_id),
              [item detailText]);
}

void LegacyChromeTableViewControllerTest::CheckSwitchCellStateAndText(
    BOOL expected_state,
    NSString* expected_title,
    int section,
    int item) {
  id switch_item = GetTableViewItem(section, item);
  EXPECT_TRUE([switch_item respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_title, [switch_item text]);
  ASSERT_TRUE([switch_item respondsToSelector:@selector(isOn)]);
  EXPECT_EQ(expected_state, [switch_item isOn]);
}

void LegacyChromeTableViewControllerTest::CheckSwitchCellStateAndTextWithId(
    BOOL expected_state,
    int expected_title_id,
    int section,
    int item) {
  CheckSwitchCellStateAndText(
      expected_state, l10n_util::GetNSString(expected_title_id), section, item);
}

void LegacyChromeTableViewControllerTest::CheckInfoButtonCellStatusAndText(
    NSString* expected_status_text,
    NSString* expected_title,
    int section,
    int item) {
  id info_button_item = base::apple::ObjCCastStrict<TableViewInfoButtonItem>(
      GetTableViewItem(section, item));
  EXPECT_TRUE([info_button_item respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_title, [info_button_item text]);
  EXPECT_TRUE([info_button_item respondsToSelector:@selector(statusText)]);
  EXPECT_NSEQ(expected_status_text, [info_button_item statusText]);
}

void LegacyChromeTableViewControllerTest::
    CheckInfoButtonCellStatusWithIdAndTextWithId(int expected_status_text_id,
                                                 int expected_title_id,
                                                 int section,
                                                 int item) {
  CheckInfoButtonCellStatusAndText(
      l10n_util::GetNSString(expected_status_text_id),
      l10n_util::GetNSString(expected_title_id), section, item);
}

void LegacyChromeTableViewControllerTest::CheckAccessoryType(
    UITableViewCellAccessoryType accessory_type,
    int section,
    int item) {
  id text_item = GetTableViewItem(section, item);
  EXPECT_TRUE([text_item respondsToSelector:@selector(accessoryType)]);
  EXPECT_EQ(accessory_type, [text_item accessoryType]);
}

void LegacyChromeTableViewControllerTest::CheckTextButtonCellButtonText(
    NSString* expected_button_text,
    int section,
    int item) {
  id text_button_item = GetTableViewItem(section, item);
  ASSERT_TRUE([text_button_item respondsToSelector:@selector(buttonText)]);
  EXPECT_NSEQ(expected_button_text, [text_button_item buttonText]);
}

void LegacyChromeTableViewControllerTest::CheckTextButtonCellButtonTextWithId(
    int expected_button_text_id,
    int section,
    int item) {
  CheckTextButtonCellButtonText(l10n_util::GetNSString(expected_button_text_id),
                                section, item);
}

void LegacyChromeTableViewControllerTest::DeleteItem(
    int section,
    int item,
    ProceduralBlock completion_block) {
  NSIndexPath* index_path = [NSIndexPath indexPathForItem:item
                                                inSection:section];
  __weak LegacyChromeTableViewController* weak_controller = controller_;
  void (^batch_updates)() = ^{
    LegacyChromeTableViewController* strong_controller = weak_controller;
    if (!strong_controller) {
      return;
    }
    // Delete data in the model.
    TableViewModel* model = strong_controller.tableViewModel;
    NSInteger section_ID =
        [model sectionIdentifierForSectionIndex:index_path.section];
    NSInteger item_type = [model itemTypeForIndexPath:index_path];
    NSUInteger index = [model indexInItemTypeForIndexPath:index_path];
    [model removeItemWithType:item_type
        fromSectionWithIdentifier:section_ID
                          atIndex:index];

    // Delete in the table view.
    [[strong_controller tableView]
        deleteRowsAtIndexPaths:@[ index_path ]
              withRowAnimation:UITableViewRowAnimationNone];
  };

  void (^completion)(BOOL finished) = ^(BOOL finished) {
    if (completion_block) {
      completion_block();
    }
  };
  [[controller_ tableView] performBatchUpdates:batch_updates
                                    completion:completion];
}
