// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class TabUtilsTest : public PlatformTest {
 public:
  TabUtilsTest() {
    TestProfileIOS::Builder profile_builder;
    profile_ = std::move(profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<FakeWebStateListDelegate>());
    web_state_list_ = browser_->GetWebStateList();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
};

// Tests that `MoveWebStateWithIdentifierToInsertionParams` correctly moves
// webStates to the desired `insertion_params` with one group.
TEST_F(TabUtilsTest, MoveWebStateWithIdentifierToInsertionParams_oneGroup) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [ 0 b c ] d"));

  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  // Move "A" in the group at the first position.
  auto insertion_params =
      WebStateList::InsertionParams::AtIndex(0).InGroup(group);
  web::WebStateID identifier_a =
      builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  MoveWebStateWithIdentifierToInsertionParams(identifier_a, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ true);
  EXPECT_EQ("| [ 0 a b c ] d", builder.GetWebStateListDescription());

  // Move "A" out of the group at the first position.
  insertion_params = WebStateList::InsertionParams::AtIndex(0);
  MoveWebStateWithIdentifierToInsertionParams(identifier_a, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ true);
  EXPECT_EQ("| a [ 0 b c ] d", builder.GetWebStateListDescription());

  // Move "D" after "A".
  insertion_params = WebStateList::InsertionParams::AtIndex(1);
  web::WebStateID identifier_d =
      builder.GetWebStateForIdentifier('d')->GetUniqueIdentifier();
  MoveWebStateWithIdentifierToInsertionParams(identifier_d, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ true);
  EXPECT_EQ("| a d [ 0 b c ]", builder.GetWebStateListDescription());

  // Move "D" in the group after "B".
  insertion_params = WebStateList::InsertionParams::AtIndex(2).InGroup(group);
  MoveWebStateWithIdentifierToInsertionParams(identifier_d, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ true);
  EXPECT_EQ("| a [ 0 b d c ]", builder.GetWebStateListDescription());

  // Move "D" in the group after "C".
  insertion_params = WebStateList::InsertionParams::AtIndex(3).InGroup(group);
  MoveWebStateWithIdentifierToInsertionParams(identifier_d, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ true);
  EXPECT_EQ("| a [ 0 b c d ]", builder.GetWebStateListDescription());
}

// Tests that `MoveWebStateWithIdentifierToInsertionParams` correctly moves
// webStates to the desired `insertion_params` with multiple groups.
TEST_F(TabUtilsTest,
       MoveWebStateWithIdentifierToInsertionParams_multipleGroups) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(
      builder.BuildWebStateListFromDescription("| a [ 0 b c ] d [ 1 e f g ]"));

  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_2 = builder.GetTabGroupForIdentifier('1');

  // Move "A" in the first group at the first position.
  auto insertion_params =
      WebStateList::InsertionParams::AtIndex(0).InGroup(group_1);
  web::WebStateID identifier_a =
      builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  MoveWebStateWithIdentifierToInsertionParams(identifier_a, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ true);
  EXPECT_EQ("| [ 0 a b c ] d [ 1 e f g ]",
            builder.GetWebStateListDescription());

  // Move "A" in the second group at the first position.
  insertion_params = WebStateList::InsertionParams::AtIndex(3).InGroup(group_2);
  identifier_a = builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  MoveWebStateWithIdentifierToInsertionParams(identifier_a, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ true);
  EXPECT_EQ("| [ 0 b c ] d [ 1 a e f g ]",
            builder.GetWebStateListDescription());

  // Move "A" after "D".
  insertion_params = WebStateList::InsertionParams::AtIndex(3);
  identifier_a = builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  MoveWebStateWithIdentifierToInsertionParams(identifier_a, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ true);
  EXPECT_EQ("| [ 0 b c ] d a [ 1 e f g ]",
            builder.GetWebStateListDescription());
}

// Tests that `MoveWebStateWithIdentifierToInsertionParams` correctly moves
// webStates to the desired `insertion_params` when the item is from another
// collection view.
//
// If from_same_collection is `false and and the sourceWebStateIndex is before
// the first tab group index, increase the `destinationWebStateIndex` by one.
// This is needed because, the destination source is not correclty calculated by
// the collection view.
TEST_F(TabUtilsTest,
       MoveWebStateWithIdentifierToInsertionParams_notSameCollection) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  const int increase_index = 1;

  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a [ 0 b c ] d"));

  const TabGroup* group = builder.GetTabGroupForIdentifier('0');

  // Move "A" in the group at the first position.
  auto insertion_params =
      WebStateList::InsertionParams::AtIndex(0 + increase_index).InGroup(group);
  web::WebStateID identifier_a =
      builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  MoveWebStateWithIdentifierToInsertionParams(identifier_a, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ false);
  EXPECT_EQ("| [ 0 a b c ] d", builder.GetWebStateListDescription());

  // Move "A" out of the group at the first position.
  insertion_params = WebStateList::InsertionParams::AtIndex(0);
  MoveWebStateWithIdentifierToInsertionParams(identifier_a, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ false);
  EXPECT_EQ("| a [ 0 b c ] d", builder.GetWebStateListDescription());

  // Move "D" after "A".
  insertion_params = WebStateList::InsertionParams::AtIndex(1);
  web::WebStateID identifier_d =
      builder.GetWebStateForIdentifier('d')->GetUniqueIdentifier();
  MoveWebStateWithIdentifierToInsertionParams(identifier_d, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ false);
  EXPECT_EQ("| a d [ 0 b c ]", builder.GetWebStateListDescription());

  // Move "D" in the group after "B".
  insertion_params =
      WebStateList::InsertionParams::AtIndex(2 + increase_index).InGroup(group);
  MoveWebStateWithIdentifierToInsertionParams(identifier_d, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ false);
  EXPECT_EQ("| a [ 0 b d c ]", builder.GetWebStateListDescription());

  // Move "D" in the group after "C".
  insertion_params =
      WebStateList::InsertionParams::AtIndex(3 + increase_index).InGroup(group);
  MoveWebStateWithIdentifierToInsertionParams(identifier_d, insertion_params,
                                              web_state_list_,
                                              /*from_same_collection*/ false);
  EXPECT_EQ("| a [ 0 b c d ]", builder.GetWebStateListDescription());
}
