// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

// Category adding convenience method to add ListItem* to the ListModel* with a
// specific type. This helps keep the test methods short and to the point.
@interface ListModel (ListModelTest)
// Adds an item with the given type to the section with the given identifier.
// It is possible to add multiple items with the same type to the same section.
// Sharing types across sections is undefined behavior.
- (void)crTest_addItemWithType:(NSInteger)itemType
       toSectionWithIdentifier:(NSInteger)sectionIdentifier;
@end

@implementation ListModel (ListModelTest)

- (void)crTest_addItemWithType:(NSInteger)itemType
       toSectionWithIdentifier:(NSInteger)sectionIdentifier {
  ListItem* item = [[ListItem alloc] initWithType:itemType];
  [self addItem:item toSectionWithIdentifier:sectionIdentifier];
}

@end

@interface TestListItemSubclass : ListItem
@end
@implementation TestListItemSubclass
@end

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierCheese = kSectionIdentifierEnumZero,
  SectionIdentifierWeasley,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeCheeseHeader = kItemTypeEnumZero,
  ItemTypeCheeseCheddar,
  ItemTypeCheeseGouda,
  ItemTypeCheesePepperJack,
  ItemTypeWeasleyRon,
  ItemTypeWeasleyGinny,
  ItemTypeWeasleyArthur,
  ItemTypeWeasleyFooter,
};

using ListModelTest = PlatformTest;

// Test generic model boxing (check done at compilation time).
TEST_F(ListModelTest, GenericModelBoxing) {
  ListModel<TestListItemSubclass*, ListItem*>* specificModel =
      [[ListModel alloc] init];

  // `generalModel` is a superclass of `specificModel`. So specificModel can be
  // boxed into generalModel, but not the other way around.
  // specificModel = generalModel would not compile.
  [[maybe_unused]] ListModel<ListItem*, ListItem*>* generalModel =
      specificModel;
  generalModel = nil;
}

TEST_F(ListModelTest, EmptyModel) {
  ListModel* model = [[ListModel alloc] init];

  // Check there are no items.
  EXPECT_EQ(NO, [model hasItemAtIndexPath:[NSIndexPath indexPathForItem:0
                                                              inSection:0]]);

  // Check the collection view data sourcing methods.
  EXPECT_EQ(0, [model numberOfSections]);
}

TEST_F(ListModelTest, SingleSection) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseGouda
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];

  // Check there are some items but not more.
  EXPECT_EQ(NO, [model hasItemAtIndexPath:nil]);
  EXPECT_EQ(YES, [model hasItemAtIndexPath:[NSIndexPath indexPathForItem:0
                                                               inSection:0]]);
  EXPECT_EQ(YES, [model hasItemAtIndexPath:[NSIndexPath indexPathForItem:2
                                                               inSection:0]]);
  EXPECT_EQ(NO, [model hasItemAtIndexPath:[NSIndexPath indexPathForItem:3
                                                              inSection:0]]);
  EXPECT_EQ(NO, [model hasItemAtIndexPath:[NSIndexPath indexPathForItem:0
                                                              inSection:1]]);

  // Check the collection view data sourcing methods.
  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(3, [model numberOfItemsInSection:0]);

  // Check the section identifier <-> section correspondance methods.
  EXPECT_EQ(SectionIdentifierCheese,
            [model sectionIdentifierForSectionIndex:0]);
  EXPECT_EQ(0, [model sectionForSectionIdentifier:SectionIdentifierCheese]);

  // Check the item type <-> item correspondance methods.
  EXPECT_EQ(ItemTypeCheeseCheddar,
            [model itemTypeForIndexPath:[NSIndexPath indexPathForItem:0
                                                            inSection:0]]);
  EXPECT_EQ(ItemTypeCheeseGouda,
            [model itemTypeForIndexPath:[NSIndexPath indexPathForItem:1
                                                            inSection:0]]);
  EXPECT_EQ(ItemTypeCheesePepperJack,
            [model itemTypeForIndexPath:[NSIndexPath indexPathForItem:2
                                                            inSection:0]]);
}

