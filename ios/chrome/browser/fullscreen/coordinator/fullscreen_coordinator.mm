// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_coordinator.h"

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@implementation FullscreenCoordinator {
  FullscreenMediator* _mediator;
}

#pragma mark - public

- (void)start {
  _mediator = [[FullscreenMediator alloc]
      initWithBrowserAgent:FullscreenBrowserAgent::FromBrowser(self.browser)
              webStateList:self.browser->GetWebStateList()];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
}

@end
