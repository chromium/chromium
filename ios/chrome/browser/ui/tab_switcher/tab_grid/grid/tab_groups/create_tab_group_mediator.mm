// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_mediator.h"

#import "base/check.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation CreateTabGroupMediator

- (instancetype)init {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  return [super init];
}

#pragma mark - TabGroupCreationMutator

- (void)createNewGroupWithTitle:(NSString*)title
                          color:(tab_groups::TabGroupColorId)colorID
                     completion:(void (^)())completion {
  // TODO(crbug.com/1501837): Create the group in the webstate list.
  completion();
}

@end
