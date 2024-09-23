// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"

#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

using TabGroupTest = PlatformTest;

using tab_groups::TabGroupId;

// Checks that visual data are correctly set up.
TEST_F(TabGroupTest, VisualData) {
  auto visual_data = tab_groups::TabGroupVisualData(
      u"Group", tab_groups::TabGroupColorId::kGrey);
  TabGroup group(TabGroupId::GenerateNew(), visual_data);

  EXPECT_EQ(visual_data, group.visual_data());
}

// Checks that visual data are only updated via the setter on TabGroup.
TEST_F(TabGroupTest, VisualDataUpdate) {
  auto visual_data = tab_groups::TabGroupVisualData(
      u"Group", tab_groups::TabGroupColorId::kGrey);
  TabGroup group(TabGroupId::GenerateNew(), visual_data);

  visual_data.SetTitle(u"Other title");
  EXPECT_NE(visual_data, group.visual_data());

  group.SetVisualData(visual_data);
  EXPECT_EQ(visual_data, group.visual_data());
}

// Checks that the default range is the InvalidRange.
TEST_F(TabGroupTest, DefaultsToInvalidRange) {
  TabGroup group(TabGroupId::GenerateNew(),
                 tab_groups::TabGroupVisualData(
                     u"Group", tab_groups::TabGroupColorId::kGrey));

  EXPECT_EQ(TabGroupRange::InvalidRange(), group.range());
}

// Checks that the range at construction is correctly set up.
TEST_F(TabGroupTest, RangeAtConstruction) {
  TabGroup group(TabGroupId::GenerateNew(),
                 tab_groups::TabGroupVisualData(
                     u"Group", tab_groups::TabGroupColorId::kGrey),
                 TabGroupRange(2, 3));

  EXPECT_EQ(TabGroupRange(2, 3), group.range());
}

// Checks that the range is correctly update via the setter on TabGroup.
TEST_F(TabGroupTest, RangeUpdate) {
  auto range = TabGroupRange(2, 3);
  TabGroup group(TabGroupId::GenerateNew(),
                 tab_groups::TabGroupVisualData(
                     u"Group", tab_groups::TabGroupColorId::kGrey),
                 range);

  range.MoveRight(2);
  range.ExpandLeft();
  EXPECT_NE(range, group.range());

  group.range().MoveRight(2);
  group.range().ExpandLeft();
  EXPECT_EQ(range, group.range());
}

// Checks that the title is correctly set up.
TEST_F(TabGroupTest, GetTitle) {
  auto range = TabGroupRange(2, 3);
  tab_groups::TabGroupVisualData visual_data = tab_groups::TabGroupVisualData(
      u"A Group", tab_groups::TabGroupColorId::kGrey);
  TabGroup group(TabGroupId::GenerateNew(), visual_data, range);
  EXPECT_NSEQ(group.GetTitle(), @"A Group");

  visual_data.SetTitle(u"A New Name");
  EXPECT_NSEQ(group.GetTitle(), @"A Group");

  group.SetVisualData(visual_data);
  EXPECT_NSEQ(group.GetTitle(), @"A New Name");

  visual_data.SetTitle(u"");
  group.SetVisualData(visual_data);
  EXPECT_NSEQ(group.GetTitle(),
              l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER, 3));
}

// Checks that the tab group id is correctly set up.
TEST_F(TabGroupTest, GetTabGroupId) {
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  TabGroup group(tab_group_id,
                 tab_groups::TabGroupVisualData(
                     u"Group", tab_groups::TabGroupColorId::kGrey),
                 TabGroupRange(2, 3));

  EXPECT_EQ(tab_group_id, group.tab_group_id());
}
