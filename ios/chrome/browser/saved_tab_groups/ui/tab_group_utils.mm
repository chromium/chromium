// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/ui/tab_group_utils.h"

#import <ostream>

#import "base/notreached.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace tab_groups {

std::vector<TabGroupColorId> AllPossibleTabGroupColors() {
  return {
      TabGroupColorId::kGrey,   TabGroupColorId::kBlue,
      TabGroupColorId::kRed,    TabGroupColorId::kYellow,
      TabGroupColorId::kGreen,  TabGroupColorId::kPink,
      TabGroupColorId::kPurple, TabGroupColorId::kCyan,
      TabGroupColorId::kOrange,
  };
}

UIColor* ColorForTabGroupColorId(TabGroupColorId tab_group_color_id) {
  switch (tab_group_color_id) {
    case TabGroupColorId::kGrey:
      return [UIColor colorNamed:kTabGroupGreyColor];
    case TabGroupColorId::kBlue:
      return [UIColor colorNamed:kBlue600Color];
    case TabGroupColorId::kRed:
      return [UIColor colorNamed:kRed600Color];
    case TabGroupColorId::kYellow:
      return [UIColor colorNamed:kYellow600Color];
    case TabGroupColorId::kGreen:
      return [UIColor colorNamed:kTabGroupGreenColor];
    case TabGroupColorId::kPink:
      return [UIColor colorNamed:kTabGroupPinkColor];
    case TabGroupColorId::kPurple:
      return [UIColor colorNamed:kTabGroupPurpleColor];
    case TabGroupColorId::kCyan:
      return [UIColor colorNamed:kTabGroupCyanColor];
    case TabGroupColorId::kOrange:
      return [UIColor colorNamed:kOrange600Color];
    case TabGroupColorId::kNumEntries:
      NOTREACHED() << "kNumEntries is not a supported color enum.";
  }
}

UIColor* ForegroundColorForTabGroupColorId(TabGroupColorId tab_group_color_id) {
  switch (tab_group_color_id) {
    case TabGroupColorId::kGrey:    // Fallthrough
    case TabGroupColorId::kBlue:    // Fallthrough
    case TabGroupColorId::kRed:     // Fallthrough
    case TabGroupColorId::kGreen:   // Fallthrough
    case TabGroupColorId::kPink:    // Fallthrough
    case TabGroupColorId::kPurple:  // Fallthrough
    case TabGroupColorId::kCyan:
      // For those colors, they are using white in light mode and black in dark
      // mode.
      return [UIColor colorNamed:kSolidWhiteColor];
    case TabGroupColorId::kYellow:  // Fallthrough
    case TabGroupColorId::kOrange:
      // Those colors are always using black.
      return UIColor.blackColor;
    case TabGroupColorId::kNumEntries:
      NOTREACHED() << "kNumEntries is not a supported color enum.";
  }
}

}  // namespace tab_groups
