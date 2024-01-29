// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_mediator.h"

#import "base/check.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_creation_consumer.h"

@implementation CreateTabGroupMediator {
  __weak id<TabGroupCreationConsumer> _consumer;
}

- (instancetype)initWithConsumer:(id<TabGroupCreationConsumer>)consumer {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    CHECK(consumer);
    _consumer = consumer;
    // TODO(crbug.com/1501837): Get the default color from the component.
    [_consumer setDefaultGroupColor:tab_groups::TabGroupColorId::kPink];
  }
  return self;
}

#pragma mark - TabGroupCreationMutator

- (void)createNewGroupWithTitle:(NSString*)title
                          color:(tab_groups::TabGroupColorId)colorID
                     completion:(void (^)())completion {
  // TODO(crbug.com/1501837): Create the group in the webstate list.
  completion();
}

@end
