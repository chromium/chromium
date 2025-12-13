// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_TAB_ACTIONS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_TAB_ACTIONS_H_

#import <set>

#import "components/tab_groups/tab_group_visual_data.h"

class WebStateList;

// Ungroups the tabs at the given indices within the WebStateList.
void UngroupTabs(const std::set<int>& indices_to_ungroup,
                 WebStateList* web_state_list);

// Creates a new tab group in the WebStateList based on the provided
// set of indices and visual data.
void CreateTabGroup(const std::set<int>& indices,
                    const tab_groups::TabGroupVisualData& visual_data,
                    WebStateList* web_state_list);

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_TAB_ACTIONS_H_
