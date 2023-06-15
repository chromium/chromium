// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_mediator.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation HistorySyncCoordinator {
  // History mediator.
  HistorySyncMediator* _mediator;
  // History view controller.
  HistorySyncViewController* _viewController;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  [super start];
  _viewController = [[HistorySyncViewController alloc] init];
  _mediator = [[HistorySyncMediator alloc] init];
  _mediator.consumer = _viewController;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _viewController ]
                                           animated:animated];
}

- (void)stop {
  [super stop];
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

- (void)dealloc {
  CHECK(_mediator);
}

@end
