// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_MEDIATOR_H_

#import <UIKit/UIKit.h>

@class AuthenticationFlow;
class AuthenticationService;
class ChromeAccountManagerService;
@class HistorySyncMediator;
@protocol HistorySyncConsumer;

namespace signin {
class IdentityManager;
}  // namespace signin

@protocol HistorySyncMediatorDelegate <NSObject>

// Notifies the mediator that the user has been removed
- (void)historySyncMediatorPrimaryAccountCleared:(HistorySyncMediator*)mediator;

@end

// Mediator that handles the sync operations.
@interface HistorySyncMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
      chromeAccountManagerService:
          (ChromeAccountManagerService*)chromeAccountManagerService
                  identityManager:(signin::IdentityManager*)identityManager
                         consumer:(id<HistorySyncConsumer>)consumer
                         delegate:(id<HistorySyncMediatorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_MEDIATOR_H_
