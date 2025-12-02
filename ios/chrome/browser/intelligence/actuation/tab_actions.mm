// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actuation/tab_actions.h"

#import "base/strings/strcat.h"
#import "base/strings/utf_string_conversions.h"
#import "components/tab_groups/tab_group_id.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

void UngroupTabs(const std::set<int>& indices_to_ungroup,
                 WebStateList* web_state_list) {
  CHECK(web_state_list);
  web_state_list->RemoveFromGroups(indices_to_ungroup);
}

void CreateTabGroup(const std::set<int>& indices_for_group,
                    const tab_groups::TabGroupVisualData& visual_data,
                    WebStateList* web_state_list) {
  CHECK(web_state_list);

  if (indices_for_group.empty()) {
    return;
  }

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  web_state_list->CreateGroup(indices_for_group, visual_data, group_id);
}
