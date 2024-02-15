// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_H_

#import <memory>

#import "components/tab_groups/tab_group_visual_data.h"

// The metadata of a tab group, how a group should appear when displayed.
//
// This is created and owned by WebStateList. The ownership of tabs to groups is
// managed by WebStateList, which also notifies observers of any grouped tab
// state change, as well as any group state change.
class TabGroup {
 public:
  TabGroup(const tab_groups::TabGroupVisualData& visual_data)
      : visual_data_(visual_data) {}

  TabGroup(const TabGroup&) = delete;
  TabGroup& operator=(const TabGroup&) = delete;

  ~TabGroup() {}

  // The underlying visual data specific to the group.
  const tab_groups::TabGroupVisualData& visual_data() const {
    return visual_data_;
  }

 private:
  tab_groups::TabGroupVisualData visual_data_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_H_
