// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_MEDIATOR_H_

#import <UIKit/UIKit.h>

class AuthenticationService;
class ChromeAccountManagerService;
@protocol HistorySyncConsumer;
@class HistorySyncCapabilitiesFetcher;
@class HistorySyncMediator;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

// Delegate for the History Sync mediator.
@protocol HistorySyncMediatorDelegate <NSObject>

// Notifies the mediator that the user has been removed.
- (void)historySyncMediatorPrimaryAccountCleared:(HistorySyncMediator*)mediator;

@end

// Mediator that handles the sync operations.
@interface HistorySyncMediator : NSObject

// Consumer for this mediator.
@property(nonatomic, weak) id<HistorySyncConsumer> consumer;
// Delegate.
@property(nonatomic, weak) id<HistorySyncMediatorDelegate> delegate;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
      chromeAccountManagerService:
          (ChromeAccountManagerService*)chromeAccountManagerService
                  identityManager:(signin::IdentityManager*)identityManager
                      syncService:(syncer::SyncService*)syncService
                    showUserEmail:(BOOL)showUserEmail NS_DESIGNATED_INITIALIZER;

// Disconnects the mediator.
- (void)disconnect;

// Opts in for history sync.
- (void)enableHistorySyncOptin;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_HISTORY_SYNC_HISTORY_SYNC_MEDIATOR_H_
