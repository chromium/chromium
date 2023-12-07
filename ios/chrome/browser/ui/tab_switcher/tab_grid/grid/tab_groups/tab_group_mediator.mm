// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_mediator.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_consumer.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@implementation TabGroupMediator {
  // Web state list which contains groups.
  WebStateList* _webStateList;
  // Tab group consumer.
  __weak id<TabGroupConsumer> _consumer;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            consumer:(id<TabGroupConsumer>)consumer {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group mediator outside the "
         "Tab Groups experiment.";
  CHECK(webStateList);
  CHECK(consumer);
  if (self = [super init]) {
    _webStateList = webStateList;
    _consumer = consumer;
    // TODO(crbug.com/1501837): Replace temporary values by calling model layer
    // to get the following informations.
    [_consumer setGroupTitle:@"Temporary title"];
    [_consumer setGroupColor:[UIColor colorNamed:kYellow500Color]];
    [_consumer setGroupDateCreation:base::Time::Now()];
  }
  return self;
}

@end
