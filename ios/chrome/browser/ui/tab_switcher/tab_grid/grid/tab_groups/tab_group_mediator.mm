// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_mediator.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation TabGroupMediator {
  // Web state list which contains groups.
  WebStateList* _webStateList;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group mediator outside the "
         "Tab Groups experiment.";
  if (self = [super init]) {
    _webStateList = webStateList;
  }
  return self;
}

@end
