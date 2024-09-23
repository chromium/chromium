// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_TAB_GROUP_UTIL_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_TAB_GROUP_UTIL_H_

#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/sessions/model/proto/tab_group.pb.h"

@class SessionTabGroup;

// Utility methods to serialize / deserialize tab groups.
namespace tab_group_util {

// Struct that represents a deserialized tab group.
struct DeserializedGroup {
  int range_start = -1;
  int range_count = 0;
  tab_groups::TabGroupVisualData visual_data;
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::CreateEmpty();
};

// Returns the `DeserializedGroup` for the given serialized `group`.
DeserializedGroup FromSerializedValue(ios::proto::TabGroupStorage group);
// Legacy version.
DeserializedGroup FromSerializedValue(SessionTabGroup* group);

// Returns the corresponding serialized `color_id`.
ios::proto::TabGroupColorId ColorForStorage(
    tab_groups::TabGroupColorId color_id);
// Returns the corresponding serialized `tab_group_id`.
void TabGroupIdForStorage(tab_groups::TabGroupId tab_group_id,
                          ios::proto::TabGroupId& storage);

// Returns the corresponding deserialized `color_id`.
tab_groups::TabGroupColorId ColorFromStorage(
    ios::proto::TabGroupColorId color_id);
// Returns the corresponding deserialized `tab_group_id`.
tab_groups::TabGroupId TabGroupIdFromStorage(
    ios::proto::TabGroupId tab_group_id);

}  // namespace tab_group_util

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_TAB_GROUP_UTIL_H_
