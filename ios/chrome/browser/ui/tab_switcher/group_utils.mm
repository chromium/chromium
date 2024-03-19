// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/group_utils.h"

#import <ostream>
#import "base/notreached.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

std::vector<tab_groups::TabGroupColorId> AllPossibleTabGroupColors() {
  return {
      tab_groups::TabGroupColorId::kGrey,
      tab_groups::TabGroupColorId::kBlue,
      tab_groups::TabGroupColorId::kRed,
      tab_groups::TabGroupColorId::kYellow,
      tab_groups::TabGroupColorId::kGreen,
      tab_groups::TabGroupColorId::kPink,
      tab_groups::TabGroupColorId::kPurple,
      tab_groups::TabGroupColorId::kCyan,
      tab_groups::TabGroupColorId::kOrange,
  };
}

UIColor* ColorForTabGroupColorId(
    tab_groups::TabGroupColorId tab_group_color_id) {
  switch (tab_group_color_id) {
    case tab_groups::TabGroupColorId::kGrey:
      return [UIColor colorNamed:kGrey600Color];
    case tab_groups::TabGroupColorId::kBlue:
      return [UIColor colorNamed:kBlue600Color];
    case tab_groups::TabGroupColorId::kRed:
      return [UIColor colorNamed:kRed600Color];
    case tab_groups::TabGroupColorId::kYellow:
      return [UIColor colorNamed:kYellow600Color];
    case tab_groups::TabGroupColorId::kGreen:
      return [UIColor colorNamed:kGreen600Color];
    case tab_groups::TabGroupColorId::kPink:
      return [UIColor colorNamed:kPink600Color];
    case tab_groups::TabGroupColorId::kPurple:
      return [UIColor colorNamed:kPurple600Color];
    case tab_groups::TabGroupColorId::kCyan:
      return [UIColor colorNamed:kCyan600Color];
    case tab_groups::TabGroupColorId::kOrange:
      return [UIColor colorNamed:kOrange600Color];
    case tab_groups::TabGroupColorId::kNumEntries:
      NOTREACHED_NORETURN() << "kNumEntries is not a supported color enum.";
  }
}

tab_groups::TabGroupColorId DefaultColorForNewTabGroup(
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
  for (tab_groups::TabGroupColorId color : AllPossibleTabGroupColors()) {
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
