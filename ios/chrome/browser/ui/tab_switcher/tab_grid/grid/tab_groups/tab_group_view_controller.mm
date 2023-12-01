// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation TabGroupViewController

#pragma mark - UIViewController

- (instancetype)init {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group view controller outside "
         "the Tab Groups experiment.";
  return [super init];
}

@end
