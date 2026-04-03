// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_TAB_ACTIONS_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_TAB_ACTIONS_UTILS_H_

#import <set>

#import "components/tab_groups/tab_group_visual_data.h"

class WebStateList;

namespace actor {

// Ungroups the tabs at the given indices within the WebStateList.
void UngroupTabs(const std::set<int>& indices_to_ungroup,
                 WebStateList* web_state_list);

// Creates a new tab group in the WebStateList based on the provided
// set of indices and visual data.
void CreateTabGroup(const std::set<int>& indices,
                    const tab_groups::TabGroupVisualData& visual_data,
                    WebStateList* web_state_list);

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_UTILS_TAB_ACTIONS_UTILS_H_
