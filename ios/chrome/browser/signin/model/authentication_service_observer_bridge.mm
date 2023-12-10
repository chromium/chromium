// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"

AuthenticationServiceObserverBridge::AuthenticationServiceObserverBridge(
    AuthenticationService* service,
    id<AuthenticationServiceObserving> observer)
    : observer_(observer) {
  DCHECK(observer_);
  scoped_observation_.Observe(service);
}

AuthenticationServiceObserverBridge::~AuthenticationServiceObserverBridge() =
    default;

void AuthenticationServiceObserverBridge::OnServiceStatusChanged() {
  if ([observer_ respondsToSelector:@selector(onServiceStatusChanged)])
    [observer_ onServiceStatusChanged];
}
