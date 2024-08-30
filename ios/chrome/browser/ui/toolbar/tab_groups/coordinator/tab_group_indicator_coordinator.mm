// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/coordinator/tab_group_indicator_mediator.h"
#import "ios/chrome/browser/ui/toolbar/tab_groups/ui/tab_group_indicator_view.h"

@implementation TabGroupIndicatorCoordinator {
  TabGroupIndicatorMediator* _mediator;
}

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  CHECK(IsTabGroupIndicatorEnabled());
  CHECK(browser);
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  _view = [[TabGroupIndicatorView alloc] init];
  _mediator = [[TabGroupIndicatorMediator alloc]
      initWithConsumer:_view
          webStateList:self.browser->GetWebStateList()];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
}

@end
