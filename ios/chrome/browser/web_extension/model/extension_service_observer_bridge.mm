// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_extension/model/extension_service_observer_bridge.h"

#import "base/check.h"

ExtensionServiceObserverBridge::ExtensionServiceObserverBridge(
    ExtensionService* service,
    id<ExtensionServiceObserving> observer)
    : observer_(observer) {
  DCHECK(observer_);
  if (service) {
    scoped_observation_.Observe(service);
  }
}

ExtensionServiceObserverBridge::~ExtensionServiceObserverBridge() = default;

void ExtensionServiceObserverBridge::OnGPCLoadErrorChanged(
    ExtensionService* service,
    bool has_gpc_load_error) {
  if ([observer_
          respondsToSelector:@selector(
                                 extensionService:didChangeGPCLoadError:)]) {
    [observer_ extensionService:service
          didChangeGPCLoadError:has_gpc_load_error];
  }
}