TEST_F(ListModelTest, SingleSectionWithMissingItems) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierCheese];
  // "Gouda" is intentionally omitted.
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];

  // Check the item type <-> item correspondance methods.
  EXPECT_EQ(ItemTypeCheeseCheddar,
            [model itemTypeForIndexPath:[NSIndexPath indexPathForItem:0
                                                            inSection:0]]);
  EXPECT_EQ(ItemTypeCheesePepperJack,
            [model itemTypeForIndexPath:[NSIndexPath indexPathForItem:1
                                                            inSection:0]]);
}

TEST_F(ListModelTest, MultipleSections) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  // "Cheddar" and "Gouda" are intentionally omitted.
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];

  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  // "Ron" is intentionally omitted.
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];

  // Check the collection view data sourcing methods.
  EXPECT_EQ(2, [model numberOfSections]);
  EXPECT_EQ(2, [model numberOfItemsInSection:1]);

  // Check the section identifier <-> section correspondance methods.
  EXPECT_EQ(SectionIdentifierCheese,
            [model sectionIdentifierForSectionIndex:0]);
  EXPECT_EQ(0, [model sectionForSectionIdentifier:SectionIdentifierCheese]);
  EXPECT_EQ(SectionIdentifierWeasley,
            [model sectionIdentifierForSectionIndex:1]);
  EXPECT_EQ(1, [model sectionForSectionIdentifier:SectionIdentifierWeasley]);

  // Check the item type <-> item correspondance methods.
  EXPECT_EQ(ItemTypeCheesePepperJack,
            [model itemTypeForIndexPath:[NSIndexPath indexPathForItem:0
                                                            inSection:0]]);
  EXPECT_EQ(ItemTypeWeasleyGinny,
            [model itemTypeForIndexPath:[NSIndexPath indexPathForItem:0
                                                            inSection:1]]);
  EXPECT_EQ(ItemTypeWeasleyArthur,
            [model itemTypeForIndexPath:[NSIndexPath indexPathForItem:1
                                                            inSection:1]]);
}

TEST_F(ListModelTest, GetIndexPathFromModelCoordinates) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];

  // Check the index path retrieval method for a single item.
  NSIndexPath* indexPath =
      [model indexPathForItemType:ItemTypeWeasleyGinny
                sectionIdentifier:SectionIdentifierWeasley];
  EXPECT_EQ(1, indexPath.section);
  EXPECT_EQ(0, indexPath.item);

  // Check the index path retrieval method for the first item.
  indexPath = [model indexPathForItemType:ItemTypeWeasleyGinny
                        sectionIdentifier:SectionIdentifierWeasley
                                  atIndex:0];
  EXPECT_EQ(1, indexPath.section);
  EXPECT_EQ(0, indexPath.item);
}

TEST_F(ListModelTest, RepeatedItems) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];

  NSIndexPath* indexPath =
      [model indexPathForItemType:ItemTypeWeasleyArthur
                sectionIdentifier:SectionIdentifierWeasley];

  // Check the index path retrieval method for a single item on a repeated item.
  EXPECT_EQ(1, indexPath.section);
  EXPECT_EQ(1, indexPath.item);

  // Check the index path retrieval method for a repeated item.
  indexPath = [model indexPathForItemType:ItemTypeWeasleyArthur
                        sectionIdentifier:SectionIdentifierWeasley
                                  atIndex:1];

  EXPECT_EQ(1, indexPath.section);
  EXPECT_EQ(2, indexPath.item);
}

TEST_F(ListModelTest, RepeatedItemIndex) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];

  // Check the index path <-> index in item type correspondance method.
  EXPECT_EQ(
      0U, [model indexInItemTypeForIndexPath:[NSIndexPath indexPathForItem:0
                                                                 inSection:0]]);
  EXPECT_EQ(
      0U, [model indexInItemTypeForIndexPath:[NSIndexPath indexPathForItem:1
                                                                 inSection:1]]);
  EXPECT_EQ(
      2U, [model indexInItemTypeForIndexPath:[NSIndexPath indexPathForItem:3
                                                                 inSection:1]]);
  EXPECT_EQ(
      3U, [model indexInItemTypeForIndexPath:[NSIndexPath indexPathForItem:5
                                                                 inSection:1]]);
}

