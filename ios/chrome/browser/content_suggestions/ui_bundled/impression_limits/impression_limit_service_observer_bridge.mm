// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/impression_limits/impression_limit_service_observer_bridge.h"

ImpressionLimitServiceObserverBridge::ImpressionLimitServiceObserverBridge(
    id<ImpressionLimitServiceObserverBridgeDelegate> delegate,
    ImpressionLimitService* service)
    : delegate_(delegate) {
  if (service) {
    scoped_observation_.Observe(service);
  }
}

ImpressionLimitServiceObserverBridge::~ImpressionLimitServiceObserverBridge() =
    default;

void ImpressionLimitServiceObserverBridge::OnUntracked(const GURL& url) {
  if (delegate_) {
    [delegate_ onUrlUntracked:url];
  }
}
