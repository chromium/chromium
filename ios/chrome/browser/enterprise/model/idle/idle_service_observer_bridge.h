// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"

// Objective-C protocol mirroring `IdleService::Observer`.
@protocol IdleServiceObserving <NSObject>
@optional
- (void)idleServiceDidTimeoutInForeground:
    (enterprise_idle::IdleService*)idleService;
- (void)idleServiceDidTimeoutOnStartup:
    (enterprise_idle::IdleService*)idleService;
- (void)idleServiceDidCompleteActions:
    (enterprise_idle::IdleService*)idleService;
- (void)idleServiceWillEnterBackground:
    (enterprise_idle::IdleService*)idleService;
@end

// Simple observer bridge that forwards all events to its delegate observer.
class IdleServiceObserverBridge
    : public enterprise_idle::IdleService::Observer {
 public:
  IdleServiceObserverBridge(enterprise_idle::IdleService* service,
                            id<IdleServiceObserving> observer);
  ~IdleServiceObserverBridge() override;

  // `IdleService::Observer` implementation.
  void OnIdleTimeoutInForeground() override;
  void OnIdleTimeoutOnStartup() override;
  void OnIdleTimeoutActionsCompleted() override;
  void OnApplicationWillEnterBackground() override;

 private:
  __weak id<IdleServiceObserving> observer_ = nil;
  raw_ptr<enterprise_idle::IdleService> service_ = nullptr;
  base::ScopedObservation<enterprise_idle::IdleService,
                          enterprise_idle::IdleService::Observer>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_IDLE_SERVICE_OBSERVER_BRIDGE_H_
