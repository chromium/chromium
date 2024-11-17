// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"

#import "base/memory/raw_ptr.h"
#import "base/numerics/safe_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "ui/base/device_form_factor.h"

using tab_groups::TabGroupId;
using tab_groups::TabGroupVisualData;

class GridUtilsTest : public PlatformTest {
 public:
  GridUtilsTest() {
    TestProfileIOS::Builder profile_builder;
    profile_ = std::move(profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<FakeWebStateListDelegate>());
    web_state_list_ = browser_->GetWebStateList();
  }

  void AddWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());
  }

  void AddPinnedWebState() {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Pinned());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
};

TEST_F(GridUtilsTest, CreateValidItemsList) {
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();

  NSArray<GridItemIdentifier*>* itemsList = CreateItems(web_state_list_);
  ASSERT_EQ(base::checked_cast<NSUInteger>(web_state_list_->count()),
            [itemsList count]);
  for (NSUInteger i = 0; i < [itemsList count]; i++) {
    EXPECT_EQ(GridItemType::kTab, itemsList[i].type);
    EXPECT_EQ(web_state_list_->GetWebStateAt(i)->GetUniqueIdentifier(),
              itemsList[i].tabSwitcherItem.identifier);
  }
}

TEST_F(GridUtilsTest, CreateValidItemsListWithoutPinnedTabs) {
  if (!IsPinnedTabsEnabled()) {
    return;
  }

  AddPinnedWebState();
  AddWebState();
  AddPinnedWebState();
  AddWebState();
  AddPinnedWebState();

  NSArray<GridItemIdentifier*>* itemsList = CreateItems(web_state_list_);
  ASSERT_EQ(base::checked_cast<NSUInteger>(web_state_list_->count()) -
                web_state_list_->pinned_tabs_count(),
            [itemsList count]);
  NSInteger number_of_pinned_tabs = web_state_list_->pinned_tabs_count();
  for (NSUInteger i = 0; i < [itemsList count]; i++) {
    web::WebState* web_state =
        web_state_list_->GetWebStateAt(i + number_of_pinned_tabs);
    GridItemIdentifier* item = itemsList[i];
    EXPECT_EQ(GridItemType::kTab, item.type);
    EXPECT_EQ(web_state->GetUniqueIdentifier(),
              itemsList[i].tabSwitcherItem.identifier);
  }
}

// Test that `-CreateItems` handles the creation of different item types (groups
// and tabs) when the `web_state_list_` contains groups.
TEST_F(GridUtilsTest, CreateItemsListWithGroup) {
  for (int i = 0; i < 10; i++) {
    AddWebState();
  }
  TabGroupVisualData visual_data_a =
      TabGroupVisualData(u"Group A", tab_groups::TabGroupColorId::kGrey);
  TabGroupVisualData visual_data_b =
      TabGroupVisualData(u"Group B", tab_groups::TabGroupColorId::kRed);

  web_state_list_->CreateGroup({0, 1, 2}, visual_data_a,
                               TabGroupId::GenerateNew());
  web_state_list_->CreateGroup({5, 6}, visual_data_b,
                               TabGroupId::GenerateNew());

  NSArray<GridItemIdentifier*>* itemsList = CreateItems(web_state_list_);

  // The number of items should be equal to 7, 2 groups ({0, 1, 2} and {5, 6})
  // and 5 web states({3,4,7,8,9}).
  ASSERT_EQ(7, (int)[itemsList count]);
  EXPECT_EQ(GridItemType::kGroup, itemsList[0].type);
  EXPECT_EQ(GridItemType::kTab, itemsList[1].type);
  EXPECT_EQ(GridItemType::kTab, itemsList[2].type);
  EXPECT_EQ(GridItemType::kGroup, itemsList[3].type);
  EXPECT_EQ(GridItemType::kTab, itemsList[4].type);
  EXPECT_EQ(GridItemType::kTab, itemsList[5].type);
  EXPECT_EQ(GridItemType::kTab, itemsList[6].type);
}

// Test that `WebStateIndexFromGridDropItemIndex:` returns the correct
// index when there is no group.
TEST_F(GridUtilsTest,
       WebStateIndexFromGridDropItemIndex_noGroup_sameCollection) {
  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a b c d e f"));

  // Move "A" after "B".
  int destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 1,
                                         /*previous_web_state_index*/ 0);
  web_state_list_->MoveWebStateAt(/*from_index*/ 0,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("| b a c d e f", builder.GetWebStateListDescription());

  // Move "A" after "E".
  destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 4,
                                         /*previous_web_state_index*/ 1);
  web_state_list_->MoveWebStateAt(/*from_index*/ 1,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("| b c d e a f", builder.GetWebStateListDescription());

  // Move "D" after "F".
  destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 5,
                                         /*previous_web_state_index*/ 2);
  web_state_list_->MoveWebStateAt(/*from_index*/ 2,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("| b c e a f d", builder.GetWebStateListDescription());
}

