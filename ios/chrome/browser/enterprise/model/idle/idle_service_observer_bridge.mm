// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/idle_service_observer_bridge.h"

IdleServiceObserverBridge::IdleServiceObserverBridge(
    enterprise_idle::IdleService* service,
    id<IdleServiceObserving> observer)
    : observer_(observer), service_(service) {
  CHECK(observer_);
  CHECK(service_);
  scoped_observation_.Observe(service);
}

IdleServiceObserverBridge::~IdleServiceObserverBridge() = default;

void IdleServiceObserverBridge::OnIdleTimeoutInForeground() {
  if ([observer_
          respondsToSelector:@selector(idleServiceDidTimeoutInForeground:)]) {
    [observer_ idleServiceDidTimeoutInForeground:service_];
  }
}

void IdleServiceObserverBridge::OnIdleTimeoutOnStartup() {
  if ([observer_
          respondsToSelector:@selector(idleServiceDidTimeoutOnStartup:)]) {
    [observer_ idleServiceDidTimeoutOnStartup:service_];
  }
}

void IdleServiceObserverBridge::OnIdleTimeoutActionsCompleted() {
  if ([observer_
          respondsToSelector:@selector(idleServiceDidCompleteActions:)]) {
    [observer_ idleServiceDidCompleteActions:service_];
  }
}

void IdleServiceObserverBridge::OnApplicationWillEnterBackground() {
  if ([observer_
          respondsToSelector:@selector(idleServiceWillEnterBackground:)]) {
    [observer_ idleServiceWillEnterBackground:service_];
  }
}
