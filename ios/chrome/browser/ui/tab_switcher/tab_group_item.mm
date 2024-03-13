// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"

#import "base/strings/sys_string_conversions.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/ui/tab_switcher/group_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation TabGroupItem

- (instancetype)initWithTabGroup:(const TabGroup*)tabGroup {
  CHECK(tabGroup);
  self = [super init];
  if (self) {
    _tabGroup = tabGroup;
  }
  return self;
}

- (NSString*)title {
  return base::SysUTF16ToNSString(_tabGroup->visual_data().title());
}

- (UIColor*)groupColor {
  return ColorForTabGroupColorId(_tabGroup->visual_data().color());
}

- (void)fetchGroupTabInfos:(GroupTabInfosFetchingCompletionBlock)completion {
  // TODO(crbug.com/1501837): Add the groupTabInfo fetching.
  return;
}

#pragma mark - Debugging

- (NSString*)description {
  return [NSString stringWithFormat:@"Group Title: %@", self.title];
}

@end