// Test that `WebStateIndexFromGridDropItemIndex:` returns the correct
// index when there is a group.
TEST_F(GridUtilsTest, WebStateIndexFromGridDropItemIndex_group_sameCollection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [ 0 b c ] d"));

  // Move "A" after the group.
  int destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 1,
                                         /*previous_web_state_index*/ 0);
  web_state_list_->MoveWebStateAt(/*from_index*/ 0,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("| [ 0 b c ] a d", builder.GetWebStateListDescription());

  // Move "A" before the group.
  destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 0,
                                         /*previous_web_state_index*/ 2);
  web_state_list_->MoveWebStateAt(/*from_index*/ 2,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("| a [ 0 b c ] d", builder.GetWebStateListDescription());

  // Move "D" before the group.
  destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 1,
                                         /*previous_web_state_index*/ 3);
  web_state_list_->MoveWebStateAt(/*from_index*/ 3,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("| a d [ 0 b c ]", builder.GetWebStateListDescription());

  // Move "A" after "D".
  destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 1,
                                         /*previous_web_state_index*/ 0);
  web_state_list_->MoveWebStateAt(/*from_index*/ 0,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("| d a [ 0 b c ]", builder.GetWebStateListDescription());

  // Move "D" after the group.
  destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 2,
                                         /*previous_web_state_index*/ 0);
  web_state_list_->MoveWebStateAt(/*from_index*/ 0,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("| a [ 0 b c ] d", builder.GetWebStateListDescription());
}

// Test that `WebStateIndexFromGridDropItemIndex:` returns the correct
// index when there is a group but the item does not belong to the same
// collection.
TEST_F(GridUtilsTest,
       WebStateIndexFromGridDropItemIndex_group_otherCollection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [ 0 b c ] d e"));

  // Drop an item before "A".
  int destination_index = WebStateIndexFromGridDropItemIndex(
      web_state_list_, /*drop_item_index*/ 0);
  EXPECT_EQ(destination_index, 0);

  // Drop an item before the group.
  destination_index = WebStateIndexFromGridDropItemIndex(web_state_list_,
                                                         /*drop_item_index*/ 1);
  EXPECT_EQ(destination_index, 1);

  // Drop an item after the group.
  destination_index = WebStateIndexFromGridDropItemIndex(web_state_list_,
                                                         /*drop_item_index*/ 2);
  EXPECT_EQ(destination_index, 3);

  // Drop an item after "E".
  destination_index = WebStateIndexFromGridDropItemIndex(web_state_list_,
                                                         /*drop_item_index*/ 4);
  EXPECT_EQ(destination_index, 5);
}

// Test that `WebStateIndexFromGridDropItemIndex:` returns the correct
// index when there is a group and some pinned tabs.
TEST_F(GridUtilsTest,
       WebStateIndexFromGridDropItemIndex_pinnedAndGroup_sameCollection) {
  if (!IsPinnedTabsEnabled()) {
    return;
  }

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  WebStateListBuilderFromDescription builder(web_state_list_);
  // In the grid, the pinned tabs "A" and "B" are not visible. The item index 0
  // corresponds to "C".
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("a b | c [ 0 d e ] f g h"));

  // Move "G" before "C".
  int destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 0,
                                         /*previous_web_state_index*/ 6);
  web_state_list_->MoveWebStateAt(/*from_index*/ 6,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("a b | g c [ 0 d e ] f h", builder.GetWebStateListDescription());

  // Move "F" before the group.
  destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 2,
                                         /*previous_web_state_index*/ 6);
  web_state_list_->MoveWebStateAt(/*from_index*/ 6,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("a b | g c f [ 0 d e ] h", builder.GetWebStateListDescription());

  // Move "G" after the group.
  destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 3,
                                         /*previous_web_state_index*/ 2);
  web_state_list_->MoveWebStateAt(/*from_index*/ 2,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("a b | c f [ 0 d e ] g h", builder.GetWebStateListDescription());

  // Move "F" after "G".
  destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 3,
                                         /*previous_web_state_index*/ 3);
  web_state_list_->MoveWebStateAt(/*from_index*/ 3,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("a b | c [ 0 d e ] g f h", builder.GetWebStateListDescription());

  // Move "C" after "H".
  destination_index =
      WebStateIndexFromGridDropItemIndex(web_state_list_, /*drop_item_index*/ 4,
                                         /*previous_web_state_index*/ 2);
  web_state_list_->MoveWebStateAt(/*from_index*/ 2,
                                  /*to_index*/ destination_index);
  EXPECT_EQ("a b | [ 0 d e ] g f h c", builder.GetWebStateListDescription());
}

