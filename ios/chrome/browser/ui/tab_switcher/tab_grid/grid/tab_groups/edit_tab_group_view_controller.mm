// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/edit_tab_group_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.h"

@implementation EditTabGroupViewController

- (instancetype)init {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  return [super init];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kEditTabGroupIdentifier;
  self.view.backgroundColor = UIColor.redColor;
}

@end
