// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_H_

#import <Foundation/Foundation.h>

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
class AuthenticationService;
class ChromeAccountManagerService;
class PrefService;
@protocol SnackbarCommands;
@protocol SyncedSetUpMediatorDelegate;
@protocol SyncedSetUpConsumer;
class WebStateList;

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
          identityManager:(signin::IdentityManager*)identityManager
             webStateList:(WebStateList*)webStateList
        startupParameters:(AppStartupParameters*)startupParameters
          snackbarHandler:(id<SnackbarCommands>)snackbarHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects this mediator from its browser services, releases service
// dependencies, and clears its internal state. This mediator should not be used
// after being disconnected.
- (void)disconnect;

// Starts the Synced Set Up flow. Evaluates synced preferences across devices,
// determines the initial setup state, and initiates the setup flow via delegate
// callbacks.
- (void)startSyncedSetUpFlow;

// Main entrypoint for applying prefs using this mediator. Applies a new set of
// preferences to the current device (either applying queued prefs from a remote
// device, or restoring the previous local device prefs during an undo/redo
// flow) then presents the appropriate Synced Set Up snackbar.
- (void)applyPrefs;

// Displays a Synced Set Up snackbar if the current state and page conditions
// allow.
// Returns YES if a snackbar was displayed, or NO otherwise. A snackbar is shown
// when:
// - Prompting the user to apply remote preferences.
// - Prompting the user to undo/redo immediately after Synced Set Up changes
// preferences on the current device.
//
// A snackbar is not shown if:
// - This mediator is idle.
// - The pending preference change would not be visible on the active page (e.g.
// NTP customization on a webpage, or omnibox position on the NTP).
// - The page is obstructed by an external launch intent or modal UI.
- (BOOL)showSnackbarIfNeeded;

@end

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_H_
