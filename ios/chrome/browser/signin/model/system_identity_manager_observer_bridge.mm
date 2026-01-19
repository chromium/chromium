// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/system_identity_manager_observer_bridge.h"

SystemIdentityManagerObserverBridge::SystemIdentityManagerObserverBridge(
    SystemIdentityManager* manager,
    id<SystemIdentityManagerObserving> observer)
    : observer_(observer) {
  DCHECK(observer_);
  scoped_observation_.Observe(manager);
}

SystemIdentityManagerObserverBridge::~SystemIdentityManagerObserverBridge() =
    default;

void SystemIdentityManagerObserverBridge::OnIdentityListChanged() {
  if ([observer_ respondsToSelector:@selector(onIdentityListChanged)]) {
    [observer_ onIdentityListChanged];
  }
}

void SystemIdentityManagerObserverBridge::OnIdentityUpdated(
    id<SystemIdentity> identity) {
  if ([observer_ respondsToSelector:@selector(onIdentityUpdated:)]) {
    [observer_ onIdentityUpdated:identity];
  }
}
