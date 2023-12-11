// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/model/idle/idle_service_observer_bridge.h"

IdleServiceObserverBridge::IdleServiceObserverBridge(
    enterprise_idle::IdleService* service,
    id<IdleServiceObserving> observer)
    : observer_(observer) {
  DCHECK(observer_);
  scoped_observation_.Observe(service);
}

IdleServiceObserverBridge::~IdleServiceObserverBridge() = default;

void IdleServiceObserverBridge::OnIdleTimeoutInForeground() {
  if ([observer_ respondsToSelector:@selector(onIdleTimeoutInForeground)]) {
    [observer_ onIdleTimeoutInForeground];
  }
}

void IdleServiceObserverBridge::OnIdleTimeoutOnStartup() {
  if ([observer_ respondsToSelector:@selector(onIdleTimeoutOnStartup)]) {
    [observer_ onIdleTimeoutOnStartup];
  }
}

void IdleServiceObserverBridge::OnIdleTimeoutActionsCompleted() {
  if ([observer_ respondsToSelector:@selector(onIdleTimeoutActionsCompleted)]) {
    [observer_ onIdleTimeoutActionsCompleted];
  }
}

void IdleServiceObserverBridge::OnApplicationWillEnterBackground() {
  if ([observer_
          respondsToSelector:@selector(onApplicationWillEnterBackground)]) {
    [observer_ onApplicationWillEnterBackground];
  }
}
