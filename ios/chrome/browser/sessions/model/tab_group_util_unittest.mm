// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/tab_group_util.h"

#import "base/strings/sys_string_conversions.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/sessions/model/proto/tab_group.pb.h"
#import "ios/chrome/browser/sessions/model/session_tab_group.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "testing/platform_test.h"

// Test suite for sessions `tab_group_util` methods.
using TabSessionGroupUtil = PlatformTest;

using tab_group_util::ColorForStorage;
using tab_group_util::ColorFromStorage;
using tab_group_util::DeserializedGroup;
using tab_groups::TabGroupId;
using tab_groups::TabGroupVisualData;

// Tests the `FromSerializedValue:` method.
TEST_F(TabSessionGroupUtil, FromSerializedValue) {
  TabGroupId tab_group_id = TabGroupId::GenerateNew();

  ios::proto::TabGroupStorage group_storage;
  ios::proto::RangeIndex& range = *group_storage.mutable_range();
  range.set_start(2);
  range.set_count(3);
  group_storage.set_title("title");
  group_storage.set_color(
      tab_group_util::ColorForStorage(tab_groups::TabGroupColorId::kGrey));
  group_storage.set_collapsed(true);
  tab_group_util::TabGroupIdForStorage(tab_group_id,
                                       *group_storage.mutable_tab_group_id());

  DeserializedGroup group_deserialized =
      tab_group_util::FromSerializedValue(group_storage);
  EXPECT_EQ(group_deserialized.range_start, 2);
  EXPECT_EQ(group_deserialized.range_count, 3);
  EXPECT_EQ(group_deserialized.visual_data.title(), u"title");
  EXPECT_EQ(group_deserialized.visual_data.color(),
            tab_groups::TabGroupColorId::kGrey);
  EXPECT_TRUE(group_deserialized.visual_data.is_collapsed());
  EXPECT_EQ(group_deserialized.tab_group_id, tab_group_id);
}

// Tests the legacy `FromSerializedValue:` method.
TEST_F(TabSessionGroupUtil, FromSerializedValueLegacy) {
  TabGroupId tab_group_id = TabGroupId::GenerateNew();

  SessionTabGroup* session_tab_group = [[SessionTabGroup alloc]
      initWithRangeStart:2
              rangeCount:3
                   title:@"title"
                 colorId:static_cast<NSInteger>(
                             tab_groups::TabGroupColorId::kGrey)
          collapsedState:YES
              tabGroupId:tab_group_id];

  SessionWindowIOS* session_window =
      [[SessionWindowIOS alloc] initWithSessions:@[]
                                       tabGroups:@[ session_tab_group ]
                                   selectedIndex:NSNotFound];

  DeserializedGroup group_deserialized =
      tab_group_util::FromSerializedValue(session_window.tabGroups[0]);
  EXPECT_EQ(group_deserialized.range_start, 2);
  EXPECT_EQ(group_deserialized.range_count, 3);
  EXPECT_EQ(group_deserialized.visual_data.title(), u"title");
  EXPECT_EQ(group_deserialized.visual_data.color(),
            tab_groups::TabGroupColorId::kGrey);
  EXPECT_TRUE(group_deserialized.visual_data.is_collapsed());
  EXPECT_EQ(group_deserialized.tab_group_id, tab_group_id);
}

// Tests the `ColorForStorage:` method.
TEST_F(TabSessionGroupUtil, ColorForStorage) {
  EXPECT_EQ(ColorForStorage(tab_groups::TabGroupColorId::kGrey),
            ios::proto::TabGroupColorId::GREY);
  EXPECT_EQ(ColorForStorage(tab_groups::TabGroupColorId::kOrange),
            ios::proto::TabGroupColorId::ORANGE);
}

// Tests the `ColorFromStorage:` method.
TEST_F(TabSessionGroupUtil, ColorFromStorage) {
  EXPECT_EQ(ColorFromStorage(ios::proto::TabGroupColorId::GREY),
            tab_groups::TabGroupColorId::kGrey);
  EXPECT_EQ(ColorFromStorage(ios::proto::TabGroupColorId::ORANGE),
            tab_groups::TabGroupColorId::kOrange);
}
