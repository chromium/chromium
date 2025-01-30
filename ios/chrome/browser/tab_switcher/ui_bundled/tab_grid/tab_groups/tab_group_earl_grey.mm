// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_earl_grey.h"

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_app_interface.h"

@implementation TabGroupEarlGreyImpl

- (void)prepareFakeSavedTabGroups:(NSInteger)numberOfGroups {
  [TabGroupAppInterface prepareFakeSavedTabGroups:numberOfGroups];
}

- (void)removeAtIndex:(unsigned int)index {
  [TabGroupAppInterface removeAtIndex:index];
}

- (void)cleanup {
  [TabGroupAppInterface cleanup];
}

- (int)countOfSavedTabGroups {
  return [TabGroupAppInterface countOfSavedTabGroups];
}

@end
