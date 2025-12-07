// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/saved_tab_groups/ui/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

TabGroup::TabGroup(tab_groups::TabGroupId tab_group_id,
                   const tab_groups::TabGroupVisualData& visual_data,
                   TabGroupRange range)
    : tab_group_id_(tab_group_id), visual_data_(visual_data), range_(range) {}

TabGroup::~TabGroup() = default;

NSString* TabGroup::GetTitle() const {
  NSString* visual_data_title = base::SysUTF16ToNSString(visual_data_.title());
  if (visual_data_title.length > 0) {
    return visual_data_title;
  }
  return l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER,
                                       range().count());
}

NSString* TabGroup::GetRawTitle() const {
  return base::SysUTF16ToNSString(visual_data_.title());
}

tab_groups::TabGroupColorId TabGroup::GetColor() const {
  return visual_data_.color();
}

// static
tab_groups::TabGroupColorId TabGroup::DefaultColorForNewTabGroup(
    WebStateList* web_state_list) {
  CHECK(web_state_list);

  std::map<tab_groups::TabGroupColorId, int> color_usage;
  for (const TabGroup* group : web_state_list->GetGroups()) {
    tab_groups::TabGroupColorId color = group->visual_data().color();
    color_usage.try_emplace(color, 0);
    color_usage[color] = color_usage[color] + 1;
  }

  tab_groups::TabGroupColorId default_color =
      tab_groups::TabGroupColorId::kGrey;
  int min_usage_color = INT_MAX;
  for (tab_groups::TabGroupColorId color :
       tab_groups::AllPossibleTabGroupColors()) {
    if (!color_usage.contains(color)) {
      return color;
    }
    if (color_usage[color] < min_usage_color) {
      min_usage_color = color_usage[color];
      default_color = color;
    }
  }
  return default_color;
}

base::WeakPtr<const TabGroup> TabGroup::GetWeakPtr() const {
  return weak_ptr_factory_.GetWeakPtr();
}