TEST_F(ListModelTest, RetrieveAddedItem) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  ListItem* someItem = [[ListItem alloc] initWithType:ItemTypeCheeseGouda];
  [model addItem:someItem toSectionWithIdentifier:SectionIdentifierCheese];

  // Check that the item is the same in the model.
  EXPECT_EQ(someItem, [model itemAtIndexPath:[NSIndexPath indexPathForItem:0
                                                                 inSection:0]]);
}

TEST_F(ListModelTest, RetrieveItemsInSection) {
  ListModel* model = [[ListModel alloc] init];
  [model addSectionWithIdentifier:SectionIdentifierCheese];
  ListItem* cheddar = [[ListItem alloc] initWithType:ItemTypeCheeseCheddar];
  [model addItem:cheddar toSectionWithIdentifier:SectionIdentifierCheese];
  ListItem* pepperJack =
      [[ListItem alloc] initWithType:ItemTypeCheesePepperJack];
  [model addItem:pepperJack toSectionWithIdentifier:SectionIdentifierCheese];
  ListItem* gouda = [[ListItem alloc] initWithType:ItemTypeCheeseGouda];
  [model addItem:gouda toSectionWithIdentifier:SectionIdentifierCheese];

  NSArray* cheeseItems =
      [model itemsInSectionWithIdentifier:SectionIdentifierCheese];
  EXPECT_EQ(3U, [cheeseItems count]);
  EXPECT_NSEQ(cheddar, cheeseItems[0]);
  EXPECT_NSEQ(pepperJack, cheeseItems[1]);
  EXPECT_NSEQ(gouda, cheeseItems[2]);
}

TEST_F(ListModelTest, RemoveItems) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseGouda
        toSectionWithIdentifier:SectionIdentifierCheese];

  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyRon
        toSectionWithIdentifier:SectionIdentifierWeasley];

  [model removeItemWithType:ItemTypeCheesePepperJack
      fromSectionWithIdentifier:SectionIdentifierCheese];
  [model removeItemWithType:ItemTypeWeasleyGinny
      fromSectionWithIdentifier:SectionIdentifierWeasley];
  [model removeItemWithType:ItemTypeWeasleyArthur
      fromSectionWithIdentifier:SectionIdentifierWeasley
                        atIndex:2];

  // Check the collection view data sourcing methods.
  EXPECT_EQ(2, [model numberOfSections]);

  // Check the index path retrieval method for a single item.
  NSIndexPath* indexPath = [model indexPathForItemType:ItemTypeCheeseGouda
                                     sectionIdentifier:SectionIdentifierCheese];
  EXPECT_EQ(0, indexPath.section);
  EXPECT_EQ(0, indexPath.item);

  // Check the index path retrieval method for a repeated item.
  indexPath = [model indexPathForItemType:ItemTypeWeasleyArthur
                        sectionIdentifier:SectionIdentifierWeasley
                                  atIndex:1];
  EXPECT_EQ(1, indexPath.section);
  EXPECT_EQ(1, indexPath.item);

  // Check the index path retrieval method for a single item.
  indexPath = [model indexPathForItemType:ItemTypeWeasleyRon
                        sectionIdentifier:SectionIdentifierWeasley];
  EXPECT_EQ(1, indexPath.section);
  EXPECT_EQ(2, indexPath.item);
}

TEST_F(ListModelTest, RemoveAllItems) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseGouda
        toSectionWithIdentifier:SectionIdentifierCheese];

  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];

  [model deleteAllItemsFromSectionWithIdentifier:SectionIdentifierCheese];

  // Check we still have two sections.
  EXPECT_EQ(2, [model numberOfSections]);

  // Check we have no more items in first section.
  EXPECT_EQ(0, [model numberOfItemsInSection:0]);
  EXPECT_EQ(2, [model numberOfItemsInSection:1]);

  // Check the index path retrieval method for a single item.
  NSIndexPath* indexPath =
      [model indexPathForItemType:ItemTypeWeasleyGinny
                sectionIdentifier:SectionIdentifierWeasley];
  EXPECT_EQ(1, indexPath.section);
  EXPECT_EQ(0, indexPath.item);

  [model crTest_addItemWithType:ItemTypeCheeseGouda
        toSectionWithIdentifier:SectionIdentifierCheese];

  // Check we could still add to the section.
  EXPECT_EQ(1, [model numberOfItemsInSection:0]);
  EXPECT_EQ(2, [model numberOfItemsInSection:1]);
}

