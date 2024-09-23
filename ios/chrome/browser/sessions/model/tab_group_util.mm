// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/tab_group_util.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/sessions/model/session_tab_group.h"

using tab_groups::TabGroupId;
using tab_groups::TabGroupVisualData;

namespace tab_group_util {

#pragma mark - Public methods

DeserializedGroup FromSerializedValue(ios::proto::TabGroupStorage group) {
  TabGroupVisualData visual_data = tab_groups::TabGroupVisualData(
      base::UTF8ToUTF16(group.title()), group.color(), group.collapsed());
  return DeserializedGroup{
      .range_start = group.range().start(),
      .range_count = group.range().count(),
      .visual_data = visual_data,
      .tab_group_id =
          tab_group_util::TabGroupIdFromStorage(group.tab_group_id())};
}

DeserializedGroup FromSerializedValue(SessionTabGroup* group) {
  TabGroupVisualData visual_data =
      tab_groups::TabGroupVisualData(base::SysNSStringToUTF16(group.title),
                                     group.colorId, group.collapsedState);
  return DeserializedGroup{.range_start = static_cast<int>(group.rangeStart),
                           .range_count = static_cast<int>(group.rangeCount),
                           .visual_data = visual_data,
                           .tab_group_id = group.tabGroupId};
}

ios::proto::TabGroupColorId ColorForStorage(
    tab_groups::TabGroupColorId color_id) {
  switch (color_id) {
    case tab_groups::TabGroupColorId::kGrey:
      return ios::proto::TabGroupColorId::GREY;
    case tab_groups::TabGroupColorId::kBlue:
      return ios::proto::TabGroupColorId::BLUE;
    case tab_groups::TabGroupColorId::kRed:
      return ios::proto::TabGroupColorId::RED;
    case tab_groups::TabGroupColorId::kYellow:
      return ios::proto::TabGroupColorId::YELLOW;
    case tab_groups::TabGroupColorId::kGreen:
      return ios::proto::TabGroupColorId::GREEN;
    case tab_groups::TabGroupColorId::kPink:
      return ios::proto::TabGroupColorId::PINK;
    case tab_groups::TabGroupColorId::kPurple:
      return ios::proto::TabGroupColorId::PURPLE;
    case tab_groups::TabGroupColorId::kCyan:
      return ios::proto::TabGroupColorId::CYAN;
    case tab_groups::TabGroupColorId::kOrange:
      return ios::proto::TabGroupColorId::ORANGE;
    case tab_groups::TabGroupColorId::kNumEntries:
      NOTREACHED() << "kNumEntries is not a supported color enum.";
  }
}

void TabGroupIdForStorage(TabGroupId tab_group_id,
                          ios::proto::TabGroupId& storage) {
  const base::Token token = tab_group_id.token();
  CHECK(!token.is_zero());
  storage.set_high(token.high());
  storage.set_low(token.low());
}

tab_groups::TabGroupColorId ColorFromStorage(
    ios::proto::TabGroupColorId color_id) {
  switch (color_id) {
    case ios::proto::TabGroupColorId::GREY:
      return tab_groups::TabGroupColorId::kGrey;
    case ios::proto::TabGroupColorId::BLUE:
      return tab_groups::TabGroupColorId::kBlue;
    case ios::proto::TabGroupColorId::RED:
      return tab_groups::TabGroupColorId::kRed;
    case ios::proto::TabGroupColorId::YELLOW:
      return tab_groups::TabGroupColorId::kYellow;
    case ios::proto::TabGroupColorId::GREEN:
      return tab_groups::TabGroupColorId::kGreen;
    case ios::proto::TabGroupColorId::PINK:
      return tab_groups::TabGroupColorId::kPink;
    case ios::proto::TabGroupColorId::PURPLE:
      return tab_groups::TabGroupColorId::kPurple;
    case ios::proto::TabGroupColorId::CYAN:
      return tab_groups::TabGroupColorId::kCyan;
    case ios::proto::TabGroupColorId::ORANGE:
      return tab_groups::TabGroupColorId::kOrange;
    default:
      NOTREACHED() << "value is not a supported color enum.";
  }
}

TabGroupId TabGroupIdFromStorage(ios::proto::TabGroupId tab_group_id) {
  return TabGroupId::FromRawToken(
      base::Token(tab_group_id.high(), tab_group_id.low()));
}

}  // namespace tab_group_util