// Test that `WebStateIndexAfterGridDropItemIndex:` returns the correct
// index when there is a group.
TEST_F(GridUtilsTest,
       WebStateIndexAfterGridDropItemIndex_group_sameCollection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [ 0 b c ] d"));
  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  // Move "Group" before "A".
  int next_index = WebStateIndexAfterGridDropItemIndex(
      web_state_list_, /*drop_item_index*/ 0,
      /*previous_web_state_index*/ group->range().range_begin());
  web_state_list_->MoveGroup(group,
                             /*before_index*/ next_index);
  EXPECT_EQ("| [ 0 b c ] a d", builder.GetWebStateListDescription());

  // Move "Group" after "A".
  next_index = WebStateIndexAfterGridDropItemIndex(
      web_state_list_, /*drop_item_index*/ 1,
      /*previous_web_state_index*/ group->range().range_begin());
  web_state_list_->MoveGroup(group,
                             /*before_index*/ next_index);
  EXPECT_EQ("| a [ 0 b c ] d", builder.GetWebStateListDescription());

  // Move "Group" after "D".
  next_index = WebStateIndexAfterGridDropItemIndex(
      web_state_list_, /*drop_item_index*/ 2,
      /*previous_web_state_index*/ group->range().range_begin());
  web_state_list_->MoveGroup(group,
                             /*before_index*/ next_index);
  EXPECT_EQ("| a d [ 0 b c ]", builder.GetWebStateListDescription());

  // Move "Group" before "A".
  next_index = WebStateIndexAfterGridDropItemIndex(
      web_state_list_, /*drop_item_index*/ 0,
      /*previous_web_state_index*/ group->range().range_begin());
  web_state_list_->MoveGroup(group,
                             /*before_index*/ next_index);
  EXPECT_EQ("| [ 0 b c ] a d", builder.GetWebStateListDescription());
}

// Test that `WebStateIndexAfterGridDropItemIndex:` returns the correct
// index when there are groups and pinned tabs.
TEST_F(GridUtilsTest,
       WebStateIndexAfterGridDropItemIndex_pinnedAndGroup_sameCollection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c [ 0 d e f ] [ 1 g h ] i j"));
  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  // Move "Group 0" after "Group 1".
  int next_index = WebStateIndexAfterGridDropItemIndex(
      web_state_list_, /*drop_item_index*/ 2,
      /*previous_web_state_index*/ group_0->range().range_begin());
  web_state_list_->MoveGroup(group_0,
                             /*before_index*/ next_index);
  EXPECT_EQ("a b | c [ 1 g h ] [ 0 d e f ] i j",
            builder.GetWebStateListDescription());

  // Move "Group 0" before "Group 1".
  next_index = WebStateIndexAfterGridDropItemIndex(
      web_state_list_, /*drop_item_index*/ 1,
      /*previous_web_state_index*/ group_0->range().range_begin());
  web_state_list_->MoveGroup(group_0,
                             /*before_index*/ next_index);
  EXPECT_EQ("a b | c [ 0 d e f ] [ 1 g h ] i j",
            builder.GetWebStateListDescription());

  // Move "Group 1" before "C".
  next_index = WebStateIndexAfterGridDropItemIndex(
      web_state_list_, /*drop_item_index*/ 0,
      /*previous_web_state_index*/ group_1->range().range_begin());
  web_state_list_->MoveGroup(group_1,
                             /*before_index*/ next_index);
  EXPECT_EQ("a b | [ 1 g h ] c [ 0 d e f ] i j",
            builder.GetWebStateListDescription());

  // Move "Group 1" after "J".
  next_index = WebStateIndexAfterGridDropItemIndex(
      web_state_list_, /*drop_item_index*/ 4,
      /*previous_web_state_index*/ group_1->range().range_begin());
  web_state_list_->MoveGroup(group_1,
                             /*before_index*/ next_index);
  EXPECT_EQ("a b | c [ 0 d e f ] i j [ 1 g h ]",
            builder.GetWebStateListDescription());
}
