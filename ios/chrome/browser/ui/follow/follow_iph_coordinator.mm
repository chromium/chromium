// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/follow/follow_iph_coordinator.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FollowIPHCoordinator

#pragma mark - FollowIPHPresenter

- (void)presentFollowWhileBrowsingIPH {
  id<BrowserCommands> browserCommandsHandler =
      static_cast<id<BrowserCommands>>(self.browser->GetCommandDispatcher());
  [browserCommandsHandler showFollowWhileBrowsingIPH];
}

@end
