// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_account_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"

#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

CollectionViewControllerTest::CollectionViewControllerTest() {}

CollectionViewControllerTest::~CollectionViewControllerTest() {}

void CollectionViewControllerTest::TearDown() {
  // Delete the controller before deleting other test variables, such as a
  // profile, to ensure things are cleaned up in the same order as in Chrome.
  controller_ = nil;
  BlockCleanupTest::TearDown();
}

void CollectionViewControllerTest::CreateController() {
  DCHECK(!controller_);
  controller_ = InstantiateController();
  // Force the model to be loaded.
  [controller_ loadModel];
  // Force the collectionView to be built.
  EXPECT_TRUE([controller_ view]);
}

CollectionViewController* CollectionViewControllerTest::controller() {
  if (!controller_)
    CreateController();
  return controller_;
}

void CollectionViewControllerTest::ResetController() {
  controller_ = nil;
}

void CollectionViewControllerTest::CheckController() {
  EXPECT_TRUE([controller_ view]);
  EXPECT_TRUE([controller_ collectionView]);
  EXPECT_TRUE([controller_ collectionViewModel]);
  EXPECT_EQ(controller_, [controller_ collectionView].dataSource);
  EXPECT_EQ(controller_, [controller_ collectionView].delegate);
}

int CollectionViewControllerTest::NumberOfSections() {
  return [[controller_ collectionViewModel] numberOfSections];
}

int CollectionViewControllerTest::NumberOfItemsInSection(int section) {
  return [[controller_ collectionViewModel] numberOfItemsInSection:section];
}

id CollectionViewControllerTest::GetCollectionViewItem(int section, int item) {
  CollectionViewModel* model = [controller_ collectionViewModel];
  NSIndexPath* index_path =
      [NSIndexPath indexPathForItem:item inSection:section];
  CollectionViewItem* collection_view_item =
      [model hasItemAtIndexPath:index_path] ? [model itemAtIndexPath:index_path]
                                            : nil;
  EXPECT_TRUE(collection_view_item);
  return collection_view_item;
}

void CollectionViewControllerTest::CheckTitle(NSString* expected_title) {
  EXPECT_NSEQ(expected_title, [controller_ title]);
}

void CollectionViewControllerTest::CheckTitleWithId(int expected_title_id) {
  CheckTitle(l10n_util::GetNSString(expected_title_id));
}

void CollectionViewControllerTest::CheckSectionHeader(NSString* expected_title,
                                                      int section) {
  CollectionViewItem* header =
      [[controller_ collectionViewModel] headerForSection:section];
  ASSERT_TRUE([header respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_title, [(id)header text]);
}

void CollectionViewControllerTest::CheckSectionHeaderWithId(
    int expected_title_id,
    int section) {
  CheckSectionHeader(l10n_util::GetNSString(expected_title_id), section);
}

void CollectionViewControllerTest::CheckSectionFooter(NSString* expected_text,
                                                      int section) {
  ASSERT_EQ(1, NumberOfItemsInSection(section));
  CollectionViewFooterItem* footer_item =
      base::mac::ObjCCastStrict<CollectionViewFooterItem>(
          GetCollectionViewItem(section, 0));
  EXPECT_NSEQ(expected_text, footer_item.text);
}

void CollectionViewControllerTest::CheckSectionFooterWithId(
    int expected_text_id,
    int section) {
  return CheckSectionFooter(l10n_util::GetNSString(expected_text_id), section);
}

void CollectionViewControllerTest::CheckTextCellText(NSString* expected_text,
                                                     int section,
                                                     int item) {
  id cell = GetCollectionViewItem(section, item);
  ASSERT_TRUE([cell respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_text, [cell text]);
}

void CollectionViewControllerTest::CheckTextCellTextWithId(int expected_text_id,
                                                           int section,
                                                           int item) {
  CheckTextCellText(l10n_util::GetNSString(expected_text_id), section, item);
}

void CollectionViewControllerTest::CheckTextCellTitle(NSString* expected_title,
                                                      int section,
                                                      int item) {
  id cell = GetCollectionViewItem(section, item);
  ASSERT_TRUE([cell respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_title, [cell text]);
  ASSERT_TRUE([cell respondsToSelector:@selector(detailText)]);
  EXPECT_FALSE([cell detailText]);
}

void CollectionViewControllerTest::CheckTextCellTitleWithId(
    int expected_title_id,
    int section,
    int item) {
  CheckTextCellTitle(l10n_util::GetNSString(expected_title_id), section, item);
}

void CollectionViewControllerTest::CheckTextCellTitleAndSubtitle(
    NSString* expected_title,
    NSString* expected_subtitle,
    int section,
    int item) {
  id cell = GetCollectionViewItem(section, item);
  ASSERT_TRUE([cell respondsToSelector:@selector(text)]);
  EXPECT_NSEQ(expected_title, [cell text]);
  ASSERT_TRUE([cell respondsToSelector:@selector(detailText)]);
  EXPECT_NSEQ(expected_subtitle, [cell detailText]);
}

void CollectionViewControllerTest::CheckDetailItemTextWithIds(
    int expected_text_id,
    int expected_detail_text_id,
    int section_id,
    int item_id) {
  CheckTextCellTitleAndSubtitle(l10n_util::GetNSString(expected_text_id),
                                l10n_util::GetNSString(expected_detail_text_id),
                                section_id, item_id);
}

void CollectionViewControllerTest::CheckSwitchCellStateAndTitle(
    BOOL expected_state,
    NSString* expected_title,
    int section,
    int item) {
  CollectionViewSwitchItem* cell = GetCollectionViewItem(section, item);
  EXPECT_NSEQ(expected_title, cell.text);
  EXPECT_EQ(expected_state, cell.isOn);
}

void CollectionViewControllerTest::CheckSwitchCellStateAndTitleWithId(
    BOOL expected_state,
    int expected_title_id,
    int section,
    int item) {
  CheckSwitchCellStateAndTitle(
      expected_state, l10n_util::GetNSString(expected_title_id), section, item);
}

void CollectionViewControllerTest::CheckAccessoryType(
    MDCCollectionViewCellAccessoryType accessory_type,
    int section,
    int item) {
  id text_item = GetCollectionViewItem(section, item);
  EXPECT_TRUE([text_item respondsToSelector:@selector(accessoryType)]);
  EXPECT_EQ(accessory_type,
            (MDCCollectionViewCellAccessoryType)[text_item accessoryType]);
}

void CollectionViewControllerTest::DeleteItem(
    int section,
    int item,
    ProceduralBlock completion_block) {
  NSIndexPath* index_path =
      [NSIndexPath indexPathForItem:item inSection:section];
  __weak CollectionViewController* weak_controller = controller_;
  void (^batch_updates)() = ^{
    CollectionViewController* strong_controller = weak_controller;
    if (!strong_controller)
      return;
    // Notify delegate to delete data.
    [strong_controller collectionView:[strong_controller collectionView]
          willDeleteItemsAtIndexPaths:@[ index_path ]];

    // Delete index paths.
    [[strong_controller collectionView]
        deleteItemsAtIndexPaths:@[ index_path ]];
  };

  void (^completion)(BOOL finished) = ^(BOOL finished) {
    // Notify delegate of deletion.
    CollectionViewController* strong_controller = weak_controller;
    if (!strong_controller)
      return;
    [strong_controller collectionView:[strong_controller collectionView]
           didDeleteItemsAtIndexPaths:@[ index_path ]];
    if (completion_block) {
      completion_block();
    }
  };

  [[controller_ collectionView] performBatchUpdates:batch_updates
                                         completion:completion];
}
