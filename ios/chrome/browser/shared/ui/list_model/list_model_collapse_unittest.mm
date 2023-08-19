// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

#import "ios/chrome/browser/shared/ui/list_model/list_item.h"
#import "testing/platform_test.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierFoo = kSectionIdentifierEnumZero,
  SectionIdentifierBar,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeFooBar = kItemTypeEnumZero,
};

class ListModelCollapseTest : public PlatformTest {
 protected:
  ListModelCollapseTest() {
    // Need to clean up NSUserDefaults before and after each test.
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:nil forKey:kListModelCollapsedKey];

    model = [[ListModel alloc] init];

    [model addSectionWithIdentifier:SectionIdentifierFoo];
    [model setSectionIdentifier:SectionIdentifierFoo collapsedKey:@"FooKey"];
    ListItem* header = [[ListItem alloc] initWithType:ItemTypeFooBar];
    ListItem* item = [[ListItem alloc] initWithType:ItemTypeFooBar];
    [model setHeader:header forSectionWithIdentifier:SectionIdentifierFoo];
    [model addItem:item toSectionWithIdentifier:SectionIdentifierFoo];

    [model addSectionWithIdentifier:SectionIdentifierBar];
    [model setSectionIdentifier:SectionIdentifierBar collapsedKey:@"BarKey"];
    header = [[ListItem alloc] initWithType:ItemTypeFooBar];
    [model setHeader:header forSectionWithIdentifier:SectionIdentifierBar];
    item = [[ListItem alloc] initWithType:ItemTypeFooBar];
    [model addItem:item toSectionWithIdentifier:SectionIdentifierBar];
    item = [[ListItem alloc] initWithType:ItemTypeFooBar];
    [model addItem:item toSectionWithIdentifier:SectionIdentifierBar];
  }

  ~ListModelCollapseTest() override {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:nil forKey:kListModelCollapsedKey];
  }

  ListModel* model;
};

// Tests the default collapsed value is NO.
TEST_F(ListModelCollapseTest, DefaultCollapsedSectionValue) {
  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);
  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierBar]);
}

// Collapses all sections.
TEST_F(ListModelCollapseTest, SetAllCollapsed) {
  [model setSection:SectionIdentifierFoo collapsed:YES];
  [model setSection:SectionIdentifierBar collapsed:YES];

  // SectionIdentifierFoo
  EXPECT_EQ(0, [model numberOfItemsInSection:0]);
  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierFoo]);
  // SectionIdentifierBar
  EXPECT_EQ(0, [model numberOfItemsInSection:1]);
  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierBar]);

  [model setSection:SectionIdentifierFoo collapsed:NO];
  [model setSection:SectionIdentifierBar collapsed:NO];

  // SectionIdentifierFoo
  EXPECT_EQ(1, [model numberOfItemsInSection:0]);
  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);
  // SectionIdentifierBar
  EXPECT_EQ(2, [model numberOfItemsInSection:1]);
  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierBar]);
}

// Collapses just one section at the time.
TEST_F(ListModelCollapseTest, SetSomeCollapsed) {
  [model setSection:SectionIdentifierFoo collapsed:NO];
  [model setSection:SectionIdentifierBar collapsed:YES];

  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);
  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierBar]);

  [model setSection:SectionIdentifierFoo collapsed:YES];
  [model setSection:SectionIdentifierBar collapsed:NO];

  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierFoo]);
  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierBar]);
}

// Removes a collapsed section.
TEST_F(ListModelCollapseTest, RemoveCollapsedSection) {
  [model setSection:SectionIdentifierFoo collapsed:NO];
  [model setSection:SectionIdentifierBar collapsed:YES];

  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);
  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierBar]);

  EXPECT_EQ(2, [model numberOfSections]);
  [model removeSectionWithIdentifier:SectionIdentifierBar];
  EXPECT_EQ(1, [model numberOfSections]);

  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);
}

// Removes a collapsed section, then re-adds it, it should still be collapsed.
TEST_F(ListModelCollapseTest, RemoveReaddCollapsedSection) {
  [model setSection:SectionIdentifierFoo collapsed:NO];
  [model setSection:SectionIdentifierBar collapsed:YES];

  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);
  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierBar]);

  EXPECT_EQ(2, [model numberOfSections]);
  [model removeSectionWithIdentifier:SectionIdentifierBar];
  EXPECT_EQ(1, [model numberOfSections]);

  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);

  [model addSectionWithIdentifier:SectionIdentifierBar];
  // Use the same Key as the previously removed section.
  [model setSectionIdentifier:SectionIdentifierBar collapsedKey:@"BarKey"];
  ListItem* header = [[ListItem alloc] initWithType:ItemTypeFooBar];
  ListItem* item = [[ListItem alloc] initWithType:ItemTypeFooBar];
  [model setHeader:header forSectionWithIdentifier:SectionIdentifierBar];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierBar];

  EXPECT_EQ(2, [model numberOfSections]);
  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierBar]);
  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);
}

// Test Collapsed persistance.
TEST_F(ListModelCollapseTest, PersistCollapsedSections) {
  [model setSection:SectionIdentifierFoo collapsed:NO];
  [model setSection:SectionIdentifierBar collapsed:YES];

  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);
  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierBar]);

  ListModel* anotherModel = [[ListModel alloc] init];

  [anotherModel addSectionWithIdentifier:SectionIdentifierFoo];
  [anotherModel setSectionIdentifier:SectionIdentifierFoo
                        collapsedKey:@"FooKey"];
  ListItem* header = [[ListItem alloc] initWithType:ItemTypeFooBar];
  ListItem* item = [[ListItem alloc] initWithType:ItemTypeFooBar];
  [anotherModel setHeader:header forSectionWithIdentifier:SectionIdentifierFoo];
  [anotherModel addItem:item toSectionWithIdentifier:SectionIdentifierFoo];

  [anotherModel addSectionWithIdentifier:SectionIdentifierBar];
  [anotherModel setSectionIdentifier:SectionIdentifierBar
                        collapsedKey:@"BarKey"];
  header = [[ListItem alloc] initWithType:ItemTypeFooBar];
  item = [[ListItem alloc] initWithType:ItemTypeFooBar];
  [anotherModel setHeader:header forSectionWithIdentifier:SectionIdentifierBar];
  [anotherModel addItem:item toSectionWithIdentifier:SectionIdentifierBar];

  // Since the Keys are the same as the previous model it should have preserved
  // its collapsed values.
  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);
  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierBar]);
}

TEST_F(ListModelCollapseTest, CollapsedSectionMode) {
  model.collapsableMode = ListModelCollapsableModeFirstCell;
  [model setSection:SectionIdentifierFoo collapsed:YES];
  [model setSection:SectionIdentifierBar collapsed:YES];

  // SectionIdentifierFoo
  EXPECT_EQ(1, [model numberOfItemsInSection:0]);
  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierFoo]);
  // SectionIdentifierBar
  EXPECT_EQ(1, [model numberOfItemsInSection:1]);
  EXPECT_TRUE([model sectionIsCollapsed:SectionIdentifierBar]);

  [model setSection:SectionIdentifierFoo collapsed:NO];
  [model setSection:SectionIdentifierBar collapsed:NO];

  // SectionIdentifierFoo
  EXPECT_EQ(1, [model numberOfItemsInSection:0]);
  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierFoo]);
  // SectionIdentifierBar
  EXPECT_EQ(2, [model numberOfItemsInSection:1]);
  EXPECT_FALSE([model sectionIsCollapsed:SectionIdentifierBar]);
}

}  // namespace