TEST_F(ListModelTest, RemoveAllItemsFromAnEmptySection) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];

  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];

  // Check we have no more items in first section.
  EXPECT_EQ(0, [model numberOfItemsInSection:0]);
  EXPECT_EQ(2, [model numberOfItemsInSection:1]);

  [model deleteAllItemsFromSectionWithIdentifier:SectionIdentifierCheese];

  // Check we still have two sections.
  EXPECT_EQ(2, [model numberOfSections]);

  // Check we still have no items in first section.
  EXPECT_EQ(0, [model numberOfItemsInSection:0]);
  EXPECT_EQ(2, [model numberOfItemsInSection:1]);
}

TEST_F(ListModelTest, RemoveSections) {
  ListModel* model = [[ListModel alloc] init];

  // Empty section.
  [model addSectionWithIdentifier:SectionIdentifierWeasley];

  // Section with items.
  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseGouda
        toSectionWithIdentifier:SectionIdentifierCheese];

  // Check the collection view data sourcing methods.
  EXPECT_EQ(2, [model numberOfSections]);
  EXPECT_EQ(0, [model numberOfItemsInSection:0]);
  EXPECT_EQ(2, [model numberOfItemsInSection:1]);

  // Remove an empty section.
  [model removeSectionWithIdentifier:SectionIdentifierWeasley];

  // Check that the section was removed.
  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(2, [model numberOfItemsInSection:0]);

  // Remove a section with items.
  [model removeSectionWithIdentifier:SectionIdentifierCheese];

  // Check that the section and its items were removed.
  EXPECT_EQ(0, [model numberOfSections]);
}

TEST_F(ListModelTest, QueryItemsFromModelCoordinates) {
  ListModel* model = [[ListModel alloc] init];

  EXPECT_FALSE([model hasSectionForSectionIdentifier:SectionIdentifierWeasley]);
  EXPECT_FALSE([model hasItemForItemType:ItemTypeCheeseCheddar
                       sectionIdentifier:SectionIdentifierCheese]);
  EXPECT_FALSE([model hasItemForItemType:ItemTypeCheeseGouda
                       sectionIdentifier:SectionIdentifierCheese
                                 atIndex:1]);

  // Section with items.
  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseGouda
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseGouda
        toSectionWithIdentifier:SectionIdentifierCheese];

  EXPECT_TRUE([model hasSectionForSectionIdentifier:SectionIdentifierCheese]);
  EXPECT_FALSE([model hasItemForItemType:ItemTypeCheeseCheddar
                       sectionIdentifier:SectionIdentifierCheese]);
  EXPECT_TRUE([model hasItemForItemType:ItemTypeCheesePepperJack
                      sectionIdentifier:SectionIdentifierCheese]);
  EXPECT_TRUE([model hasItemForItemType:ItemTypeCheeseGouda
                      sectionIdentifier:SectionIdentifierCheese
                                atIndex:1]);
}

// Tests that inserted sections are added at the correct index.
TEST_F(ListModelTest, InsertSections) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  EXPECT_EQ(1, [model numberOfSections]);
  EXPECT_EQ(0, [model sectionForSectionIdentifier:SectionIdentifierWeasley]);

  [model insertSectionWithIdentifier:SectionIdentifierCheese atIndex:0];
  EXPECT_EQ(2, [model numberOfSections]);
  EXPECT_EQ(1, [model sectionForSectionIdentifier:SectionIdentifierWeasley]);
  EXPECT_EQ(0, [model sectionForSectionIdentifier:SectionIdentifierCheese]);

  [model removeSectionWithIdentifier:SectionIdentifierCheese];
  [model insertSectionWithIdentifier:SectionIdentifierCheese atIndex:1];
  EXPECT_EQ(2, [model numberOfSections]);
  EXPECT_EQ(0, [model sectionForSectionIdentifier:SectionIdentifierWeasley]);
  EXPECT_EQ(1, [model sectionForSectionIdentifier:SectionIdentifierCheese]);
}

