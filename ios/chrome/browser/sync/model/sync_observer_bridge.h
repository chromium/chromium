// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

@protocol SyncObserverModelBridge <NSObject>
- (void)onSyncStateChanged;
@end

// C++ class to monitor profile sync status in Objective-C type.
class SyncObserverBridge : public syncer::SyncServiceObserver {
 public:
  // `service` must outlive the SyncObserverBridge.
  SyncObserverBridge(id<SyncObserverModelBridge> delegate,
                     syncer::SyncService* service);

  SyncObserverBridge(const SyncObserverBridge&) = delete;
  SyncObserverBridge& operator=(const SyncObserverBridge&) = delete;

  ~SyncObserverBridge() override;

  // syncer::SyncServiceObserver implementation:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

 private:
  __weak id<SyncObserverModelBridge> delegate_ = nil;
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_OBSERVER_BRIDGE_H_
