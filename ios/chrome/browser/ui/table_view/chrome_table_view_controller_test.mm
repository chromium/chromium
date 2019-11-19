// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Add selectors to be tested in the helpers.
@interface TableViewItem (DetailTextAddition)
- (NSString*)text;
- (NSString*)detailText;
@end

ChromeTableViewControllerTest::ChromeTableViewControllerTest() {}

ChromeTableViewControllerTest::~ChromeTableViewControllerTest() {}

void ChromeTableViewControllerTest::TearDown() {
  // Delete the controller before deleting other test variables, such as a
  // profile, to ensure things are cleaned up in the same order as in Chrome.
  controller_ = nil;
  BlockCleanupTest::TearDown();
}

void ChromeTableViewControllerTest::CreateController() {
  DCHECK(!controller_);
  controller_ = InstantiateController();
  // Force the model to be loaded.
  [controller_ loadModel];
  // Force the tableView to be built.
  EXPECT_TRUE([controller_ view]);
}

ChromeTableViewController* ChromeTableViewControllerTest::controller() {
  if (!controller_)
    CreateController();
  return controller_;
}

void ChromeTableViewControllerTest::ResetController() {
  controller_ = nil;
}

void ChromeTableViewControllerTest::CheckController() {
  EXPECT_TRUE([controller_ view]);
  EXPECT_TRUE([controller_ tableView]);
  EXPECT_TRUE([controller_ tableViewModel]);
  EXPECT_EQ(controller_, [controller_ tableView].delegate);
}

int ChromeTableViewControllerTest::NumberOfSections() {
  return [[controller_ tableViewModel] numberOfSections];
}

int ChromeTableViewControllerTest::NumberOfItemsInSection(int section) {
  return [[controller_ tableViewModel] numberOfItemsInSection:section];
}

id ChromeTableViewControllerTest::GetTableViewItem(int section, int item) {
  TableViewModel* model = [controller_ tableViewModel];
  NSIndexPath* index_path = [NSIndexPath indexPathForItem:item
                                                inSection:section];
  TableViewItem* collection_view_item = [model hasItemAtIndexPath:index_path]
                                            ? [model itemAtIndexPath:index_path]
                                            : nil;
  EXPECT_TRUE(collection_view_item);
  return collection_view_item;
}

void ChromeTableViewControllerTest::CheckTitle(NSString* expected_title) {
  EXPECT_NSEQ(expected_title, [controller_ title]);
}

void ChromeTableViewControllerTest::CheckTitleWithId(int expected_title_id) {
  CheckTitle(l10n_util::GetNSString(expected_title_id));
}

void ChromeTableViewControllerTest::CheckSectionHeader(NSString* expected_text,
                                                       int section) {
  TableViewHeaderFooterItem* header =
      [[controller_ tableViewModel] headerForSection:section];
  ASSERT_TRUE([header respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_text, [(id)header text]);
}

void ChromeTableViewControllerTest::CheckSectionHeaderWithId(
    int expected_text_id,
    int section) {
  CheckSectionHeader(l10n_util::GetNSString(expected_text_id), section);
}

void ChromeTableViewControllerTest::CheckSectionFooter(NSString* expected_text,
                                                       int section) {
  TableViewHeaderFooterItem* footer =
      [[controller_ tableViewModel] footerForSection:section];
  ASSERT_TRUE([footer respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_text, [(id)footer text]);
}

void ChromeTableViewControllerTest::CheckSectionFooterWithId(
    int expected_text_id,
    int section) {
  CheckSectionFooter(l10n_util::GetNSString(expected_text_id), section);
}

// TODO(crbug.com/894800): There are some unittests that are using
// CheckTextCellText to check Item with both "text" and "detailText". Change all
// of them to CheckTextCellTextAndDetailText when the migration is finished.
void ChromeTableViewControllerTest::CheckTextCellText(NSString* expected_text,
                                                      int section,
                                                      int item) {
  id cell = GetTableViewItem(section, item);
  ASSERT_TRUE([cell respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_text, [cell text]);
}

void ChromeTableViewControllerTest::CheckTextCellTextWithId(
    int expected_text_id,
    int section,
    int item) {
  CheckTextCellText(l10n_util::GetNSString(expected_text_id), section, item);
}

void ChromeTableViewControllerTest::CheckTextCellTextAndDetailText(
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

void ChromeTableViewControllerTest::CheckDetailItemTextWithIds(
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

void ChromeTableViewControllerTest::CheckSwitchCellStateAndText(
    BOOL expected_state,
    NSString* expected_title,
    int section,
    int item) {
  id switch_item = GetTableViewItem(section, item);
  EXPECT_TRUE([switch_item respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_title, [switch_item text]);
  EXPECT_TRUE([switch_item respondsToSelector:@selector(isOn)]);
  EXPECT_EQ(expected_state, [switch_item isOn]);
}

void ChromeTableViewControllerTest::CheckSwitchCellStateAndTextWithId(
    BOOL expected_state,
    int expected_title_id,
    int section,
    int item) {
  CheckSwitchCellStateAndText(
      expected_state, l10n_util::GetNSString(expected_title_id), section, item);
}

void ChromeTableViewControllerTest::CheckAccessoryType(
    UITableViewCellAccessoryType accessory_type,
    int section,
    int item) {
  id text_item = GetTableViewItem(section, item);
  EXPECT_TRUE([text_item respondsToSelector:@selector(accessoryType)]);
  EXPECT_EQ(accessory_type, [text_item accessoryType]);
}

void ChromeTableViewControllerTest::CheckTextButtonCellButtonText(
    NSString* expected_button_text,
    int section,
    int item) {
  id text_button_item = GetTableViewItem(section, item);
  ASSERT_TRUE([text_button_item respondsToSelector:@selector(buttonText)]);
  EXPECT_NSEQ(expected_button_text, [text_button_item buttonText]);
}

void ChromeTableViewControllerTest::CheckTextButtonCellButtonTextWithId(
    int expected_button_text_id,
    int section,
    int item) {
  CheckTextButtonCellButtonText(l10n_util::GetNSString(expected_button_text_id),
                                section, item);
}

void ChromeTableViewControllerTest::DeleteItem(
    int section,
    int item,
    ProceduralBlock completion_block) {
  NSIndexPath* index_path = [NSIndexPath indexPathForItem:item
                                                inSection:section];
  __weak ChromeTableViewController* weak_controller = controller_;
  void (^batch_updates)() = ^{
    ChromeTableViewController* strong_controller = weak_controller;
    if (!strong_controller)
      return;
    // Delete data in the model.
    TableViewModel* model = strong_controller.tableViewModel;
    NSInteger section_ID =
        [model sectionIdentifierForSection:index_path.section];
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