// Tests that inserted items are added at the correct index.
TEST_F(ListModelTest, InsertItemAtIndex) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseGouda
        toSectionWithIdentifier:SectionIdentifierCheese];
  ListItem* cheddarItem = [[ListItem alloc] initWithType:ItemTypeCheeseCheddar];
  [model insertItem:cheddarItem
      inSectionWithIdentifier:SectionIdentifierCheese
                      atIndex:1];

  EXPECT_EQ(1, [model numberOfSections]);

  NSIndexPath* pepperJackIndexPath =
      [model indexPathForItemType:ItemTypeCheesePepperJack
                sectionIdentifier:SectionIdentifierCheese];
  EXPECT_EQ(0, pepperJackIndexPath.section);
  EXPECT_EQ(0, pepperJackIndexPath.item);

  NSIndexPath* cheddarIndexPath =
      [model indexPathForItemType:ItemTypeCheeseCheddar
                sectionIdentifier:SectionIdentifierCheese];
  EXPECT_EQ(0, cheddarIndexPath.section);
  EXPECT_EQ(1, cheddarIndexPath.item);

  NSIndexPath* goudaIndexPath =
      [model indexPathForItemType:ItemTypeCheeseGouda
                sectionIdentifier:SectionIdentifierCheese];
  EXPECT_EQ(0, goudaIndexPath.section);
  EXPECT_EQ(2, goudaIndexPath.item);
}

// Tests [ListModel indexPathForItem:].
TEST_F(ListModelTest, IndexPathForItem) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  // Added at index 1.
  ListItem* item1 = [[ListItem alloc] initWithType:ItemTypeWeasleyRon];
  [model addItem:item1 toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  // Repeated item added at index 4.
  ListItem* item4 = [[ListItem alloc] initWithType:ItemTypeWeasleyArthur];
  [model addItem:item4 toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  // Item not added.
  ListItem* notAddedItem = [[ListItem alloc] initWithType:ItemTypeCheeseGouda];

  EXPECT_TRUE([model hasItem:item1]);
  NSIndexPath* indexPath1 = [model indexPathForItem:item1];
  EXPECT_EQ(0, indexPath1.section);
  EXPECT_EQ(1, indexPath1.item);

  EXPECT_TRUE([model hasItem:item4]);
  NSIndexPath* indexPath4 = [model indexPathForItem:item4];
  EXPECT_EQ(0, indexPath4.section);
  EXPECT_EQ(4, indexPath4.item);

  EXPECT_FALSE([model hasItem:notAddedItem]);
}

// Tests [ListModel indexPathsForItemType:sectionIdentifier:].
TEST_F(ListModelTest, IndexPathsForItemTypeSectionIdentifier) {
  ListModel* model = [[ListModel alloc] init];

  // 1st section: Cheddar, Cheddar, Ron, Cheddar, Ron.
  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeWeasleyRon
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeWeasleyRon
        toSectionWithIdentifier:SectionIdentifierCheese];

  // 2nd section: Ron, Cheddar, Ron, Ron, Cheddar, Cheddar.
  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyRon
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyRon
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyRon
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierWeasley];

  NSArray<NSIndexPath*>* indexPaths =
      [model indexPathsForItemType:ItemTypeCheeseCheddar
                 sectionIdentifier:SectionIdentifierCheese];
  ASSERT_EQ(3UL, indexPaths.count);
  EXPECT_NSEQ([NSIndexPath indexPathForItem:0 inSection:0], indexPaths[0]);
  EXPECT_NSEQ([NSIndexPath indexPathForItem:1 inSection:0], indexPaths[1]);
  EXPECT_NSEQ([NSIndexPath indexPathForItem:3 inSection:0], indexPaths[2]);

  indexPaths = [model indexPathsForItemType:ItemTypeWeasleyRon
                          sectionIdentifier:SectionIdentifierCheese];
  ASSERT_EQ(2UL, indexPaths.count);
  EXPECT_NSEQ([NSIndexPath indexPathForItem:2 inSection:0], indexPaths[0]);
  EXPECT_NSEQ([NSIndexPath indexPathForItem:4 inSection:0], indexPaths[1]);

  indexPaths = [model indexPathsForItemType:ItemTypeCheeseCheddar
                          sectionIdentifier:SectionIdentifierWeasley];
  EXPECT_NSEQ([NSIndexPath indexPathForItem:1 inSection:1], indexPaths[0]);
  EXPECT_NSEQ([NSIndexPath indexPathForItem:4 inSection:1], indexPaths[1]);
  EXPECT_NSEQ([NSIndexPath indexPathForItem:5 inSection:1], indexPaths[2]);

  indexPaths = [model indexPathsForItemType:ItemTypeWeasleyRon
                          sectionIdentifier:SectionIdentifierWeasley];
  EXPECT_NSEQ([NSIndexPath indexPathForItem:0 inSection:1], indexPaths[0]);
  EXPECT_NSEQ([NSIndexPath indexPathForItem:2 inSection:1], indexPaths[1]);
  EXPECT_NSEQ([NSIndexPath indexPathForItem:3 inSection:1], indexPaths[2]);
}

