// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_iph_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PriceNotificationsIPHCoordinator

#pragma mark - PriceNotificationsIPHPresenter

- (void)presentPriceNotificationsWhileBrowsingIPH {
  id<PriceNotificationsCommands> priceNotificationCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         PriceNotificationsCommands);
  [priceNotificationCommandsHandler presentPriceNotificationsWhileBrowsingIPH];
}

@end
