// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
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

UIColor* TabGroup::GetColor() const {
  return ColorForTabGroupColorId(visual_data_.color());
}

UIColor* TabGroup::GetForegroundColor() const {
  return ForegroundColorForTabGroupColorId(visual_data_.color());
}

// static
std::vector<tab_groups::TabGroupColorId> TabGroup::AllPossibleTabGroupColors() {
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

// static
UIColor* TabGroup::ColorForTabGroupColorId(
    tab_groups::TabGroupColorId tab_group_color_id) {
  switch (tab_group_color_id) {
    case tab_groups::TabGroupColorId::kGrey:
      return [UIColor colorNamed:kTabGroupGreyColor];
    case tab_groups::TabGroupColorId::kBlue:
      return [UIColor colorNamed:kBlue600Color];
    case tab_groups::TabGroupColorId::kRed:
      return [UIColor colorNamed:kRed600Color];
    case tab_groups::TabGroupColorId::kYellow:
      return [UIColor colorNamed:kYellow600Color];
    case tab_groups::TabGroupColorId::kGreen:
      return [UIColor colorNamed:kTabGroupGreenColor];
    case tab_groups::TabGroupColorId::kPink:
      return [UIColor colorNamed:kTabGroupPinkColor];
    case tab_groups::TabGroupColorId::kPurple:
      return [UIColor colorNamed:kTabGroupPurpleColor];
    case tab_groups::TabGroupColorId::kCyan:
      return [UIColor colorNamed:kTabGroupCyanColor];
    case tab_groups::TabGroupColorId::kOrange:
      return [UIColor colorNamed:kOrange600Color];
    case tab_groups::TabGroupColorId::kNumEntries:
      NOTREACHED() << "kNumEntries is not a supported color enum.";
  }
}

// static
UIColor* TabGroup::ForegroundColorForTabGroupColorId(
    tab_groups::TabGroupColorId tab_group_color_id) {
  switch (tab_group_color_id) {
    case tab_groups::TabGroupColorId::kGrey:    // Fallthrough
    case tab_groups::TabGroupColorId::kBlue:    // Fallthrough
    case tab_groups::TabGroupColorId::kRed:     // Fallthrough
    case tab_groups::TabGroupColorId::kGreen:   // Fallthrough
    case tab_groups::TabGroupColorId::kPink:    // Fallthrough
    case tab_groups::TabGroupColorId::kPurple:  // Fallthrough
    case tab_groups::TabGroupColorId::kCyan:
      // For those colors, they are using white in light mode and black in dark
      // mode.
      return [UIColor colorNamed:kSolidWhiteColor];
    case tab_groups::TabGroupColorId::kYellow:  // Fallthrough
    case tab_groups::TabGroupColorId::kOrange:
      // Those colors are always using black.
      return UIColor.blackColor;
    case tab_groups::TabGroupColorId::kNumEntries:
      NOTREACHED() << "kNumEntries is not a supported color enum.";
  }
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

base::WeakPtr<const TabGroup> TabGroup::GetWeakPtr() const {
  return weak_ptr_factory_.GetWeakPtr();
}
