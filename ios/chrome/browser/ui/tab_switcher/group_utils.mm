// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/group_utils.h"

#import <ostream>
#import "base/notreached.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

UIColor* ColorForTabGroupColorId(
    tab_groups::TabGroupColorId tab_group_color_id) {
  switch (tab_group_color_id) {
    case tab_groups::TabGroupColorId::kGrey:
      return [UIColor colorNamed:kStaticGrey300Color];
    case tab_groups::TabGroupColorId::kBlue:
      return [UIColor colorNamed:kBlueColor];
    case tab_groups::TabGroupColorId::kRed:
      return [UIColor colorNamed:kRedColor];
    case tab_groups::TabGroupColorId::kYellow:
      return [UIColor colorNamed:kYellow500Color];
    case tab_groups::TabGroupColorId::kGreen:
      return [UIColor colorNamed:kGreenColor];
    case tab_groups::TabGroupColorId::kPink:
      return [UIColor colorNamed:kPink500Color];
    case tab_groups::TabGroupColorId::kPurple:
      return [UIColor colorNamed:kPurple500Color];
    case tab_groups::TabGroupColorId::kCyan:
      return [UIColor colorNamed:kBlueHaloColor];
    case tab_groups::TabGroupColorId::kOrange:
      return [UIColor colorNamed:kOrange500Color];
    case tab_groups::TabGroupColorId::kNumEntries:
      NOTREACHED_NORETURN() << "kNumEntries is not a supported color enum.";
  }
}
