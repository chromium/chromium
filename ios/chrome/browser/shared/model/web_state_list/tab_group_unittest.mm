// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"

#import "testing/platform_test.h"

using TabGroupTest = PlatformTest;

// Checks that visual data are correctly set up.
TEST_F(TabGroupTest, VisualData) {
  auto visual_data = tab_groups::TabGroupVisualData(
      u"Group", tab_groups::TabGroupColorId::kGrey);
  TabGroup group(visual_data);

  EXPECT_EQ(visual_data, group.visual_data());
}

// Checks that visual data are only updated via the setter on TabGroup.
TEST_F(TabGroupTest, VisualDataUpdate) {
  auto visual_data = tab_groups::TabGroupVisualData(
      u"Group", tab_groups::TabGroupColorId::kGrey);
  TabGroup group(visual_data);

  visual_data.SetTitle(u"Other title");
  EXPECT_NE(visual_data, group.visual_data());

  group.SetVisualData(visual_data);
  EXPECT_EQ(visual_data, group.visual_data());
}

// Checks that the default range is the InvalidRange.
TEST_F(TabGroupTest, DefaultsToInvalidRange) {
  TabGroup group(tab_groups::TabGroupVisualData(
      u"Group", tab_groups::TabGroupColorId::kGrey));

  EXPECT_EQ(WebStateList::Range::InvalidRange(), group.range());
}

// Checks that the range at construction is correctly set up.
TEST_F(TabGroupTest, RangeAtConstruction) {
  TabGroup group(tab_groups::TabGroupVisualData(
                     u"Group", tab_groups::TabGroupColorId::kGrey),
                 WebStateList::Range(2, 3));

  EXPECT_EQ(WebStateList::Range(2, 3), group.range());
}

// Checks that the range is correctly update via the setter on TabGroup.
TEST_F(TabGroupTest, RangeUpdate) {
  auto range = WebStateList::Range(2, 3);
  TabGroup group(tab_groups::TabGroupVisualData(
                     u"Group", tab_groups::TabGroupColorId::kGrey),
                 range);

  range.MoveRight(2);
  range.ExpandLeft();
  EXPECT_NE(range, group.range());

  group.range().MoveRight(2);
  group.range().ExpandLeft();
  EXPECT_EQ(range, group.range());
}
