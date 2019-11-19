// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_SYNC_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_SYNC_SYNC_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"

@protocol SyncObserverModelBridge<NSObject>
- (void)onSyncStateChanged;
@optional
- (void)onSyncConfigurationCompleted;
@end

// C++ class to monitor profile sync status in Objective-C type.
class SyncObserverBridge : public syncer::SyncServiceObserver {
 public:
  // |service| must outlive the SyncObserverBridge.
  SyncObserverBridge(id<SyncObserverModelBridge> delegate,
                     syncer::SyncService* service);

  ~SyncObserverBridge() override;

  // syncer::SyncServiceObserver implementation:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncConfigurationCompleted(syncer::SyncService* sync) override;

 private:
  __weak id<SyncObserverModelBridge> delegate_ = nil;
  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(SyncObserverBridge);
};

#endif  // IOS_CHROME_BROWSER_SYNC_SYNC_OBSERVER_BRIDGE_H_
