// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_sync_earl_grey.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_sync_earl_grey_app_interface.h"

@implementation TabGroupSyncEarlGreyImpl

- (void)prepareFakeSavedTabGroups {
  [TabGroupSyncEarlGreyAppInterface prepareFakeSavedTabGroups];
}

- (void)removeAtIndex:(unsigned int)index {
  [TabGroupSyncEarlGreyAppInterface removeAtIndex:index];
}

- (void)cleanup {
  [TabGroupSyncEarlGreyAppInterface cleanup];
}

- (int)countOfSavedTabGroups {
  return [TabGroupSyncEarlGreyAppInterface countOfSavedTabGroups];
}

@end
