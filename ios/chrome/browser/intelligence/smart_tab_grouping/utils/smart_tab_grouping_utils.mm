// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/smart_tab_grouping/utils/smart_tab_grouping_utils.h"

#import <set>

#import "base/strings/strcat.h"
#import "base/strings/utf_string_conversions.h"
#import "components/optimization_guide/proto/features/ios_smart_tab_grouping.pb.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/intelligence/actuation/tab_actions.h"
#import "ios/chrome/browser/saved_tab_groups/ui/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

void ApplySmartTabGroupResponse(
    const optimization_guide::proto::IosSmartTabGroupingResponse&
        response_proto,
    WebStateList* web_state_list) {
  CHECK(web_state_list);

  WebStateList::ScopedBatchOperation lock =
      web_state_list->StartBatchOperation();

  // Collect indices of all tabs currently in any group to ungroup them to
  // ensure a clean state before applying the new smart group suggestions.
  std::set<int> indices_to_ungroup;
  for (int i = 0; i < web_state_list->count(); ++i) {
    if (web_state_list->GetGroupOfWebStateAt(i)) {
      indices_to_ungroup.insert(i);
    }
  }

  if (!indices_to_ungroup.empty()) {
    UngroupTabs(indices_to_ungroup, web_state_list);
  }

  // Map tab unique IDs to WebState pointers. This is necessary because
  // tab indices in the WebStateList will change as tabs are moved into new
  // groups. The unique ID provides a stable identifier for each tab.
  std::map<int64_t, web::WebState*> id_to_web_state;
  for (int i = 0; i < web_state_list->count(); ++i) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    if (web_state) {
      id_to_web_state[web_state->GetUniqueIdentifier().identifier()] =
          web_state;
    }
  }

  const std::vector<tab_groups::TabGroupColorId> available_colors =
      tab_groups::AllPossibleTabGroupColors();
  const size_t num_group_colors = available_colors.size();
  int group_color_index = 0;
  // Iterate through each tab group suggestion in the response proto.
  // For each suggestion, gather the tab indices, create the group's visual data
  // (title with emoji and label, and colour), and create the tab group.
  for (const auto& group_proto : response_proto.tab_groups()) {
    if (group_proto.tab_ids().empty()) {
      continue;
    }

    std::set<int> indices_in_group;

    for (int64_t tab_id : group_proto.tab_ids()) {
      auto id_iterator = id_to_web_state.find(tab_id);
      if (id_iterator == id_to_web_state.end()) {
        continue;
      }

      web::WebState* web_state = id_iterator->second;

      int current_index = web_state_list->GetIndexOfWebState(web_state);

      if (current_index != WebStateList::kInvalidIndex) {
        // Only include the tab in this new group if it's not already
        // part of a group. This check prevents a tab from being added to
        // multiple groups, as a tab can only belong to one group at a time.
        // This check is done in case the model's response repeated tab IDs in
        // different tab groups.
        if (!web_state_list->GetGroupOfWebStateAt(current_index)) {
          indices_in_group.insert(current_index);
        }
      }
    }

    if (indices_in_group.empty()) {
      continue;
    }

    std::string title_with_emoji =
        group_proto.emoji().empty()
            ? group_proto.label()
            : base::StrCat({group_proto.emoji(), " ", group_proto.label()});
    tab_groups::TabGroupColorId color =
        available_colors[group_color_index % num_group_colors];

    const tab_groups::TabGroupVisualData group_visual_data(
        base::UTF8ToUTF16(title_with_emoji), color);

    CreateTabGroup(indices_in_group, group_visual_data, web_state_list);

    group_color_index++;
  }
}
