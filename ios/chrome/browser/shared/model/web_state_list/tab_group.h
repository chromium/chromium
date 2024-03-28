// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "base/sequence_checker.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

class WebStateList;

// The metadata of a tab group, how a group should appear when displayed.
//
// This is created and owned by WebStateList. The ownership of tabs to groups is
// managed by WebStateList, which also notifies observers of any grouped tab
// state change, as well as any group state change.
class TabGroup {
 public:
  TabGroup(
      const tab_groups::TabGroupVisualData& visual_data,
      const WebStateList::Range& range = WebStateList::Range::InvalidRange())
      : visual_data_(visual_data), range_(range) {}

  TabGroup(const TabGroup&) = delete;
  TabGroup& operator=(const TabGroup&) = delete;

  ~TabGroup() {}

  // Returns the title of the group.
  NSString* GetTitle() const;

  // Returns the color of the group.
  UIColor* GetColor() const;

  // The underlying visual data specific to the group.
  const tab_groups::TabGroupVisualData& visual_data() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return visual_data_;
  }
  void SetVisualData(const tab_groups::TabGroupVisualData& visual_data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    visual_data_ = visual_data;
  }

  // The range of this group within its owning WebStateList.
  const WebStateList::Range& range() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return range_;
  }
  WebStateList::Range& range() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return range_;
  }

  // Returns all the colors a TabGroup can have.
  static std::vector<tab_groups::TabGroupColorId> AllPossibleTabGroupColors();

  // Returns a UIColor based on a `tab_group_color_id`.
  static UIColor* ColorForTabGroupColorId(
      tab_groups::TabGroupColorId tab_group_color_id);

  // Returns the default color for a new TabGroup in `web_state_list`. This is
  // based on the colors currently used by this web state list (for this
  // window).
  static tab_groups::TabGroupColorId DefaultColorForNewTabGroup(
      WebStateList* web_state_list);

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  tab_groups::TabGroupVisualData visual_data_;
  WebStateList::Range range_;
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_H_
