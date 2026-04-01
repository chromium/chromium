// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_coordinator.h"

#import "ios/chrome/browser/fullscreen/coordinator/fullscreen_mediator.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/omnibox/model/omnibox_position/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"

@implementation FullscreenCoordinator {
  FullscreenMediator* _mediator;
}

#pragma mark - public

- (void)start {
  _mediator = [[FullscreenMediator alloc]
             initWithBrowserAgent:FullscreenBrowserAgent::FromBrowser(
                                      self.browser)
                     webStateList:self.browser->GetWebStateList()
      omniboxPositionBrowserAgent:OmniboxPositionBrowserAgent::FromBrowser(
                                      self.browser)];

  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:_mediator
                   forProtocol:@protocol(FullscreenCommands)];
}

- (void)stop {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:_mediator];

  [_mediator disconnect];
  _mediator = nil;
}

@end
