// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_H_

#import <Foundation/Foundation.h>

namespace sync_preferences {
class CrossDevicePrefTracker;
}  // namespace sync_preferences

// Mediator responsible for querying and applying tracked prefs on a synced
// device.
@interface SyncedSetUpMediator : NSObject

- (instancetype)initWithPrefTracker:
    (sync_preferences::CrossDevicePrefTracker*)tracker;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SYNCED_SET_UP_COORDINATOR_SYNCED_SET_UP_MEDIATOR_H_
