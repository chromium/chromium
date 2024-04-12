// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

TabGroup::TabGroup(const tab_groups::TabGroupVisualData& visual_data,
                   TabGroupRange range)
    : visual_data_(visual_data), range_(range) {}

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

UIColor* TabGroup::ColorForTabGroupColorId(
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
