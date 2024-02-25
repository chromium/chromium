// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"

#import "base/check.h"
#import "components/sync/service/sync_service.h"

SyncObserverBridge::SyncObserverBridge(id<SyncObserverModelBridge> delegate,
                                       syncer::SyncService* sync_service)
    : delegate_(delegate) {
  DCHECK(delegate);
  if (sync_service) {
    scoped_observation_.Observe(sync_service);
  }
}

SyncObserverBridge::~SyncObserverBridge() {}

void SyncObserverBridge::OnStateChanged(syncer::SyncService* sync) {
  [delegate_ onSyncStateChanged];
}

void SyncObserverBridge::OnSyncShutdown(syncer::SyncService* sync) {
  scoped_observation_.Reset();
}
