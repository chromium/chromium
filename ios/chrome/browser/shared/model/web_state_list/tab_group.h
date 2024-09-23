// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

class WebStateList;

// The metadata of a tab group, how a group should appear when displayed.
//
// This is created and owned by WebStateList. The ownership of tabs to groups is
// managed by WebStateList, which also notifies observers of any grouped tab
// state change, as well as any group state change.
class TabGroup {
 public:
  // Parameters:
  //   - tab_group_id: A unique identifier used by TabGroupSync for
  //   synchronization purposes.
  //   - visual_data: Encapsulates visual information for displaying the tab
  //   group (name, color, etc.).
  //   - range: The range of indices defining the position of this group within
  //   the WebStateList (default is invalid, indicating the range hasn't been
  //   set yet).
  //
  // Ownership:
  //   - This object is created and owned by WebStateList. WebStateList manages
  //   the association of tabs to groups, and it notifies observers of any
  //   changes in group state.
  TabGroup(tab_groups::TabGroupId tab_group_id,
           const tab_groups::TabGroupVisualData& visual_data,
           TabGroupRange range = TabGroupRange::InvalidRange());

  TabGroup(const TabGroup&) = delete;
  TabGroup& operator=(const TabGroup&) = delete;

  ~TabGroup();

  // Returns the title of the group from `visual_data_` if it is not empty.
  // Otherwise, returns an alternative non-empty descriptive title.
  NSString* GetTitle() const;

  // Returns the title of the group from `visual_data_`, even if empty.
  NSString* GetRawTitle() const;

  // Returns the color of the group.
  UIColor* GetColor() const;

  // Returns the color for the elements displayed on top of the group color.
  UIColor* GetForegroundColor() const;

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
  const TabGroupRange& range() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return range_;
  }
  TabGroupRange& range() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return range_;
  }

  // The local tab group identifier.
  tab_groups::TabGroupId tab_group_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return tab_group_id_;
  }

  // Returns all the colors a TabGroup can have.
  static std::vector<tab_groups::TabGroupColorId> AllPossibleTabGroupColors();

  // Returns a UIColor based on a `tab_group_color_id`.
  static UIColor* ColorForTabGroupColorId(
      tab_groups::TabGroupColorId tab_group_color_id);

  // Returns a UIColor for the text to be displayed on top a
  // `tab_group_color_id` color.
  static UIColor* ForegroundColorForTabGroupColorId(
      tab_groups::TabGroupColorId tab_group_color_id);

  // Returns the default color for a new TabGroup in `web_state_list`. This is
  // based on the colors currently used by this web state list (for this
  // window).
  static tab_groups::TabGroupColorId DefaultColorForNewTabGroup(
      WebStateList* web_state_list);

  // Returns a weak pointer.
  base::WeakPtr<const TabGroup> GetWeakPtr() const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  tab_groups::TabGroupId tab_group_id_;
  tab_groups::TabGroupVisualData visual_data_;
  TabGroupRange range_;

  base::WeakPtrFactory<const TabGroup> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_WEB_STATE_LIST_TAB_GROUP_H_