TEST_F(ListModelTest, Headers) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  ListItem* cheeseHeader = [[ListItem alloc] initWithType:ItemTypeCheeseHeader];
  [model setHeader:cheeseHeader
      forSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseGouda
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyRon
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];

  NSInteger cheeseSection =
      [model sectionForSectionIdentifier:SectionIdentifierCheese];
  NSInteger weasleySection =
      [model sectionForSectionIdentifier:SectionIdentifierWeasley];

  EXPECT_EQ(cheeseHeader,
            [model headerForSectionWithIdentifier:SectionIdentifierCheese]);
  EXPECT_EQ(cheeseHeader, [model headerForSectionIndex:cheeseSection]);

  EXPECT_FALSE([model headerForSectionWithIdentifier:SectionIdentifierWeasley]);
  EXPECT_FALSE([model headerForSectionIndex:weasleySection]);
}

TEST_F(ListModelTest, Footers) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseGouda
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyRon
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  ListItem* weasleyFooter =
      [[ListItem alloc] initWithType:ItemTypeWeasleyFooter];
  [model setFooter:weasleyFooter
      forSectionWithIdentifier:SectionIdentifierWeasley];

  NSInteger cheeseSection =
      [model sectionForSectionIdentifier:SectionIdentifierCheese];
  NSInteger weasleySection =
      [model sectionForSectionIdentifier:SectionIdentifierWeasley];

  EXPECT_FALSE([model footerForSectionWithIdentifier:SectionIdentifierCheese]);
  EXPECT_FALSE([model footerForSectionIndex:cheeseSection]);

  EXPECT_EQ(weasleyFooter,
            [model footerForSectionWithIdentifier:SectionIdentifierWeasley]);
  EXPECT_EQ(weasleyFooter, [model footerForSectionIndex:weasleySection]);
}

// Tests -[ListModel indexPathForItemType:].
TEST_F(ListModelTest, GetItemByItemType) {
  ListModel* model = [[ListModel alloc] init];

  [model addSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheesePepperJack
        toSectionWithIdentifier:SectionIdentifierCheese];
  [model crTest_addItemWithType:ItemTypeCheeseCheddar
        toSectionWithIdentifier:SectionIdentifierCheese];

  [model addSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyRon
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyGinny
        toSectionWithIdentifier:SectionIdentifierWeasley];
  [model crTest_addItemWithType:ItemTypeWeasleyArthur
        toSectionWithIdentifier:SectionIdentifierWeasley];

  // Check that gouda cannot be found.
  EXPECT_EQ(nil, [model indexPathForItemType:ItemTypeCheeseGouda]);
  // Check cheddar can be found.
  NSIndexPath* cheedarIndexPath = [NSIndexPath indexPathForRow:1 inSection:0];
  EXPECT_EQ(cheedarIndexPath,
            [model indexPathForItemType:ItemTypeCheeseCheddar]);
  // Check weasley ginny can be found.
  NSIndexPath* weasleyGinnyIndexPath = [NSIndexPath indexPathForRow:2
                                                          inSection:1];
  EXPECT_EQ(weasleyGinnyIndexPath,
            [model indexPathForItemType:ItemTypeWeasleyGinny]);
  // Check the first weasley arthur is found.
  NSIndexPath* firstWeasleyArthurIndexPath = [NSIndexPath indexPathForRow:1
                                                                inSection:1];
  EXPECT_EQ(firstWeasleyArthurIndexPath,
            [model indexPathForItemType:ItemTypeWeasleyArthur]);
}

}  // namespace
