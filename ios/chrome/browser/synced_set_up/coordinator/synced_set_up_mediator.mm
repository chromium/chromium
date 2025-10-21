// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"

@implementation SyncedSetUpMediator {
  // Tracker for retrieving cross device preferences.
  raw_ptr<sync_preferences::CrossDevicePrefTracker> _prefTracker;
}

- (instancetype)initWithPrefTracker:
    (sync_preferences::CrossDevicePrefTracker*)tracker {
  if ((self = [super init])) {
    CHECK(tracker);
    _prefTracker = tracker;
  }
  return self;
}

@end
