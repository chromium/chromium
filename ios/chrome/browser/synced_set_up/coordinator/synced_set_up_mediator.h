// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
class ChromeAccountManagerService;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace sync_preferences {
class CrossDevicePrefTracker;
}  // namespace sync_preferences

namespace syncer {
class DeviceInfoSyncService;
}  // namespace syncer

@class AppStartupParameters;
class PrefService;
@protocol SyncedSetUpMediatorDelegate;

@protocol SyncedSetUpConsumer;

// Mediator responsible for querying and applying tracked prefs on a synced
// device.
@interface SyncedSetUpMediator : NSObject

// Consumer to display user details (name, avatar) on the Synced Set Up welcome
// screen.
@property(nonatomic, weak) id<SyncedSetUpConsumer> consumer;

// Delegate that receives events from this mediator.
@property(nonatomic, weak) id<SyncedSetUpMediatorDelegate> delegate;

- (instancetype)
      initWithPrefTracker:(sync_preferences::CrossDevicePrefTracker*)tracker
    authenticationService:(AuthenticationService*)authenticationService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
    deviceInfoSyncService:(syncer::DeviceInfoSyncService*)deviceInfoSyncService
       profilePrefService:(PrefService*)profilePrefService
        startupParameters:(AppStartupParameters*)startupParameters
          identityManager:(signin::IdentityManager*)identityManager;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_H_
