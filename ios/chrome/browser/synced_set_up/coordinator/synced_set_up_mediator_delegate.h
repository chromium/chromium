// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class SyncedSetUpMediator;

// Delegate for the `SyncedSetUpMediator`.
@protocol SyncedSetUpMediatorDelegate <NSObject>

// Called when the Synced Set Up UI is shown to the user.
- (void)syncedSetUpMediatorDidShow:(SyncedSetUpMediator*)mediator;

// Called when the `SyncedSetUpMediator` is used during the post-FRE flow.
- (void)syncedSetUpMediatorWillStartPostFirstRunFlow:
    (SyncedSetUpMediator*)mediator;

// Called when the `SyncedSetUpMediator` is used from a URL page.
- (void)syncedSetUpMediatorWillStartFromURLPage:(SyncedSetUpMediator*)mediator;

// Called when the `SyncedSetUpMediator` is finished.
- (void)syncedSetUpMediatorDidComplete:(SyncedSetUpMediator*)mediator;

@end

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_DELEGATE_H_
