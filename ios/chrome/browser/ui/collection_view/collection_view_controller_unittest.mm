// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#include "ios/chrome/test/block_cleanup_test.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Checks that key methods are called.
// CollectionViewItem can't easily be mocked via OCMock as one of the methods to
// mock returns a Class type.
@interface MockCollectionViewItem : CollectionViewItem
@property(nonatomic, assign) BOOL configureCellCalled;
@end

@implementation MockCollectionViewItem

@synthesize configureCellCalled = _configureCellCalled;

- (void)configureCell:(MDCCollectionViewCell*)cell {
  self.configureCellCalled = YES;
  [super configureCell:cell];
}

@end

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFoo = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFooBar = kItemTypeEnumZero,
  ItemTypeFooBiz,
};

typedef void (^ReconfigureBlock)(CollectionViewController*, NSArray*);

class CollectionViewControllerTest : public BlockCleanupTest {
 public:
  void TestReconfigureBlock(ReconfigureBlock block) {
    // Setup.
    CollectionViewController* controller = [[CollectionViewController alloc]
        initWithLayout:[[MDCCollectionViewFlowLayout alloc] init]
                 style:CollectionViewControllerStyleDefault];
    [controller loadModel];

    CollectionViewModel* model = [controller collectionViewModel];
    [model addSectionWithIdentifier:SectionIdentifierFoo];

    MockCollectionViewItem* firstReconfiguredItem =
        [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBar];
    [model addItem:firstReconfiguredItem
        toSectionWithIdentifier:SectionIdentifierFoo];

    MockCollectionViewItem* secondReconfiguredItem =
        [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBiz];
    [model addItem:secondReconfiguredItem
        toSectionWithIdentifier:SectionIdentifierFoo];

    MockCollectionViewItem* firstNonReconfiguredItem =
        [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBiz];
    [model addItem:firstNonReconfiguredItem
        toSectionWithIdentifier:SectionIdentifierFoo];

    MockCollectionViewItem* thirdReconfiguredItem =
        [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBiz];
    [model addItem:thirdReconfiguredItem
        toSectionWithIdentifier:SectionIdentifierFoo];

    MockCollectionViewItem* secondNonReconfiguredItem =
        [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBiz];
    [model addItem:secondNonReconfiguredItem
        toSectionWithIdentifier:SectionIdentifierFoo];

    // The collection view is not visible on screen, so it has not created any
    // of its cells.  Swizzle |cellsForItemAtIndexPath:| and inject an
    // implementation for testing that always returns a non-nil cell.
    MDCCollectionViewCell* dummyCell = [[MDCCollectionViewCell alloc] init];
    {
      ScopedBlockSwizzler swizzler([UICollectionView class],
                                   @selector(cellForItemAtIndexPath:),
                                   ^(id self) {
                                     return dummyCell;
                                   });

      NSArray* itemsToReconfigure = @[
        firstReconfiguredItem, secondReconfiguredItem, thirdReconfiguredItem
      ];
      // Action.
      block(controller, itemsToReconfigure);
    }

    // Tests.
    EXPECT_TRUE([firstReconfiguredItem configureCellCalled]);
    EXPECT_TRUE([secondReconfiguredItem configureCellCalled]);
    EXPECT_TRUE([thirdReconfiguredItem configureCellCalled]);

    EXPECT_FALSE([firstNonReconfiguredItem configureCellCalled]);
    EXPECT_FALSE([secondNonReconfiguredItem configureCellCalled]);
  }
};

}  // namespace

TEST_F(CollectionViewControllerTest, InitDefaultStyle) {
  CollectionViewController* controller = [[CollectionViewController alloc]
      initWithLayout:[[MDCCollectionViewFlowLayout alloc] init]
               style:CollectionViewControllerStyleDefault];
  EXPECT_EQ(nil, controller.appBarViewController);
}

TEST_F(CollectionViewControllerTest, InitAppBarStyle) {
  CollectionViewController* controller = [[CollectionViewController alloc]
      initWithLayout:[[MDCCollectionViewFlowLayout alloc] init]
               style:CollectionViewControllerStyleAppBar];
  EXPECT_NE(nil, controller.appBarViewController);
}

TEST_F(CollectionViewControllerTest, CellForItemAtIndexPath) {
  CollectionViewController* controller = [[CollectionViewController alloc]
      initWithLayout:[[MDCCollectionViewFlowLayout alloc] init]
               style:CollectionViewControllerStyleDefault];
  [controller loadModel];

  [[controller collectionViewModel]
      addSectionWithIdentifier:SectionIdentifierFoo];
  MockCollectionViewItem* someItem =
      [[MockCollectionViewItem alloc] initWithType:ItemTypeFooBar];
  [[controller collectionViewModel] addItem:someItem
                    toSectionWithIdentifier:SectionIdentifierFoo];

  ASSERT_EQ(NO, [someItem configureCellCalled]);
  [controller collectionView:[controller collectionView]
      cellForItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]];
  EXPECT_EQ(YES, [someItem configureCellCalled]);
}

TEST_F(CollectionViewControllerTest, ReconfigureCells) {
  TestReconfigureBlock(
      ^void(CollectionViewController* controller, NSArray* itemsToReconfigure) {
        [controller reconfigureCellsForItems:itemsToReconfigure];
      });
}

TEST_F(CollectionViewControllerTest, ReconfigureCellsWithIndexPath) {
  TestReconfigureBlock(
      ^void(CollectionViewController* controller, NSArray* itemsToReconfigure) {
        // More setup.
        NSMutableArray* indexPaths = [NSMutableArray array];
        for (CollectionViewItem* item : itemsToReconfigure) {
          NSIndexPath* indexPath =
              [controller.collectionViewModel indexPathForItem:item];
          if (indexPath) {
            [indexPaths addObject:indexPath];
          }
        }

        // Action.
        [controller reconfigureCellsAtIndexPaths:indexPaths];
      });
}
